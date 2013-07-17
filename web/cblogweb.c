#include <sys/param.h>

#include <stdbool.h>
#include <string.h>
#include <sqlite3.h>
#include <syslog.h>

#include "cblog_utils.h"
#include "cblog_common.h"
#include "cblogweb.h"

/*extern int errno;*/
#include <errno.h>

#include <event2/event.h>
#include <event2/http.h>
#include <event2/buffer.h>
#include <event2/http_struct.h>
#include <event2/event_struct.h>

static struct pages {
	const char	*name;
	int			type;
} page[] = {
	{ "/post", CBLOG_POST },
	{ "/tag", CBLOG_TAG },
	{ "/index.rss", CBLOG_ATOM },
	{ "/index.atom", CBLOG_ATOM },
	{ NULL, -1 },
};

struct criteria {
	int  type;
	bool feed;
	const char *timefmt;
	const char *tagname;
	int64_t start;
	int64_t end;
};

int
add_posts_to_hdf(HDF *hdf, sqlite3_stmt *stmt, sqlite3 *sqlite)
{
	char *filename = NULL;
	int icol;
	int nb_post, nb_tags;
	nb_post = 0;
	sqlite3_stmt *stmt2;

	if (sqlite3_prepare_v2(sqlite,
	    "SELECT tag FROM tags, tags_posts, posts WHERE link=?1 AND posts.id=post_id and tag_id=tags.id;",
	    -1, &stmt2, NULL) != SQLITE_OK) {
		syslog(LOG_ERR, "%s", sqlite3_errmsg(sqlite));
		return 0;
	}

	while (sqlite3_step(stmt) == SQLITE_ROW) {
		sqlite3_reset(stmt2);
		for (icol = 0; icol < sqlite3_column_count(stmt); icol++) {
			switch (sqlite3_column_type(stmt, icol)) {
			case SQLITE_TEXT:
				if (strcmp(sqlite3_column_name(stmt, icol), "filename") == 0) {
					filename = (char*)sqlite3_column_text(stmt, icol);
					sqlite3_bind_text(stmt2, 1, filename, -1, SQLITE_STATIC);
				}
				hdf_set_valuef(hdf, "Posts.%i.%s=%s", nb_post, sqlite3_column_name(stmt, icol), sqlite3_column_text(stmt, icol));
				break;
			case SQLITE_INTEGER:
				hdf_set_valuef(hdf, "Posts.%i.%s=%lld", nb_post, sqlite3_column_name(stmt, icol), sqlite3_column_int64(stmt, icol));
				break;
			default:
				break;
			}
		}
		nb_tags = 0;
		while (sqlite3_step(stmt2) == SQLITE_ROW) {
			hdf_set_valuef(hdf, "Posts.%i.tags.%i.name=%s", nb_post, nb_tags, sqlite3_column_text(stmt2, 0));
			nb_tags++;
		}
		hdf_set_valuef(hdf, "Posts.%i.nb_comments=%i", nb_post, get_comments_count(filename, sqlite));
		nb_post++;
	}

	sqlite3_finalize(stmt2);

	return (nb_post);
}

void
set_tags(HDF *hdf, sqlite3 *sqlite)
{
	sqlite3_stmt *stmt;
	int nb_tags;

	if (sqlite3_prepare_v2(sqlite, "select count(*), tag from tags, tags_posts where id=tag_id group by tag order by tag;",
				-1, &stmt, NULL) != SQLITE_OK) {
		syslog(LOG_ERR, "%s", sqlite3_errmsg(sqlite));
		return;
	}

	nb_tags = 0;

	while (sqlite3_step(stmt) == SQLITE_ROW) {
		hdf_set_valuef(hdf, "Tags.%i.name=%s", nb_tags, sqlite3_column_text(stmt, 1));
		hdf_set_valuef(hdf, "Tags.%i.count=%lld", nb_tags, sqlite3_column_int64(stmt, 0));
		nb_tags++;
	}

	sqlite3_finalize(stmt);
}

int
build_post(HDF *hdf, const char *postname, sqlite3 *sqlite)
{
	sqlite3_stmt *stmt;
	int ret=0;

	if (sqlite3_prepare_v2(sqlite,
		"SELECT link as filename, title, source, html, strftime(?2, datetime(date, 'unixepoch')) as date from posts "
		"WHERE link=?1 ;",
		-1, &stmt, NULL) != SQLITE_OK) {
		syslog(LOG_ERR, "%s", sqlite3_errmsg(sqlite));
		return 0;
	}

	sqlite3_bind_text(stmt, 1, postname, -1, SQLITE_STATIC);
	sqlite3_bind_text(stmt, 2, get_dateformat(hdf), -1, SQLITE_STATIC);

	ret = add_posts_to_hdf(hdf, stmt, sqlite);
	sqlite3_finalize(stmt);
	get_comments(hdf, postname, sqlite);

	return ret;
}

int
build_index(HDF *hdf, struct criteria *criteria, sqlite3 *sqlite)
{
	sqlite3_stmt *stmt, *stmtcnt;

	int nb_post = 0, first_post = 0, max_post, total_posts, nb_pages;
	int page = 0;

	const char *baseurl, *counturl;

	max_post = hdf_get_int_value(hdf, "posts_per_pages", DEFAULT_POSTS_PER_PAGES);
	page = hdf_get_int_value(hdf, "Query.page", 1);
	if (page <= 0)
		page = 1;

	switch (criteria->type) {
	case CRITERIA_TAGNAME:
		baseurl = 
	    "SELECT link as filename, title, source, html, strftime(?3, datetime(date, 'unixepoch')) as date, date as timestamp from posts, tags_posts, tags "
		"WHERE posts.id=post_id and tag_id=tags.id and tag=?4 "
		"ORDER BY timestamp DESC LIMIT ?1 OFFSET ?2 ";
		counturl =
	    "SELECT count(*) from posts, tags_posts, tags "
		"WHERE posts.id=post_id and tag_id=tags.id and tag=?1;";
		break;
	case CRITERIA_TIME_T:
		baseurl = "SELECT link as filename, title, source, html, strftime(?3, datetime(date, 'unixepoch')) as date, date as timestamp from posts "
			"WHERE date BETWEEN ?4 and ?5 "
			"ORDER BY timestamp DESC LIMIT ?1 OFFSET ?2;";
		counturl = "SELECT count(*) from posts WHERE date BETWEEN ?1 and ?2 ";
		break;
	default:
		baseurl = "SELECT link as filename, title, source, html, strftime(?3, datetime(date, 'unixepoch')) as date, date as timestamp from posts ORDER BY timestamp DESC LIMIT ?1 OFFSET ?2;";
		counturl = "SELECT count(*) from posts;";
		break;
	}
	if (sqlite3_prepare_v2(sqlite,
		baseurl,
	    -1, &stmt, NULL) != SQLITE_OK) {
		syslog(LOG_ERR, "%s", sqlite3_errmsg(sqlite));
		return 0;
	}

	if (sqlite3_prepare_v2(sqlite, counturl, -1, &stmtcnt, NULL) != SQLITE_OK) {
		syslog(LOG_ERR, "%s", sqlite3_errmsg(sqlite));
		return 0;
	}

	first_post = (page * max_post) - max_post;
	sqlite3_bind_int(stmt, 1, max_post);
	sqlite3_bind_int(stmt, 2, first_post);
	sqlite3_bind_text(stmt, 3, criteria->timefmt, -1, SQLITE_STATIC);
	switch (criteria->type) {
	case CRITERIA_TAGNAME:
		sqlite3_bind_text(stmt, 4, criteria->tagname, -1, SQLITE_STATIC);
		sqlite3_bind_text(stmtcnt, 1, criteria->tagname, -1, SQLITE_STATIC);
		break;
	case CRITERIA_TIME_T:
		sqlite3_bind_int64(stmt, 4, criteria->start);
		sqlite3_bind_int64(stmt, 5, criteria->end);
		sqlite3_bind_int64(stmtcnt, 1, criteria->start);
		sqlite3_bind_int64(stmtcnt, 2, criteria->end);
		break;
	default:
		break;
	}

	sqlite3_step(stmtcnt);
	total_posts = sqlite3_column_int64(stmtcnt, 0);
	sqlite3_finalize(stmtcnt);

	nb_post = add_posts_to_hdf(hdf, stmt, sqlite);
	sqlite3_finalize(stmt);

	if (!criteria->feed)
		set_tags(hdf, sqlite);

	nb_pages = total_posts / max_post;
	if (total_posts % max_post > 0)
		nb_pages++;

	set_nb_pages(hdf, nb_pages);

	return nb_post;
}

static NEOERR *
cblog_output(void *ctx, char *s)
{
	STRING *str = ctx;
	NEOERR *neoerr;

	neoerr = nerr_pass(string_append(str, s));

	return neoerr;
}

static int
parse_post(HDF *hdf, struct evkeyvalq *h)
{
	const char *var;
	int ret = CBLOG_COMMENT_NONE;

	if ((var = evhttp_find_header(h, "submit")) == NULL)
		return (ret);

	if (strcmp(var, "Post") == 0)
		ret = CBLOG_COMMENT_POST;
	else if (strcmp(var, "Preview") == 0)
		ret = CBLOG_COMMENT_PREVIEW;

	if (ret != CBLOG_COMMENT_PREVIEW)
		return (ret);

	hdf_set_valuef(hdf, "Query.submit=Preview");
	if ((var = evhttp_find_header(h, "name")) != NULL)
		hdf_set_valuef(hdf, "Query.name=%s", var);
	if ((var = evhttp_find_header(h, "url")) != NULL)
		hdf_set_valuef(hdf, "Query.url=%s", var);
	if ((var = evhttp_find_header(h, "comment")) != NULL)
		hdf_set_valuef(hdf, "Query.comment=%s", var);

	return (ret);
}

void
cblog(struct evhttp_request* req, void* args)
{
	NEOERR				*neoerr;
	HDF *out;
	STRING str, neoerr_str;
	const char *reqpath, *requesturi, *tplpath;
	const struct evhttp_uri *uri;
	const char *q;
	char cspath[MAXPATHLEN];
	struct evkeyvalq h;
	CSPARSE *parse;
	int method;
	int type, i, nb_posts;
	int yyyy, mm, dd;
	struct criteria criteria;
	char *date = NULL;
	const char *var;
	struct evbuffer *evb = NULL;
	sqlite3 *sqlite;
	int comment = CBLOG_COMMENT_NONE;

	/* read the configuration file */

	type = CBLOG_ROOT;
	criteria.type = 0;
	criteria.feed = false;

	method = evhttp_request_get_command(req);
	uri = evhttp_request_get_evhttp_uri(req);


	hdf_init(&out);
	neoerr = hdf_copy(out, "", args);
	nerr_ignore(&neoerr);

	tplpath = hdf_get_valuef(out, "templates");
	strlcpy(cspath, tplpath, MAXPATHLEN);
	hdf_set_valuef(out, "CBlog.version=%s", cblog_version);
	hdf_set_valuef(out, "CBlog.url=%s", cblog_url);

	requesturi = evhttp_request_get_uri(req);
	reqpath = evhttp_uri_get_path(uri);
	evb = evbuffer_new();
	if ((q = evhttp_uri_get_query(uri)) != NULL)
		evhttp_parse_query_str(q, &h);

	hdf_set_value(out, "CGI.RequestURI", requesturi);

	if (method == EVHTTP_REQ_POST) {
		/* parse the post and set everything into the hdf */
		struct evbuffer *postbuf = evhttp_request_get_input_buffer(req);
		if (evbuffer_get_length(postbuf) > 0) {
			int len = evbuffer_get_length(postbuf);
			char *tmp = malloc(len+1);
			memcpy(tmp, evbuffer_pullup(postbuf, -1), len);
			tmp[len] = '\0';
			evhttp_parse_query_str(tmp, &h);
			free(tmp);
			comment = parse_post(out, &h);
		}
	}

	criteria.timefmt = NULL;
	if (q != NULL) {
		var = evhttp_find_header(&h, "feed");
		if (var != NULL && (
		    EQUALS(var, "rss") ||
		    EQUALS(var, "atom"))) {
			criteria.feed=true;
			criteria.timefmt = "%Y-%m-%dT%H:%M:%SZ";
		}

		if ((var = evhttp_find_header(&h, "source")) != NULL)
			hdf_set_valuef(out, "Query.source=%s", var);

		if ((var = evhttp_find_header(&h, "page")) != NULL)
			hdf_set_valuef(out, "Query.page=%s", var);
	}

	for (i=0; page[i].name != NULL; i++) {
		if (STARTS_WITH(reqpath, page[i].name)) {
			type = page[i].type;
			break;
		}
	}
	if (type == CBLOG_ATOM)
		criteria.timefmt = "%Y-%m-%dT%H:%M:%SZ";

	if (type == CBLOG_ROOT) {
		if (sscanf(reqpath, "/%4d/%02d/%02d", &yyyy, &mm, &dd) == 3) {
			type = CBLOG_YYYY_MM_DD;
			criteria.type = CRITERIA_TIME_T;
		} else if (sscanf(reqpath, "/%4d/%02d", &yyyy, &mm) == 2) {
			type = CBLOG_YYYY_MM;
			criteria.type = CRITERIA_TIME_T;
		} else if (sscanf(reqpath, "/%4d", &yyyy) == 1) {
			type = CBLOG_YYYY;
			criteria.type = CRITERIA_TIME_T;
		}
	}

	sqlite3_open(get_cblog_db(out), &sqlite);
	if (criteria.timefmt == NULL)
		criteria.timefmt = get_dateformat(out);

	switch (type) {
		case CBLOG_POST:
			reqpath++;
			while (reqpath[0] != '/')
				reqpath++;
			reqpath++;

			if (comment == CBLOG_COMMENT_POST) {
				set_comment(&h, reqpath, hdf_get_value(out, "antispamres", NULL), sqlite);
				type = CBLOG_POST_REDIRECT;
				break;
			}
			nb_posts = build_post(out, reqpath, sqlite);
			if (nb_posts == 0) {
				hdf_set_valuef(out, "err_msg=Unknown post: %s", reqpath);
				type = CBLOG_ERR;
			}
			break;
		case CBLOG_TAG:
			reqpath++;
			while (reqpath[0] != '/')
				reqpath++;
			reqpath++;
			criteria.type = CRITERIA_TAGNAME;
			criteria.tagname = reqpath;
			nb_posts = build_index(out, &criteria, sqlite);
			if (nb_posts == 0) {
				hdf_set_valuef(out, "err_msg=Unknown tag: %s", reqpath);
				type = CBLOG_ERR;
			}
			break;
		case CBLOG_ATOM:
			criteria.feed = true;
			build_index(out, &criteria, sqlite);
			break;
		case CBLOG_ROOT:
			build_index(out, &criteria, sqlite);
			break;
		case CBLOG_YYYY:
			sql_int(sqlite, &criteria.start, "select strftime('%%s','%d-01-01');", yyyy);
			sql_int(sqlite, &criteria.end, "select strftime('%%s','%d-01-01');", yyyy + 1);
			build_index(out, &criteria, sqlite);
			break;
		case CBLOG_YYYY_MM:
			sql_int(sqlite, &criteria.start, "select strftime('%%s','%d-%d-01');", yyyy, mm);
			sql_int(sqlite, &criteria.end, "select strftime('%%s','%d-%d-01');", yyyy, mm + 1);
			build_index(out, &criteria, sqlite);
			break;
		case CBLOG_YYYY_MM_DD:
			sql_int(sqlite, &criteria.start, "select strftime('%%s','%d-%d-%d');", yyyy, mm, dd);
			sql_int(sqlite, &criteria.end, "select strftime('%%s','%d-%d-%d');", yyyy, mm, dd + 1);
			build_index(out, &criteria, sqlite);
			break;
	}

	if (type != CBLOG_ATOM && criteria.feed)
			type = CBLOG_ATOM;

	cs_init(&parse, out);
	string_init(&str);
	cgi_register_strfuncs(parse);
	evhttp_add_header(req->output_headers, "Content-Type", "text/html; charset=UTF-8");
	/* work set the good date format and display everything */
	switch (type) {
		case CBLOG_ATOM:
			sql_text(sqlite, &date, "select strftime('gendate=%%Y-%%m-%%dT%%H:%%M:%%SZ','now');");

			hdf_set_valuef(out, "%s", date);
			free(date);

			evhttp_add_header(req->output_headers, "Content-Type", "application/atom+xml");

			strlcat(cspath, "/atom.cs", MAXPATHLEN);
			neoerr = cs_parse_file(parse, cspath);
			/* XXX test errors*/
			neoerr = cs_render(parse, &str, cblog_output);

			evbuffer_add_printf(evb, "%s", str.buf);
			evhttp_send_reply(req, HTTP_OK, "OK", evb);
			break;
		case CBLOG_ERR:
			set_tags(out, sqlite);
			strlcat(cspath, "/default.cs", MAXPATHLEN);
			neoerr = cs_parse_file(parse, cspath);
			neoerr = cs_render(parse, &str, cblog_output);
			evbuffer_add_printf(evb, "%s", str.buf);
			evhttp_send_reply(req, HTTP_NOTFOUND, "Not found", evb);
			break;
		case CBLOG_POST_REDIRECT:
			evhttp_add_header(req->output_headers, "Location", reqpath);
			evhttp_send_reply(req, HTTP_MOVETEMP, "Redirect", evb);
			break;
		default:
			if (type == CBLOG_POST)
				set_tags(out, sqlite);

			strlcat(cspath, "/default.cs", MAXPATHLEN);
			neoerr = cs_parse_file(parse, cspath);
			neoerr = cs_render(parse, &str, cblog_output);
			evbuffer_add_printf(evb, "%s", str.buf);
			evhttp_send_reply(req, HTTP_OK, "OK", evb);
			break;
	}
	string_clear(&str);
	cs_destroy(&parse);
	evbuffer_free(evb);
	sqlite3_close(sqlite);

	if (neoerr != STATUS_OK && method == EVHTTP_REQ_HEAD) {
		nerr_error_string(neoerr, &neoerr_str);
		syslog(LOG_ERR, "%s", neoerr_str.buf);
		string_clear(&neoerr_str);
	}
	nerr_ignore(&neoerr);
	hdf_destroy(&out);
}
/* vim: set sw=4 sts=4 ts=4 : */

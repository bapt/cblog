#include <stdbool.h>
#include <string.h>
#include <sqlite3.h>
#include <syslog.h>
#include <sys/queue.h>
#include <sys/param.h>

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
	const char *tagname;
	time_t start;
	time_t end;
};

void
cblog_log(const char * message, ...)
{
	va_list		args;

	va_start(args, message);
	vsyslog(LOG_ERR, message, args);
	va_end(args);
}

int
add_posts_to_hdf(HDF *hdf, sqlite3_stmt *stmt, sqlite3 *sqlite)
{
	char *filename;
	int icol;
	int nb_post, nb_tags;
	nb_post = 0;
	sqlite3_stmt *stmt2;

	if (sqlite3_prepare_v2(sqlite,
	    "SELECT tag FROM tags, tags_posts, posts WHERE link=?1 AND posts.id=post_id and tag_id=tags.id;",
	    -1, &stmt2, NULL) != SQLITE_OK) {
		cblog_log("%s", sqlite3_errmsg(sqlite));
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
		cblog_log("%s", sqlite3_errmsg(sqlite));
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
	char *submit;

	submit = get_query_str(hdf, "submit");
	if (submit)
		printf("%s\n", submit);
	if (submit != NULL && EQUALS(submit, "Post"))
			set_comment(hdf, postname, sqlite);

	if (sqlite3_prepare_v2(sqlite, 
		"SELECT link as filename, title, source, html, date from posts "
		"WHERE link=?1 ;",
		-1, &stmt, NULL) != SQLITE_OK) {
		cblog_log("%s", sqlite3_errmsg(sqlite));
		return 0;
	}

	sqlite3_bind_text(stmt, 1, postname, -1, SQLITE_STATIC);

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
	    "SELECT link as filename, title, source, html, date from posts, tags_posts, tags "
		"WHERE posts.id=post_id and tag_id=tags.id and tag=?3 "
		"ORDER BY DATE DESC LIMIT ?1 OFFSET ?2 ";
		counturl =
	    "SELECT count(*) from posts, tags_posts, tags "
		"WHERE posts.id=post_id and tag_id=tags.id and tag=?1;";
		break;
	case CRITERIA_TIME_T:
		baseurl = "SELECT link as filename, title, source, html, date from posts "
			"WHERE date BETWEEN ?3 and ?4 "
			"ORDER BY date DESC LIMIT ?1 OFFSET ?2;";
		counturl = "SELECT count(*) from posts WHERE date BETWEEN ?1 and ?2 ";
		break;
	default:
		baseurl = "SELECT link as filename, title, source, html, date from posts ORDER BY DATE DESC LIMIT ?1 OFFSET ?2;";
		counturl = "SELECT count(*) from posts;";
		break;
	}
	if (sqlite3_prepare_v2(sqlite,
		baseurl,
	    -1, &stmt, NULL) != SQLITE_OK) {
		cblog_log("%s", sqlite3_errmsg(sqlite));
		return 0;
	}

	if (sqlite3_prepare_v2(sqlite, counturl, -1, &stmtcnt, NULL) != SQLITE_OK) {
		cblog_log("%s", sqlite3_errmsg(sqlite));
		return 0;
	}

	first_post = (page * max_post) - max_post;
	sqlite3_bind_int(stmt, 1, max_post);
	sqlite3_bind_int(stmt, 2, first_post);
	switch (criteria->type) {
	case CRITERIA_TAGNAME:
		sqlite3_bind_text(stmt, 3, criteria->tagname, -1, SQLITE_STATIC);
		sqlite3_bind_text(stmtcnt, 1, criteria->tagname, -1, SQLITE_STATIC);
		break;
	case CRITERIA_TIME_T:
		sqlite3_bind_int64(stmt, 3, criteria->start);
		sqlite3_bind_int64(stmt, 4, criteria->end);
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

void
cblog(struct evhttp_request* req, void* args)
{
	NEOERR				*neoerr;
	HDF *hdf, *out;
	STRING str, neoerr_str;
	char *date_format;
	const char *reqpath, *requesturi, *tplpath;
	const struct evhttp_uri *uri;
	const char *q;
	char cspath[MAXPATHLEN];
	struct evkeyvalq *h;
	CSPARSE *parse;
	int method;
	int type, i, nb_posts;
	time_t gentime, posttime;
	int yyyy, mm, dd, datenum;
	struct criteria criteria;
	struct tm calc_time, *date;
	char buf[BUFSIZ];
	const char *typefeed;
	const char *var;
	struct evbuffer *evb = NULL;
	sqlite3 *sqlite;

	/* read the configuration file */

	type = CBLOG_ROOT;
	criteria.type = 0;
	criteria.feed = false;

	memset(&calc_time, 0, sizeof(struct tm));
	h = malloc(sizeof(struct evkeyvalq));

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
		evhttp_parse_query_str(q, h);

	if (method == EVHTTP_REQ_POST) {
		/* parse the post and set everything into the hdf */
		struct evbuffer *postbuf = evhttp_request_get_input_buffer(req);
		size_t sz, a;
		if (evbuffer_get_length(postbuf) > 0) {
			int len = evbuffer_get_length(postbuf);
			char *tmp = malloc(len+1);
			memcpy(tmp, evbuffer_pullup(postbuf, -1), len);
			tmp[len] = '\0';
			evhttp_parse_query_str(tmp, h);
			if ((var = evhttp_find_header(h, "submit")) != NULL)
				hdf_set_valuef(out, "Query.submit=%s", var);
			if ((var = evhttp_find_header(h, "name")) != NULL)
				hdf_set_valuef(out, "Query.name=%s", var);
			if ((var = evhttp_find_header(h, "url")) != NULL)
				hdf_set_valuef(out, "Query.url=%s", var);
			if ((var = evhttp_find_header(h, "comment")) != NULL)
				hdf_set_valuef(out, "Query.comment=%s", var);
			if ((var = evhttp_find_header(h, "antispam")) != NULL)
				hdf_set_valuef(out, "Query.antispam=%s", var);
			if ((var = evhttp_find_header(h, "test1")) != NULL)
				hdf_set_valuef(out, "Query.test1=%s", var);
			if ((var = evhttp_find_header(h, "test2")) != NULL)
				hdf_set_valuef(out, "Query.test2=%s", var);
			free(tmp);
		}
	}

	if (q != NULL) {
		typefeed = evhttp_find_header(h, "feed");
		if (typefeed != NULL && (
					EQUALS(typefeed, "rss") ||
					EQUALS(typefeed, "atom")))
			criteria.feed=true;
	}

	for (i=0; page[i].name != NULL; i++) {
		if (STARTS_WITH(reqpath, page[i].name)) {
			type = page[i].type;
			break;
		}
	}

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
	switch (type) {
		case CBLOG_POST:
			reqpath++;
			while (reqpath[0] != '/')
				reqpath++;
			reqpath++;

			nb_posts = build_post(out, reqpath, sqlite);
			if (nb_posts == 0) {
				hdf_set_valuef(out, "err_msg=Unknown post: %s", requesturi);
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
				hdf_set_valuef(out, "err_msg=Unknown tag: %s", requesturi);
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
			calc_time.tm_year = yyyy - 1900;
			calc_time.tm_mon = 0;
			calc_time.tm_mday = 1;
			criteria.start = mktime(&calc_time);
			calc_time.tm_mon = 11;
			calc_time.tm_mday = 31;
			calc_time.tm_hour = 23;
			calc_time.tm_min = 59;
			calc_time.tm_sec = 59;
			criteria.end = mktime(&calc_time);
			build_index(out, &criteria, sqlite);
			break;
		case CBLOG_YYYY_MM:
			calc_time.tm_year = yyyy - 1900;
			calc_time.tm_mon = mm - 1;
			calc_time.tm_mday = 1;
			criteria.start = mktime(&calc_time);
			calc_time.tm_mon = mm;
			calc_time.tm_hour = 23;
			calc_time.tm_min = 59;
			calc_time.tm_sec = 59;
			criteria.end = mktime(&calc_time);
			criteria.end -= 60 * 60 * 24;
			build_index(out, &criteria, sqlite);
			break;
		case CBLOG_YYYY_MM_DD:
			calc_time.tm_year = yyyy - 1900;
			calc_time.tm_mon = mm - 1;
			calc_time.tm_mday = dd;
			criteria.start = mktime(&calc_time);
			calc_time.tm_hour = 23;
			calc_time.tm_min = 59;
			calc_time.tm_sec = 59;
			criteria.end = mktime(&calc_time);
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
			HDF_FOREACH(hdf, out, "Posts") {

				posttime = hdf_get_int_value(hdf, "date", time(NULL));
				date = gmtime(&posttime);

				hdf_set_valuef(hdf, "date=%04d-%02d-%02dT%02d:%02d:%02dZ",
						date->tm_year + 1900, date->tm_mon + 1, date->tm_mday, 
						date->tm_hour, date->tm_min, date->tm_sec);
			}

			gentime = time(NULL);
			date = gmtime(&gentime);

			hdf_set_valuef(out, "gendate=%04d-%02d-%02dT%02d:%02d:%02dZ",
					date->tm_year + 1900, date->tm_mon + 1, date->tm_mday, 
					date->tm_hour, date->tm_min, date->tm_sec);

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
		default:
			if (type == CBLOG_POST)
				set_tags(out, sqlite);

			date_format = get_dateformat(out);

			HDF_FOREACH(hdf, out, "Posts") {
				datenum = hdf_get_int_value(hdf, "date", time(NULL));
				time_to_str(datenum, date_format, buf, BUFSIZ);

				hdf_set_valuef(hdf, "date=%s", buf);
			}
			strlcat(cspath, "/default.cs", MAXPATHLEN);
			neoerr = cs_parse_file(parse, cspath);
			neoerr = cs_render(parse, &str, cblog_output);
			evbuffer_add_printf(evb, "%s", str.buf);
			evhttp_send_reply(req, HTTP_OK, "OK", evb);
			break;
	}
	string_clear(&str);
	cs_destroy(&parse);

	if (neoerr != STATUS_OK && method == EVHTTP_REQ_HEAD) {
		nerr_error_string(neoerr, &neoerr_str);
		cblog_log(neoerr_str.buf);
		string_clear(&neoerr_str);
	}
	nerr_ignore(&neoerr);
}
/* vim: set sw=4 sts=4 ts=4 : */

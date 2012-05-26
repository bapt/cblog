#include <time.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sqlite3.h>

#include "cblogweb.h"
#include "cblog_utils.h"
#include <syslog.h>

int
get_comments_count(const char *postname, sqlite3 *sqlite)
{
	sqlite3_stmt *stmt;
	int nb;

	if (sqlite3_prepare_v2(sqlite,
	    "SELECT count(comment) FROM comments, posts where posts.id = post_id and link=?1;",
	    -1 , &stmt, NULL) != SQLITE_OK) {
		cblog_log("%s", sqlite3_errmsg(sqlite));
		return (0);
	}

	sqlite3_bind_text(stmt, 1, postname, -1, SQLITE_STATIC);
	sqlite3_step(stmt);
	nb = sqlite3_column_int(stmt, 0);
	sqlite3_finalize(stmt);

	return (nb);
}

void
get_comments(HDF *hdf, const char *postname, sqlite3 *sqlite)
{
	sqlite3_stmt *stmt;
	int icol;
	int i = 0;

	if (sqlite3_prepare_v2(sqlite,
	    "SELECT author, url, strftime(?1, date(comments.date, 'unixepoch')) as date, comment as content FROM comments, posts where post_id=posts.id and link=?2 ORDER by comments.date ASC;",
	    -1, &stmt, NULL) != SQLITE_OK) {
		cblog_log("%s", sqlite3_errmsg(sqlite));
		return;
	}

	sqlite3_bind_text(stmt, 1, get_dateformat(hdf), -1, SQLITE_STATIC);
	sqlite3_bind_text(stmt, 2, postname, -1, SQLITE_STATIC);

	while (sqlite3_step(stmt) == SQLITE_ROW) {
		for (icol = 0; icol < sqlite3_column_count(stmt); icol++) {
			switch (sqlite3_column_type(stmt, icol)) {
			case SQLITE_TEXT:
				hdf_set_valuef(hdf, "Posts.0.comments.%i.%s=%s", i, sqlite3_column_name(stmt, icol), sqlite3_column_text(stmt, icol));
				break;
			default:
				break;
			}
		}
		i++;
	}

	sqlite3_finalize(stmt);
}

void
set_comment(HDF *hdf, const char *postname, sqlite3 *sqlite)
{
	char *nospam;
	char *var;
	char *from, *to;
	char subject[LINE_MAX];
	sqlite3_stmt *stmt;

	/* very simple antispam */
	if (strlen(get_query_str(hdf, "test1")) != 0)
		return;

	/* second one just in case */
	if ((nospam = hdf_get_value(hdf, "antispamres", NULL)) == NULL)
		return;

	if (get_query_str(hdf, "antispam") == NULL)
		return;

	if (!EQUALS(nospam, get_query_str(hdf, "antispam")))
		return;

	/* prevent empty name and empty comment */
	if (strlen(get_query_str(hdf, "name")) == 0 || strlen(get_query_str(hdf, "comment")) == 0)
		return;

	/* TODO Log here */
	if (sqlite3_prepare_v2(sqlite, "INSERT INTO comments (post_id, author, url, comment, date) values((select id from posts where link=?1), ?2, ?3, ?4, strftime('%s', 'now'));", -1, &stmt, NULL) != SQLITE_OK) {
		cblog_log("%s", sqlite3_errmsg(sqlite));
		return;
	}

	sqlite3_bind_text(stmt, 1, postname, -1, SQLITE_STATIC);
	var = evhttp_htmlescape(get_query_str(hdf, "name"));
	if (var != NULL) {
		sqlite3_bind_text(stmt, 2, var, -1, SQLITE_STATIC);
		free(var);
	}
	var = evhttp_htmlescape(get_query_str(hdf, "url"));
	if (var != NULL) {
		sqlite3_bind_text(stmt, 3, var, -1, SQLITE_STATIC);
		free(var);
	}
	var = evhttp_htmlescape(get_query_str(hdf, "comment"));
	if (var != NULL) {
		sqlite3_bind_text(stmt, 4, var, -1, SQLITE_STATIC);
		free(var);
	}

	sqlite3_step(stmt);

	/* All is good, send an email for the new comment */
	if (hdf_get_int_value(hdf, "email.enable", 0) == 1) {
		from = hdf_get_value(hdf, "email.from", NULL);
		to   = hdf_get_value(hdf, "email.to", NULL);

		snprintf(subject, LINE_MAX, "New comment by %s", get_query_str(hdf, "name"));

		if (from && to)
			send_mail(from, to, subject,
					get_cgi_str(hdf, "RemoteAddress"),
					get_query_str(hdf, "comment"),
					get_query_str(hdf, "name"),
					get_query_str(hdf, "url"));
	}

	/* Some cleanup on the hdf */
	hdf_remove_tree(hdf, "Query.name");
	hdf_remove_tree(hdf, "Query.url");
	hdf_remove_tree(hdf, "Query.comment");
	sqlite3_finalize(stmt);
}

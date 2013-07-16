#include <stdio.h>
#include <libgen.h>
#include <stdbool.h>
#include <err.h>
#include <limits.h>
#include <unistd.h>

#include <soldout/buffer.h>
#include <soldout/markdown.h>
#include <soldout/renderers.h>

#include "cblogctl.h"
#include "cblog_common.h"
#include "cblog_utils.h"

/* path the the CDB database file */
char cblog_cdb[PATH_MAX];

void
cblogctl_list(void)
{
	sqlite3 *sqlite;
	sqlite3_stmt *stmt;

	sqlite3_initialize();
	sqlite3_open(cblog_cdb, &sqlite);

	sqlite3_prepare_v2(sqlite, "SELECT link FROM posts ORDER BY DATE;", -1, &stmt, NULL);

	while (sqlite3_step(stmt) == SQLITE_ROW)
		printf("%s\n", sqlite3_column_text(stmt, 0));

	sqlite3_finalize(stmt);
	sqlite3_close(sqlite);
	sqlite3_shutdown();
}

static void
print_stmt(sqlite3_stmt *stmt)
{
	int icol;

	while (sqlite3_step(stmt) == SQLITE_ROW) {
		for (icol = 0; icol < sqlite3_column_count(stmt); icol++) {
			switch (sqlite3_column_type(stmt, icol)) {
			case SQLITE_TEXT:
				printf("%s: %s\n", sqlite3_column_name(stmt, icol), sqlite3_column_text(stmt, icol));
				break;
			case SQLITE_INTEGER:
				printf("%s: %lld\n", sqlite3_column_name(stmt, icol), sqlite3_column_int64(stmt, icol));
				break;
			default:
				/* do nothing */
				break;
			}
		}
	}
}

void
cblogctl_info(const char *post_name)
{
	sqlite3 *sqlite;
	sqlite3_stmt *stmt;

	sqlite3_initialize();
	sqlite3_open(cblog_cdb, &sqlite);

	if (sqlite3_prepare_v2(sqlite, "SELECT title, datetime(posts.date, 'unixepoch') as date "
	    "FROM posts "
	    "WHERE link=?1;", -1, &stmt, NULL) != SQLITE_OK)
		errx(1, "%s", sqlite3_errmsg(sqlite));

	sqlite3_bind_text(stmt, 1, post_name, -1, SQLITE_STATIC);
	print_stmt(stmt);
	sqlite3_finalize(stmt);
	if (sqlite3_prepare_v2(sqlite,
	    "SELECT group_concat(tag, ', ') as tags "
	    "FROM tags, tags_posts, posts WHERE link=?1 and posts.id=tags_posts.post_id and tags_posts.tag_id=tags.id;",
	    -1 , &stmt, NULL) != SQLITE_OK)
		errx(1, "%s", sqlite3_errmsg(sqlite));
	sqlite3_bind_text(stmt, 1, post_name, -1, SQLITE_STATIC);
	print_stmt(stmt);
	sqlite3_finalize(stmt);
	if (sqlite3_prepare_v2(sqlite,
	    "SELECT count(comment) as 'Number of comments' FROM comments, posts where posts.id = post_id and link=?1;",
	    -1 , &stmt, NULL) != SQLITE_OK)
		errx(1, "%s", sqlite3_errmsg(sqlite));
	sqlite3_bind_text(stmt, 1, post_name, -1, SQLITE_STATIC);
	print_stmt(stmt);
	sqlite3_finalize(stmt);
	printf("\n");
	sqlite3_close(sqlite);
	sqlite3_shutdown();
}

void
cblogctl_get(const char *post_name)
{
	sqlite3 *sqlite;
	sqlite3_stmt *stmt;
	int icol;
	FILE *out;

	sqlite3_initialize();
	sqlite3_open(cblog_cdb, &sqlite);

	out = fopen(post_name, "w");

	if (sqlite3_prepare_v2(sqlite, "SELECT title as Title, "
	    "group_concat(tag, ', ') as Tags, source "
	    "FROM posts, tags_posts, tags "
	    "WHERE link=?1 and posts.id=tags_posts.post_id and tags_posts.tag_id=tags.id;", -1, &stmt, NULL) != SQLITE_OK)
		errx(1, "%s", sqlite3_errmsg(sqlite));

	sqlite3_bind_text(stmt, 1, post_name, -1, SQLITE_STATIC);
	while (sqlite3_step(stmt) == SQLITE_ROW) {
		for (icol = 0; icol < sqlite3_column_count(stmt); icol++) {
			switch (sqlite3_column_type(stmt, icol)) {
				case SQLITE_TEXT:
					if (strcmp(sqlite3_column_name(stmt, icol),"source") == 0)
							fprintf(out, "\n%s\n",  sqlite3_column_text(stmt, icol));
					else
						fprintf(out, "%s: %s\n", sqlite3_column_name(stmt, icol), sqlite3_column_text(stmt, icol));
					break;
				case SQLITE_INTEGER:
					fprintf(out, "%s: %lld\n", sqlite3_column_name(stmt, icol), sqlite3_column_int64(stmt, icol));
					break;
				default:
					/* do nothing */
					break;
			}
		}
	}
	sqlite3_finalize(stmt);
	sqlite3_close(sqlite);
	sqlite3_shutdown();
	fclose(out);
}

void
cblogctl_add(const char *post_path)
{
	sqlite3 *sqlite;
	sqlite3_stmt *stmt, *stmtu;;
	FILE *post;
	char *ppath, *val;
	char *tagbuf;
	struct buf *ib, *ob;
	char filebuf[LINE_MAX];
	bool headers = true;
	int nbel, i;
	size_t next;

	tagbuf = NULL;
	ppath = basename(post_path);

	sqlite3_initialize();
	sqlite3_open(cblog_cdb, &sqlite);

	post = fopen(post_path, "r");

	sql_exec(sqlite, "pragma foreign_keys=on;");

	if (post == NULL)
		errx(1, "Unable to open %s", post_path);

	if (sqlite3_prepare_v2(sqlite, "INSERT INTO posts (link, title, source, html, date) "
	    "values (?1, trim(?2, ' \t\r\n'), ?3, ?4, strftime('%s', 'now'));",
	    -1, &stmt, NULL) != SQLITE_OK)
		errx(1, "%s", sqlite3_errmsg(sqlite));
	if (sqlite3_prepare_v2(sqlite, "UPDATE posts set title=trim(?2, ' \t\r\n'), source=?3, html=?4 where link=?1;",
	    -1, &stmtu, NULL) != SQLITE_OK)
		errx(1, "%s", sqlite3_errmsg(sqlite));

	sqlite3_bind_text(stmt, 1, ppath, -1, SQLITE_STATIC);
	sqlite3_bind_text(stmtu, 1, ppath, -1, SQLITE_STATIC);
	ib = bufnew(BUFSIZ);

	while (fgets(filebuf, LINE_MAX, post) != NULL) {
		if (filebuf[0] == '\n' && headers) {
			headers = false;
			continue;
		}

		if (headers) {
			if (STARTS_WITH(filebuf, "Title: ")) {
				val = filebuf + strlen("Title: ");
				sqlite3_bind_text(stmt, 2, val, -1, SQLITE_TRANSIENT);
				sqlite3_bind_text(stmtu, 2, val, -1, SQLITE_TRANSIENT);

			} else if (STARTS_WITH(filebuf, "Tags")) {

				val = filebuf + strlen("Tags: ");
				tagbuf=strdup(val);
			}
		} else
			bufputs(ib, filebuf);
	}
	fclose(post);

	ob = bufnew(BUFSIZ);
	markdown(ob, ib, &mkd_xhtml);
	bufnullterm(ob);
	bufnullterm(ib);

	sqlite3_bind_text(stmt, 3, ib->data, -1, SQLITE_STATIC);
	sqlite3_bind_text(stmtu, 3, ib->data, -1, SQLITE_STATIC);
	sqlite3_bind_text(stmt, 4, ob->data, -1, SQLITE_STATIC);
	sqlite3_bind_text(stmtu, 4, ob->data, -1, SQLITE_STATIC);
	if (sqlite3_step(stmt) != SQLITE_DONE) {
		if (sqlite3_step(stmtu) != SQLITE_DONE)
			errx(1, "%s", sqlite3_errmsg(sqlite));
	}
	sqlite3_finalize(stmt);
	sqlite3_finalize(stmtu);
	bufrelease(ib);
	bufrelease(ob);

	if (tagbuf == NULL)
		return;

	if (sqlite3_prepare_v2(sqlite, "DELETE from tags_posts where post_id IN (select id from posts where link=?1);", -1, &stmt, NULL) != SQLITE_OK)
		errx(1, "%s", sqlite3_errmsg(sqlite));
	sqlite3_bind_text(stmt, 1, ppath, -1, SQLITE_STATIC);
	sqlite3_step(stmt);
	sqlite3_finalize(stmt);

	if (sqlite3_prepare_v2(sqlite, "INSERT OR IGNORE into tags (tag) values (trim(?1));", -1, &stmt, NULL) != SQLITE_OK)
		errx(1, "%s", sqlite3_errmsg(sqlite));

	if (sqlite3_prepare_v2(sqlite, "INSERT OR IGNORE into tags_posts (tag_id, post_id) values ((select id from tags where tag=trim(?1)), (select id from posts where link=?2));", -1, &stmtu, NULL) != SQLITE_OK)
		errx(1, "%s", sqlite3_errmsg(sqlite));

	nbel = splitchr(tagbuf, ',');
	val = tagbuf;
	for (i = 0; i <= nbel; i++) {
		next = strlen(val);
		if (val[next - 1] == '\n')
			val[next - 1] = '\0';
		sqlite3_bind_text(stmt, 1, val, -1, SQLITE_STATIC);
		sqlite3_step(stmt);
		sqlite3_reset(stmt);
		sqlite3_bind_text(stmtu, 1, val, -1, SQLITE_STATIC);
		sqlite3_bind_text(stmtu, 2, ppath, -1, SQLITE_STATIC);
		sqlite3_step(stmtu);
		sqlite3_reset(stmtu);
		val += next + 1;
	}

	sqlite3_finalize(stmt);
	sqlite3_finalize(stmtu);
}

void
cblogctl_del(const char *post_name)
{
	sqlite3 *sqlite;
	sqlite3_stmt *stmt;

	sqlite3_initialize();
	sqlite3_open(cblog_cdb, &sqlite);
	sql_exec(sqlite, "PRAGMA foreign_key=on;");

	if (sqlite3_prepare_v2(sqlite, "DELETE FROM posts WHERE link=?1;", -1, &stmt, NULL) != SQLITE_OK)
		errx(1, "%s", sqlite3_errmsg(sqlite));

	sqlite3_bind_text(stmt, 1, post_name, -1, SQLITE_STATIC);
	sqlite3_step(stmt);
	sqlite3_finalize(stmt);

	sql_exec(sqlite, "DELETE from tags_posts where post_id not in (select id from posts)");

	sqlite3_close(sqlite);
	sqlite3_shutdown();
}

void
cblogctl_create(void)
{
	sqlite3 *sqlite;
	int ret;

	sqlite3_initialize();
	if (sqlite3_open(cblog_cdb, &sqlite) != SQLITE_OK)
		errx(1, "%s", sqlite3_errmsg(sqlite));

	ret = sql_exec(sqlite,
		"CREATE TABLE posts "
		    "(id INTEGER PRIMARY KEY, "
		    "link TEXT NOT NULL UNIQUE," 
		    "title TEXT NOT NULL, "
		    "source TEXT NOT NULL, "
		    "html TEXT NOT NULL, "
		    "date INTEGER, "
		    "published INTEGER); "
		"CREATE TABLE comments ("
		    "id INTEGER PRIMARY KEY, "
		    "post_id INTEGER REFERENCES posts(id) ON DELETE CASCADE ON UPDATE CASCADE, "
		    "author TEXT NOT NULL, "
		    "url TEXT, "
		    "date INTEGER, "
		    "comment TEXT NOT NULL); "
		"CREATE TABLE tags ("
		    "id INTEGER PRIMARY KEY, "
		    "tag TEXT NOT NULL UNIQUE); "
		"CREATE TABLE tags_posts ("
		    "tag_id INTEGER REFERENCES tags(id) ON DELETE CASCADE ON UPDATE CASCADE, "
		    "post_id INTEGER REFERENCES posts(id) ON DELETE CASCADE ON UPDATE CASCADE, "
		    "PRIMARY KEY (tag_id, post_id));"
		);
	if (ret < 0)
		errx(1, "%s", sqlite3_errmsg(sqlite));

	sqlite3_close(sqlite);
	sqlite3_shutdown();
}

void
cblogctl_path(void)
{
	printf("%s\n", cblog_cdb);
}

void
cblogctl_version(void)
{
	fprintf(stderr, "%s (%s)\n", cblog_version, cblog_url);
}
/* vim: set sw=4 sts=4 ts=4 : */

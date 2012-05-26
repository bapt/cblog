#include <sys/param.h>
#include <sys/stat.h>
#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <cdb.h>
#include <ClearSilver.h>

#include "cblog_utils.h"

static char *field[] = {
	"title",
	"tags",
	"source",
	"html",
	"ctime",
	"published",
	"comments",
	NULL
};

char *
db_get(struct cdb *cdb) {
	int vpos, vlen;
	char *val;

	vpos = cdb_datapos(cdb);
	vlen = cdb_datalen(cdb);

	val = malloc(vlen + 1);
	if (val == NULL)
		errx(1, "Unable to allocate memory");

	cdb_read(cdb, val, vlen, vpos);
	val[vlen] = '\0';

	return val;
}

sqlite3 *
init_sqlite(const char *path)
{
	int ret;
	sqlite3 *sqlite = NULL;

	sqlite3_initialize();
	if (sqlite3_open(path, &sqlite) != SQLITE_OK) {
		sqlite3_shutdown();
		return (NULL);
	}
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
		    "PRIMARY KEY(tag_id, post_id));"
		);
	if (ret < 0) {
		sqlite3_close(sqlite);
		sqlite3_shutdown();
		return (NULL);
	}

	return (sqlite);
}

int
main(int argc, char *argv[])
{
	char *s, *val, *val2;
	char *buffer, *bufstart;
	struct cdb cdb;
	struct cdb_find cdbf;
	struct stat st;
	char key[BUFSIZ];
	char comment_file[MAXPATHLEN];
	sqlite3 *sqlite;
	sqlite3_stmt *stmt_post, *stmt_tag, *stmt_tags, *stmt_comments;
	int db, i, pos, j, commentfd;
	size_t next;

	s = CDB_PATH"/cblog.cdb";

	if ((sqlite = init_sqlite(CDB_PATH"/cblog.sqlite")) == NULL)
		errx(1, "fail to init database %s", argv[1]);

	if ((db = open(s, O_RDONLY)) < 0)
		err(1, "open(%s)", s);

	cdb_init(&cdb, db);
	cdb_findinit(&cdbf, &cdb, "posts", 5);
	if (sqlite3_prepare_v2(sqlite,
	    "INSERT INTO posts (link, title, source, html, date, published) values (?1, ?2, ?3, ?4, ?5, 0)",
	    -1, &stmt_post, NULL) != SQLITE_OK)
		errx(1, "%s", sqlite3_errmsg(sqlite));
	if (sqlite3_prepare_v2(sqlite, "INSERT OR IGNORE INTO tags (tag) values (?1);", -1, &stmt_tag, NULL) != SQLITE_OK)
		errx(1, "%s", sqlite3_errmsg(sqlite));
	if (sqlite3_prepare_v2(sqlite, "INSERT INTO tags_posts (tag_id, post_id) values ((select id from tags where tag=?1), ?2);", -1, &stmt_tags, NULL) != SQLITE_OK)
		errx(1, "%s", sqlite3_errmsg(sqlite));
	if (sqlite3_prepare_v2(sqlite, "INSERT INTO comments (post_id, author, url, comment, date) values(trim(?1), ?2, ?3, ?4, ?5);", -1, &stmt_comments, NULL) != SQLITE_OK)
		errx(1, "%s", sqlite3_errmsg(sqlite));

	sql_exec(sqlite, "BEGIN;");
	while (cdb_findnext(&cdbf) > 0) {
		pos = 1;
		val = db_get(&cdb);
		sqlite3_bind_text(stmt_post, pos, val, -1, SQLITE_TRANSIENT);
		pos++;

		for (i = 0; field[i] != NULL; i++) {
			if (strcmp(field[i], "comments") == 0)
				continue;
			if (strcmp(field[i], "published") == 0)
				continue;
			if (strcmp(field[i], "tags") == 0)
				continue;
			snprintf(key, BUFSIZ, "%s_%s", val, field[i]);
			if (cdb_find(&cdb, key, strlen(key)) > 0) {
				val2 = db_get(&cdb);
				sqlite3_bind_text(stmt_post, pos, val2, -1, SQLITE_TRANSIENT);
				pos++;
				free(val2);
			}
		}
		sqlite3_step(stmt_post);
		sqlite3_reset(stmt_post);
		int id = sqlite3_last_insert_rowid(sqlite);
		int nbel;
		snprintf(key, BUFSIZ, "%s_tags", val);
		if (cdb_find(&cdb, key, strlen(key)) > 0) {
			val2 = db_get(&cdb);
			nbel = splitchr(val2, ',');
			char *nval = val2;
			for (j = 0; j <= nbel; j++) {
				next = strlen(nval);
				sqlite3_bind_text(stmt_tag, 1, nval, -1, SQLITE_TRANSIENT);
				sqlite3_step(stmt_tag);
				sqlite3_reset(stmt_tag);
				sqlite3_bind_text(stmt_tags, 1, nval, -1, SQLITE_TRANSIENT);
				sqlite3_bind_int64(stmt_tags, 2, id);
				sqlite3_step(stmt_tags);
				sqlite3_reset(stmt_tags);
				nval += next + 1;
			}
			free(val2);
		}
		/* now comments */
		snprintf(comment_file, MAXPATHLEN, CDB_PATH"/comments/%s", val);

		if (stat(comment_file, &st) != -1) {
			buffer = malloc((st.st_size + 1) * sizeof(char));
			commentfd = open(comment_file, O_RDONLY);
			read(commentfd, buffer, st.st_size);
			buffer[st.st_size] = '\0';
			bufstart = buffer;
			nbel = splitchr(buffer, '\n');
			next = strlen(buffer);
			sqlite3_bind_int64(stmt_comments, 1, id);
			j=0;
			while (j <= nbel) {
				if (STARTS_WITH(buffer, "comment: "))
					sqlite3_bind_text(stmt_comments, 4, cgi_url_unescape(buffer + 9), -1, SQLITE_TRANSIENT);
				else if (STARTS_WITH(buffer, "name: "))
					sqlite3_bind_text(stmt_comments, 2, buffer + 6, -1, SQLITE_TRANSIENT);
				else if (STARTS_WITH(buffer, "url: "))
					sqlite3_bind_text(stmt_comments, 3, buffer + 5, -1, SQLITE_TRANSIENT);
				else if (STARTS_WITH(buffer, "date: "))
					sqlite3_bind_text(stmt_comments, 5, buffer + 6, -1, SQLITE_TRANSIENT);
				else if (STARTS_WITH(buffer, "---")) {
					sqlite3_step(stmt_comments);
					sqlite3_reset(stmt_comments);
					sqlite3_bind_int64(stmt_comments, 1, id);
				}
				buffer += next + 1;
				j++;
				next = strlen(buffer);
			}
			free(bufstart);
		}
		free(val);
	}
	sql_exec(sqlite, "COMMIT;");
	sqlite3_finalize(stmt_post);
	sqlite3_finalize(stmt_tag);
	sqlite3_finalize(stmt_tags);
	sqlite3_finalize(stmt_comments);

	cdb_free(&cdb);
	close(db);

	sqlite3_shutdown();

	return (0);
}

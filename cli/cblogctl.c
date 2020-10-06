#include <ClearSilver.h>
#include <stdio.h>
#include <libgen.h>
#include <stdbool.h>
#include <err.h>
#include <limits.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>

#include <soldout/buffer.h>
#include <soldout/markdown.h>
#include <soldout/renderers.h>

#include "cblogctl.h"
#include "cblog_common.h"
#include "cblog_utils.h"

/* path the the CDB database file */
char cblog_cdb[PATH_MAX];

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
	ppath = basename(__DECONST(char *,post_path));

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
cblogctl_path(HDF *conf)
{
	printf("%s\n", get_cblog_db(conf));
}

void
cblogctl_version(void)
{
	fprintf(stderr, "%s (%s)\n", cblog_version, cblog_url);
}
struct article {
	char *title;
	char *tags;
	struct buf *content;
};

struct article *
parse_article(int dfd, const char *name)
{
	int fd;
	FILE *f;
	char *line = NULL;
	char *val;
	size_t linecap = 0;
	ssize_t linelen;
	bool headers = true;
	struct article *ar = NULL;

	fd = openat(dfd, name, O_RDONLY);
	if (fd == -1)
		err(1, "Impossible to open: '%s'", name);

	f = fdopen(fd, "r");
	ar = calloc(1, sizeof(*ar));
	if (ar == NULL)
		err(1, "malloc");
	while ((linelen = getline(&line, &linecap, f)) > 0) {
		if (line[0] == '\n' && headers) {
			headers = false;
			ar->content = bufnew(BUFSIZ);
			continue;
		}
		if (!headers) {
			bufputs(ar->content, line);
			continue;
		}

		/* headers */
		if (line[linelen -1 ] == '\n')
			line[linelen - 1] = '\0';
		if (STARTS_WITH(line, "Title: ")) {
			val = line + strlen("Title: ");
			ar->title = strdup(val);
		} else if (STARTS_WITH(line, "Tags: ")) {
			val = line + strlen("Tags: ");
			ar->tags = strdup(val);
		}
	}
	if (ar->content != NULL)
		bufnullterm(ar->content);

	free(line);

	fclose(f);
	return (ar);
}

void
cblogctl_gen(HDF *conf)
{
	struct dirent *dp;
	struct article *ar;
	DIR *dir;
	int dbfd;
	const char *dbpath = get_cblog_db(conf);

	dbfd = open(dbpath, O_DIRECTORY);
	if (dbfd == -1)
		err(1, "Impossible to open the database directory '%s'", dbpath);

	dir = fdopendir(dbfd);
	while ((dp = readdir(dir)) != NULL) {
		if (dp->d_namlen == 1 && strcmp(dp->d_name, ".") == 0)
			continue;
		if (dp->d_namlen == 2 && strcmp(dp->d_name, "..") == 0)
			continue;
		/* THIS IS WHERE THE CODE WILL BE */
		ar = parse_article(dbfd, dp->d_name);
		printf("%s, %s\n", dp->d_name, ar->title);
	}
	closedir(dir);
}

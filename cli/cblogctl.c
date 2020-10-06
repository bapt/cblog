#include <sys/mman.h>
#include <sys/param.h>
#include <sys/capsicum.h>

#include <ClearSilver.h>
#include <stdio.h>
#include <libgen.h>
#include <stdbool.h>
#include <err.h>
#include <limits.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <utlist.h>
#include <errno.h>
#include <string.h>
#include <xmalloc.h>
#include <capsicum_helpers.h>

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
	time_t creation;
	time_t modification;
	struct buf *content;
	struct article *next, *prev;
};

bool
mkdirat_p(int fd, const char *path)
{
	const char *next;
	char *walk, *walkorig, pathdone[MAXPATHLEN];

	walk = walkorig = xstrdup(path);
	pathdone[0] = '\0';

	while ((next = strsep(&walk, "/")) != NULL) {
		if (*next == '\0')
			continue;
		strlcat(pathdone, next, sizeof(pathdone));
		if (mkdirat(fd, pathdone, 0755) == -1) {
			if (errno == EEXIST) {
				strlcat(pathdone, "/", sizeof(pathdone));
				continue;
			}
			err(1, "Fail to create /%s", pathdone);
			free(walkorig);
			return (false);
		}
		strlcat(pathdone, "/", sizeof(pathdone));
	}
	free(walkorig);
	return (true);
}

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
	struct stat st;

	fd = openat(dfd, name, O_RDONLY);
	if (fd == -1)
		err(1, "Impossible to open: '%s'", name);

	if (fstat(fd, &st) == -1) {
		warn("Fail to stat('%s')", name);
		close(fd);
		return (NULL);
	}
	f = fdopen(fd, "r");
	if (f == NULL)
		err(1, "Impossible to open stream");
	ar = calloc(1, sizeof(*ar));
	if (ar == NULL)
		err(1, "malloc");
	ar->creation = st.st_ctime;
	ar->modification = st.st_mtime;
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

static NEOERR *
cblog_write(void *ctx, char *s)
{
	int *fd = ctx;

	dprintf(*fd, "%s", s);

	return STATUS_OK;
}

static void
cblog_render(HDF *hdf, int tplfd, int outputfd, const char *type, const char *output)
{
	CSPARSE *parse;
	NEOERR *neoerr;
	char *cs = NULL;
	struct stat st;
	int fd;

	/* not capsicum friendly */
	cs_init(&parse, hdf);
	fd = openat(tplfd, type, O_RDONLY);
	if (fd == -1)
		err(1, "Unable to open templace: '%s'", type);
	if (fstat(fd, &st) == -1)
		err(1, "fstat()");
	cs = mmap(NULL, st.st_size, PROT_READ, MAP_SHARED, fd, 0);
	if (cs == MAP_FAILED)
		err(1, "mmap()");
	/* TODO don't know what yet cs_parse_string failes with a direct mmap */
	char *toto = strndup(cs, st.st_size );
	munmap(cs, st.st_size);
	close(fd);
	neoerr = cs_parse_string(parse, toto, strlen(toto));
	/* end of not capsicum friendly */
	fd = openat(outputfd, output, O_CREAT|O_WRONLY|O_TRUNC, 0644);
	if (fd == -1)
		err(1, "open");
	neoerr = cs_render(parse, &fd, cblog_write);
	close(fd);
}

static void
cblog_generate(struct article *articles, int tplfd, int outputfd, HDF *conf, int nbarticles)
{
	struct article *ar;
	int i = 0;
	HDF *idx;
	NEOERR *neoerr;
	int max_post = hdf_get_int_value(conf, "posts_per_pages", DEFAULT_POSTS_PER_PAGES);

	LL_FOREACH(articles, ar) {
		if ((i % max_post) == 0) {
			hdf_init(&idx);
			neoerr = hdf_copy(idx, "", conf);
			nerr_ignore(&neoerr);
		}

		hdf_set_valuef(idx, "CBlog.version=%s", cblog_version);
		hdf_set_valuef(idx, "CBlog.url=%s", cblog_url);

		if ((i % max_post) + 1 == max_post) {
			/* render */
			cblog_render(idx, tplfd, outputfd, "index.cs", "index.html");
		}
		i++;
	}
}

void
cblogctl_gen(HDF *conf)
{
	struct dirent *dp;
	struct article *articles = NULL;
	struct article *ar;
	int nb = 0;
	DIR *dir;
	int dbfd, tplfd, outputfd;
	const char *path = get_cblog_db(conf);
	cap_rights_t rights_read;

	dbfd = open(path, O_DIRECTORY);
	if (dbfd == -1)
		err(1, "Impossible to open the database directory '%s'", path);

	path = hdf_get_valuef(conf, "outputs");
	outputfd = open(path, O_DIRECTORY);
	if (outputfd == -1)
		err(1, "Unable to open the output directory: '%s'", path);

	path = hdf_get_valuef(conf, "templates");
	tplfd = open(path, O_DIRECTORY);
	if (tplfd == -1)
		err(1, "Unable to open the template directory: '%s'", path);

	caph_cache_catpages();
	if (caph_enter() < 0)
		err(1, "cap_enter");
	if (caph_limit_stdout() < 0 || caph_limit_stderr() < 0 || caph_limit_stdin() < 0)
		err(1, "caph_limit");
	cap_rights_init(&rights_read, CAP_READ, CAP_FSTAT, CAP_READ,CAP_LOOKUP, CAP_FCNTL, CAP_SEEK_TELL,CAP_MMAP_R);
	if (caph_rights_limit(dbfd, &rights_read)< 0 )
		err(1, "cap_right_limits");
	if (caph_rights_limit(tplfd, &rights_read)< 0 )
		err(1, "cap_right_limits");


	dir = fdopendir(dbfd);
	while ((dp = readdir(dir)) != NULL) {
		if (dp->d_namlen == 1 && strcmp(dp->d_name, ".") == 0)
			continue;
		if (dp->d_namlen == 2 && strcmp(dp->d_name, "..") == 0)
			continue;
		ar = parse_article(dbfd, dp->d_name);
		if (ar != NULL) {
			DL_APPEND(articles, ar);
			nb++;
		}
	}
	closedir(dir);

	cblog_generate(articles, tplfd, outputfd, conf, nb);
}

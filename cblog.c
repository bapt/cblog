#include <sys/mman.h>
#include <sys/param.h>
#include <sys/capsicum.h>

#include <ClearSilver.h>
#include <stdio.h>
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

#include "cblog.h"

int
splitchr(char *str, char sep)
{
	char	*next;
	char	*buf = str;
	int		nbel = 0;

	while ((next = strchr(buf, sep)) != NULL) {
		nbel++;
		buf = next;
		buf[0] = '\0';
		buf++;
	}

	return nbel;
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
	char *filename;
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
		if (*next == '\0') {
			strlcat(pathdone, "/", sizeof(pathdone));
			continue;
		}
		strlcat(pathdone, next, sizeof(pathdone));
		/* skip the final file */
		if (strcmp(pathdone, path) == 0)
			break;
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
	ar = xcalloc(1, sizeof(*ar));
	ar->creation = st.st_birthtime;
	ar->modification = st.st_mtime;
	ar->filename = xstrdup(name);
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
			ar->title = xstrdup(val);
		} else if (STARTS_WITH(line, "Tags: ")) {
			val = line + strlen("Tags: ");
			ar->tags = xstrdup(val);
		}
	}
	if (ar->content != NULL)
		bufnullterm(ar->content);

	free(line);
	fclose(f);
	if (ar->title == NULL || ar->content == NULL) {
		warnx("invalid file '%s', ignoring", ar->filename);
		free(ar->filename);
		free(ar);
		return (NULL);
	}
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
	STRING buf;

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
	if (neoerr != STATUS_OK) {
		string_init(&buf);
		nerr_error_string(neoerr, &buf);
		errx(1, "%s: %s", type, buf.buf);
	}
	/* end of not capsicum friendly */
	if (!mkdirat_p(outputfd, output))
		err(1, "mkdirat");
	fd = openat(outputfd, output, O_CREAT|O_WRONLY|O_TRUNC, 0644);
	if (fd == -1)
		err(1, "open('%s')", output);
	neoerr = cs_render(parse, &fd, cblog_write);
	cs_destroy(&parse);
	close(fd);
}

static void
cblog_generate(struct article *articles, int tplfd, int outputfd, HDF *conf, int nbarticles)
{
	struct article *ar;
	int i = 0;
	HDF *idx, *post;
	NEOERR *neoerr;
	int page = 0;
	int max_post = hdf_get_int_value(conf, "posts_per_pages", DEFAULT_POSTS_PER_PAGES);
	char buf[BUFSIZ];
	char *walk;
	struct buf *ob;
	char *output;

	ob = bufnew(BUFSIZ);
	int nb_pages = nbarticles / max_post;
	if (nbarticles % max_post > 0)
		nb_pages++;
	hdf_set_valuef(conf, "CBlog.version=%s", cblog_version);
	hdf_set_valuef(conf, "CBlog.url=%s", cblog_url);
	LL_FOREACH(articles, ar) {
		if ((i % max_post) == 0) {
			page++; /* pages starts with */
			hdf_init(&idx);
			neoerr = hdf_copy(idx, "", conf);
			nerr_ignore(&neoerr);
			hdf_set_valuef(idx, "nbpages=%i", nb_pages);
			hdf_set_valuef(idx, "page=%i", page);
		}

		/* Add to index */
		hdf_set_valuef(idx, "Posts.%i.filename=%s", i, ar->filename);
		hdf_set_valuef(idx, "Posts.%i.title=%s", i, ar->title);
		strftime(buf, sizeof(buf), "%Y/%m/%d", localtime(&ar->modification));
		hdf_set_valuef(idx, "Posts.%i.link=/%s/%s", i, buf, ar->filename);
		hdf_set_valuef(idx, "Posts.%i.date=%s", i, buf);
		bufreset(ob);
		markdown(ob, ar->content, &mkd_xhtml);
		bufnullterm(ob);
		walk = strstr(ob->data, "</p>");
		walk += 4;
		hdf_set_valuef(idx, "Posts.%i.html=%.*s", i, (int)(walk - ob->data), ob->data);

		/* Create post page */
		hdf_init(&post);
		neoerr = hdf_copy(post, "", conf);
		hdf_set_valuef(post, "Post.filename=%s", ar->filename);
		hdf_set_valuef(post, "Post.title=%s", ar->title);
		hdf_set_valuef(post, "Post.date=%s", buf);
		hdf_set_valuef(post, "Post.html=%s", ob->data);
		xasprintf(&output, "%s/%s/index.html", buf, ar->filename);
		cblog_render(post, tplfd, outputfd, "index.cs", output);
		free(output);
		xasprintf(&output, "post/%s/index.html", ar->filename);
		cblog_render(post, tplfd, outputfd, "index.cs", output);
		free(output);

		if (((i % max_post) + 1 == max_post) || ar->next == NULL) {
			/* render */
			if (page == 1)
				output = xstrdup("index.html");
			else
				xasprintf(&output, "page/%d/index.html", page);
			cblog_render(idx, tplfd, outputfd, "index.cs", output);
			free(output);
			hdf_destroy(&idx);
		}
		i++;
	}
	bufrelease(ob);
}

static int
date_cmp(struct article *a1, struct article *a2)
{
	return (a1->creation < a2->creation);
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

	DL_SORT(articles, date_cmp);

	cblog_generate(articles, tplfd, outputfd, conf, nb);
}

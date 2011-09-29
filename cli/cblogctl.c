#include <sys/stat.h>
#include <ctype.h>
#include <stdio.h>
#include <libgen.h>
#include "buffer.h"
#include "markdown.h"
#include "renderers.h"
#include <stdbool.h>
#include <stdlib.h>
#include <err.h>
#include <limits.h>
#include <cdb.h>
#include <unistd.h>
#include <fcntl.h>
#include "cblogctl.h"
#include "cblog_common.h"
#include "cblog_utils.h"

/* path the the CDB database file */
char	cblog_cdb[PATH_MAX];
char	cblog_cdb_tmp[PATH_MAX];

void
cblogctl_list(void)
{
	int					db;
	struct cdb			cdb;
	struct cdb_find		cdbf;
	char				*val;

	if ((db = open(cblog_cdb, O_RDONLY)) < 0)
		err(1, "%s", cblog_cdb);
	cdb_init(&cdb, db);

	cdb_findinit(&cdbf, &cdb, "posts", 5);
	while (cdb_findnext(&cdbf) > 0) {
		val=db_get(&cdb);
		puts(val);
		free(val);
	}

	cdb_free(&cdb);
	close(db);
}

int
trimcr(char *str)
{
	size_t len;

	if (str == NULL)
		return -1;

	len = strlen(str);
	while (len--)
		if ((str[len] == '\r') || (str[len] == '\n'))
			str[len] = '\0';

	return 0;
}

void
cblogctl_info(const char *post_name)
{
	int			i;
	int			db;
	char		key[BUFSIZ];
	char		*val;
	struct cdb	cdb;

	if ((db = open(cblog_cdb, O_RDONLY)) < 0)
		err(1, "%s", cblog_cdb);
	cdb_init(&cdb, db);

	printf("Informations about %s\n", post_name);
	for (i=0; field[i] != NULL; i++) {
		if (EQUALS(field[i], "source") || EQUALS(field[i], "html"))
			continue;

		snprintf(key, BUFSIZ, "%s_%s", post_name, field[i]);
		if (cdb_find(&cdb, key, strlen(key)) > 0) {
			val = db_get(&cdb);
			if (EQUALS(field[i], "ctime")) {
				time_to_str((time_t)strtoll(val, NULL, 10), "%Y/%m/%d %T", key, BUFSIZ);
				printf("- %s: %s\n", field[i], key);
			} else 
				printf("- %s: %s\n", field[i], val);
			free(val);
		}
	}
	printf("\n");
	cdb_free(&cdb);
	close(db);
}

void
cblogctl_get(const char *post_name)
{
	int			db;
	FILE		*out;
	char		key[BUFSIZ];
	char		*val;
	struct cdb	cdb;

	if ((db = open(cblog_cdb, O_RDONLY)) < 0)
		err(1, "%s", cblog_cdb);
	cdb_init(&cdb, db);

	out = fopen(post_name, "w");

	snprintf(key, BUFSIZ, "%s_%s", post_name, "title");
	if (cdb_find(&cdb, key, strlen(key)) > 0) {
		val = db_get(&cdb);
		fprintf(out, "Title: %s\n", val);
	} else {
		warnx("post %s not found", post_name);
		return;
	}

	snprintf(key, BUFSIZ, "%s_%s", post_name, "tags");
	if (cdb_find(&cdb, key, strlen(key)) > 0) {
		val=db_get(&cdb);
		fprintf(out, "Tags: %s\n", val);
		free(val);
	}

	fprintf(out, "\n");

	snprintf(key, BUFSIZ, "%s_%s", post_name,"source");
	if (cdb_find(&cdb, key, strlen(key)) > 0) {
		val = db_get(&cdb);
		fprintf(out, "%s\n", val);
		free(val);
	}

	fclose(out);
	cdb_free(&cdb);
	close(db);
}

void
cblogctl_add(const char *post_path)
{
	int					olddb, db, i;
	FILE				*post;
	char				key[BUFSIZ], date[11];
	char				*val, *valkey, *post_name;
	bool				found = false;
	struct cdb			cdb;
	struct cdb_make		cdb_make;
	struct cdb_find		cdbf;
	struct buf			*ib, *ob;
	char				filebuf[LINE_MAX];
	bool				headers = true;
	struct stat			filestat;
	char				*ppath;

	ppath = strdup(post_path);
	post_name = basename(ppath);
	post = fopen(post_path, "r");

	if (post == NULL)
		errx(EXIT_FAILURE, "Unable to open %s", post_name);

	if ((olddb = open(cblog_cdb, O_RDONLY)) < 0)
		err(1, "%s", cblog_cdb);
	if ((db = open(cblog_cdb_tmp, O_CREAT|O_RDWR|O_TRUNC, 0644)) < 0)
		err(1, "%s", cblog_cdb);

	cdb_init(&cdb, olddb);
	cdb_make_start(&cdb_make, db);

	/* First recopy the whole "posts" entry and determine if the post already exist or not */
	cdb_findinit(&cdbf, &cdb, "posts", 5);
	while (cdb_findnext(&cdbf) > 0) {
		valkey = db_get(&cdb);

		cdb_make_add(&cdb_make, "posts", 5, valkey, strlen(valkey));

		if (EQUALS(post_name, valkey))
			found = true;

		for (i=0; field[i] != NULL; i++) {
			snprintf(key, BUFSIZ, "%s_%s", valkey, field[i]);
			if (cdb_find(&cdb, key, strlen(key)) > 0) {
				val = db_get(&cdb);
				cdb_make_add(&cdb_make, key, strlen(key), val, strlen(val));
				free(val);
			}
		}
		free(valkey);
	}
	if (!found)
		cdb_make_add(&cdb_make, "posts", 5, post_name, strlen(post_name));

	ib = bufnew(BUFSIZ);

	while (fgets(filebuf, LINE_MAX, post) != NULL) {
		if (filebuf[0] == '\n' && headers) {
			headers = false;
			continue;
		}

		if (headers) {
			trimcr(filebuf);
			if (STARTS_WITH(filebuf, "Title: ")) {
				while (isspace(filebuf[strlen(filebuf) - 1]))
					filebuf[strlen(filebuf) - 1] = '\0';

				snprintf(key, BUFSIZ, "%s_title", post_name);
				val = filebuf + strlen("Title: ");
				cdb_make_put(&cdb_make, key, strlen(key), val, strlen(val), CDB_PUT_REPLACE);

			} else if (STARTS_WITH(filebuf, "Tags")) {
				while (isspace(filebuf[strlen(filebuf) - 1]))
					filebuf[strlen(filebuf) - 1] = '\0';

				val = filebuf + strlen("Tags: ");
				snprintf(key, BUFSIZ, "%s_tags", post_name);
				cdb_make_put(&cdb_make, key, strlen(key), val, strlen(val), CDB_PUT_REPLACE);
			}
		} else
			bufputs(ib, filebuf);
	}
	fclose(post);

	stat(post_path, &filestat);
	snprintf(date, 11, "%lld", (long long int)filestat.st_mtime);
	snprintf(key, BUFSIZ, "%s_ctime", post_name);
	cdb_make_put(&cdb_make, key, strlen(key), date, strlen(date), CDB_PUT_INSERT);

	ob = bufnew(BUFSIZ);
	markdown(ob, ib, &mkd_xhtml);
	bufnullterm(ob);
	bufnullterm(ib);

	snprintf(key, BUFSIZ, "%s_source", post_name);
	cdb_make_put(&cdb_make, key, strlen(key), ib->data, strlen(ib->data), CDB_PUT_REPLACE);
	bufrelease(ib);

	snprintf(key, BUFSIZ, "%s_html", post_name);
	cdb_make_put(&cdb_make, key, strlen(key), ob->data, strlen(ob->data), CDB_PUT_REPLACE);
	bufrelease(ob);

	cdb_make_finish(&cdb_make);
	close(olddb);
	cdb_free(&cdb);
	close(db);
	if (rename(cblog_cdb_tmp, cblog_cdb) < 0)
		err(1, "%s", cblog_cdb);

	free(ppath);
}

void
cblogctl_del(const char *post_name)
{
	int					olddb, db, i;
	struct cdb			cdb;
	struct cdb_make		cdb_make;
	struct cdb_find		cdbf;
	char				*val, *valkey;
	char				key[BUFSIZ];

	if ((olddb = open(cblog_cdb, O_RDONLY)) < 0)
		err(1, "%s", cblog_cdb);
	if ((db = open(cblog_cdb_tmp, O_CREAT|O_RDWR|O_TRUNC, 0644)) < 0)
		err(1, "%s", cblog_cdb);

	cdb_init(&cdb, olddb);
	cdb_make_start(&cdb_make, db);

	cdb_findinit(&cdbf, &cdb, "posts", 5);
	while (cdb_findnext(&cdbf) > 0) {
		valkey = db_get(&cdb);

		if (EQUALS(post_name, valkey))
			continue;
		cdb_make_add(&cdb_make, "posts", 5, valkey, strlen(valkey));

		for (i=0; field[i] != NULL; i++) {
			snprintf(key, BUFSIZ, "%s_%s", valkey, field[i]);
			if (cdb_find(&cdb, key, strlen(key)) > 0) {
				val = db_get(&cdb);
				cdb_make_add(&cdb_make, key, strlen(key), val, strlen(val));
				free(val);
			}
		}
		free(valkey);
	}
	cdb_make_finish(&cdb_make);
	close(olddb);
	cdb_free(&cdb);
	close(db);
	if (rename(cblog_cdb_tmp, cblog_cdb) < 0)
		err(1, "%s", cblog_cdb);
}

void
cblogctl_set(const char *post_name, char *to_be_set)
{
	int					olddb, db, i;
	char				key[BUFSIZ];
	char				*val, *newkey, *valkey;
	struct cdb			cdb;
	struct cdb_make		cdb_make;
	struct cdb_find		cdbf;
	bool				found = false;

	if ((olddb = open(cblog_cdb, O_RDONLY)) < 0)
		err(1, "%s", cblog_cdb);
	if ((db = open(cblog_cdb_tmp, O_CREAT|O_RDWR|O_TRUNC, 0644)) < 0)
		err(1, "%s", cblog_cdb);

	cdb_init(&cdb, olddb);
	cdb_make_start(&cdb_make, db);

	cdb_findinit(&cdbf, &cdb, "posts", 5);
	while (cdb_findnext(&cdbf) > 0) {
		valkey = db_get(&cdb);
		cdb_make_add(&cdb_make, "posts", 5, valkey, strlen(valkey));

		if (EQUALS(post_name, valkey))
			found = true;

		for (i=0; field[i] != NULL; i++) {
			snprintf(key, BUFSIZ, "%s_%s", valkey, field[i]);
			if (cdb_find(&cdb, key, strlen(key)) > 0) {
				val = db_get(&cdb);
				cdb_make_add(&cdb_make, key, strlen(key), val, strlen(val));
				free(val);
			}
		}
		free(valkey);
	}
	if (!found)
		errx(EXIT_FAILURE, "%s: No such post", post_name);

	newkey = to_be_set;
	while (to_be_set[0] != '=')
		to_be_set++;

	to_be_set[0] = '\0';
	to_be_set++;

	snprintf(key, BUFSIZ, "%s_%s", post_name, newkey);

	cdb_make_put(&cdb_make, key, strlen(key), to_be_set, strlen(to_be_set), CDB_PUT_REPLACE);

	cdb_make_finish(&cdb_make);
	cdb_free(&cdb);
	close(db);
	close(olddb);

	if (rename(cblog_cdb_tmp, cblog_cdb) < 0)
		err(1, "%s", cblog_cdb);
}

void
cblogctl_create(void)
{
	int					db;
	struct cdb_make		cdb_make;

	if (access(cblog_cdb, F_OK) == 0)
		errx(1, "%s already exists", cblog_cdb);
	if ((db = open(cblog_cdb, O_CREAT|O_RDWR|O_TRUNC, 0644)) < 0)
		err(1, "%s", cblog_cdb);

	cdb_make_start(&cdb_make, db);
	cdb_make_finish(&cdb_make);
	close(db);
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

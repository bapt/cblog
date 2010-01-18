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

void
cblogctl_list()
{
	int db;
	unsigned int vlen, vpos;
	struct cdb cdb;
	struct cdb_find cdbf;
	char *val;

	db = open(CACHE"/cblog.cdb", O_RDONLY);
	cdb_init(&cdb, db);

	cdb_findinit(&cdbf, &cdb, "posts", 5);
	while (cdb_findnext(&cdbf) > 0) {
		vpos = cdb_datapos(&cdb);
		vlen = cdb_datalen(&cdb);

		XMALLOC(val, vlen);
		val[vlen] = '\0';

		cdb_read(&cdb, val, vlen, vpos);
		puts(val);
		XFREE(val);
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
	int i;
	int db;
	char *key;
	char *val;
	struct cdb cdb;
	unsigned int vlen, vpos;

	db = open(CACHE"/cblog.cdb", O_RDONLY);
	cdb_init(&cdb, db);

	printf("Informations about %s\n", post_name);
	for (i=0; field[i] != NULL; i++) {
		if (EQUALS(field[i], "source") || EQUALS(field[i], "html"))
			continue;

		asprintf(&key, "%s_%s", post_name, field[i]);
		if (cdb_find(&cdb, key, strlen(key)) > 0) {
			vpos = cdb_datapos(&cdb);
			vlen = cdb_datalen(&cdb);
			XMALLOC(val, vlen);
			cdb_read(&cdb, val, vlen, vpos);
			val[vlen] = '\0';
			printf("- %s: %s\n", field[i], val);
			XFREE(val);
		}
		XFREE(key);
	}
	printf("\n");
	cdb_free(&cdb);
	close(db);
}

void
cblogctl_get(const char *post_name)
{
	int db;
	FILE *out;
	char *key;
	char *val;
	struct cdb cdb;
	unsigned int vlen, vpos;

	db = open(CACHE"/cblog.cdb", O_RDONLY);
	cdb_init(&cdb, db);

	out = fopen(post_name, "w");

	asprintf(&key, "%s_%s", post_name, "title");
	if (cdb_find(&cdb, key, strlen(key)) > 0) {
		vpos = cdb_datapos(&cdb);
		vlen = cdb_datalen(&cdb);
		XMALLOC(val, vlen);
		cdb_read(&cdb, val, vlen, vpos);
		val[vlen] = '\0';
		fprintf(out, "Title: %s\n", val);
	} else {
		warnx("post %s not found", post_name);
		XFREE(key);
		return;
	}
	XFREE(key);

	asprintf(&key, "%s_%s", post_name, "tags");
	if (cdb_find(&cdb, key, strlen(key)) > 0) {
		vpos = cdb_datapos(&cdb);
		vlen = cdb_datalen(&cdb);
		XMALLOC(val, vlen);
		cdb_read(&cdb, val, vlen, vpos);
		val[vlen] = '\0';
		fprintf(out, "Tags: %s\n", val);
	}
	XFREE(key);

	fprintf(out, "\n");

	asprintf(&key, "%s_%s", post_name,"source");
	if (cdb_find(&cdb, key, strlen(key)) > 0) {
		vpos = cdb_datapos(&cdb);
		vlen = cdb_datalen(&cdb);
		XMALLOC(val, vlen);
		cdb_read(&cdb, val, vlen, vpos);
		val[vlen] = '\0';
		fprintf(out, "%s\n", val);
	}
	XFREE(key);
	fclose(out);
	cdb_free(&cdb);
	close(db);
}

void
cblogctl_add(const char *post_path)
{
	int olddb, db, i;
	FILE *post;
	char *key, *val, *valkey, *date, *post_name;
	bool found = false;
	unsigned int vlen, vpos;
	struct cdb cdb;
	struct cdb_make cdb_make;
	struct cdb_find cdbf;
	struct buf *ib, *ob;
	char filebuf[LINE_MAX];
	bool headers = true;
	struct stat filestat;

	post_name = basename(post_path);
	post = fopen(post_path, "r");

	if (post == NULL)
		errx(EXIT_FAILURE, "Unable to open %s", post_name);

	olddb = open(CACHE"/cblog.cdb", O_RDONLY);
	db = open(CACHE"/cblogtmp.cdb", O_CREAT|O_RDWR|O_TRUNC, 0644);

	cdb_init(&cdb, olddb);
	cdb_make_start(&cdb_make, db);

	/* First recopy the whole "posts" entry and determine if the post already exist or not */
	cdb_findinit(&cdbf, &cdb, "posts", 5);
	while (cdb_findnext(&cdbf) > 0) {
		vpos = cdb_datapos(&cdb);
		vlen = cdb_datalen(&cdb);

		XMALLOC(valkey, vlen);
		cdb_read(&cdb, valkey, vlen, vpos);
		valkey[vlen] = '\0';
		puts(valkey);
		cdb_make_add(&cdb_make, "posts", 5, valkey, strlen(valkey));

		if (EQUALS(post_name, valkey))
			found = true;

		for (i=0; field[i] != NULL; i++) {
			asprintf(&key, "%s_%s", valkey, field[i]);
			if (cdb_find(&cdb, key, strlen(key)) > 0) {
				vpos = cdb_datapos(&cdb);
				vlen = cdb_datalen(&cdb);
				XMALLOC(val, vlen);
				cdb_read(&cdb, val, vlen, vpos);
				val[vlen] = '\0';
				cdb_make_add(&cdb_make, key, strlen(key), val, vlen);
				XFREE(val);
			}
			XFREE(key);
		}
		XFREE(valkey);
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

				asprintf(&key, "%s_title", post_name);
				val = filebuf + strlen("Title :");
				cdb_make_put(&cdb_make, key, strlen(key), val, strlen(val), CDB_PUT_REPLACE);
				XFREE(key);
			} else if (STARTS_WITH(filebuf, "Tags")) {
				while (isspace(filebuf[strlen(filebuf) - 1]))
					filebuf[strlen(filebuf) - 1] = '\0';

				val = filebuf + strlen("Tags: ");
				asprintf(&key, "%s_tags", post_name);
				cdb_make_put(&cdb_make, key, strlen(key), val, strlen(val), CDB_PUT_REPLACE);
				XFREE(key);
			}
		} else
			bufputs(ib, filebuf);
	}
	fclose(post);

	stat(post_path, &filestat);
	asprintf(&date, "%lld", (long long int)filestat.st_mtime);
	asprintf(&key, "%s_ctime", post_name);
	cdb_make_put(&cdb_make, key, strlen(key), date, strlen(val), CDB_PUT_INSERT);
	XFREE(date);
	XFREE(key);

	ob = bufnew(BUFSIZ);
	markdown(ob, ib, &mkd_xhtml);
	bufnullterm(ob);
	bufnullterm(ib);

	asprintf(&key, "%s_source", post_name);
	cdb_make_put(&cdb_make, key, strlen(key), ib->data, strlen(ib->data), CDB_PUT_REPLACE);
	XFREE(key);
	bufrelease(ob);

	asprintf(&key, "%s_html", post_name);
	cdb_make_put(&cdb_make, key, strlen(key), ob->data, strlen(ob->data), CDB_PUT_REPLACE);
	XFREE(key);
	bufrelease(ob);

	cdb_make_finish(&cdb_make);
	close(olddb);
	cdb_free(&cdb);
	close(db);
	rename( CACHE"/cblogtmp.cdb", CACHE"/cblog.cdb");
}

void
cblogctl_set(const char *post_name, char *to_be_set)
{
	int olddb, db, i;
	char *key, *val, *newkey, *valkey;
	unsigned int vlen, vpos;
	struct cdb cdb;
	struct cdb_make cdb_make;
	struct cdb_find cdbf;
	bool found = false;


	olddb = open(CACHE"/cblog.cdb", O_RDONLY);
	db = open(CACHE"/cblogtmp.cdb", O_CREAT|O_RDWR|O_TRUNC, 0644);

	cdb_init(&cdb, olddb);
	cdb_make_start(&cdb_make, db);

	cdb_findinit(&cdbf, &cdb, "posts", 5);
	while (cdb_findnext(&cdbf) > 0) {
		vpos = cdb_datapos(&cdb);
		vlen = cdb_datalen(&cdb);

		XMALLOC(valkey, vlen);
		cdb_read(&cdb, valkey, vlen, vpos);
		valkey[vlen] = '\0';
		cdb_make_add(&cdb_make, "posts", 5, valkey, strlen(valkey));

		if (EQUALS(post_name, valkey))
			found = true;

		for (i=0; field[i] != NULL; i++) {
			asprintf(&key, "%s_%s", valkey, field[i]);
			if (cdb_find(&cdb, key, strlen(key)) > 0) {
				vpos = cdb_datapos(&cdb);
				vlen = cdb_datalen(&cdb);
				XMALLOC(val, vlen);
				cdb_read(&cdb, val, vlen, vpos);
				val[vlen] = '\0';
				cdb_make_add(&cdb_make, key, strlen(key), val, vlen);
				XFREE(val);
			}
			XFREE(key);
		}
		XFREE(valkey);
	}
	if (!found)
		errx(EXIT_FAILURE, "%s: No such post", post_name);

	newkey = to_be_set;
	while (to_be_set[0] != '=')
		to_be_set++;
	to_be_set[0] = '\0';
	to_be_set++;

	asprintf(&key, "%s_%s", post_name, newkey);
	puts(to_be_set);
	cdb_make_put(&cdb_make, key, strlen(key), to_be_set, strlen(to_be_set), CDB_PUT_REPLACE);
	XFREE(key);

	cdb_make_finish(&cdb_make);
	cdb_free(&cdb);
	close(db);
	close(olddb);
	rename(CACHE"/cblogtmp.cdb", CACHE"/cblog.cdb");
}

/* vim: set sw=4 sts=4 ts=4 : */

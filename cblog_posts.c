#include <sys/types.h>
#include <sys/stat.h>
#include <limits.h>
#include <sys/queue.h>
#include <fts.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <ClearSilver.h>
#include <ctype.h>

#include "cblog_posts.h"
#include "cblog_conf.h"
#include "cblog_log.h"
#include "utils.h"
#include "markdown.h"
#include "renderers.h"
#include <cdb.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/queue.h>

static char *field[] = {
	"title",
	"tags",
	"source",
	"html",
	"ctime",
	NULL
};

char *feed;

struct posts {
	char *name;
	time_t ctime;
};


int
sort_tags_by_name(const void *a, const void *b)
{
	HDF **ha = (HDF **)a;
	HDF **hb = (HDF **)b;
	return strcasecmp(hdf_get_valuef(*ha, "name"), hdf_get_valuef(*hb,"name"));
}

long long
get_ctime_from_index(HDF *hdf, char * filename)
{
	char *index;
	FILE *cache;
	char buf[LINE_MAX];
	long long ret=0;

	asprintf(&index, "%s/entries.index",hdf_get_valuef(hdf, "cache.dir"));
	if (!file_exists(index)) {
		XFREE(index);
		return ret;
	}

	cache = fopen(index,"r");
	while ( fgets(buf, LINE_MAX, cache ) != NULL) {
		if (STARTS_WITH(buf, filename)) {
			char ** fields = splitstr(buf, "|");
			ret = (long long) strtol(fields[1],NULL, 10);
			free_list(fields);
			break;
		}
	}
	return ret;
	XFREE(index);
}

void
cache_post(struct cdb_make *cache, const char *path, const char *filename)
{
	struct buf *ib, *ob;
	char *key, *val;
	char filebuf[LINE_MAX];
	FILE *post_txt;
	bool headers=true;

	/* Remove the previous cache entry if exists */
	post_txt = fopen(path, "r");
	ib = bufnew(READ_UNIT);

	cdb_make_add(cache, "posts", 5, filename, strlen(filename));
	while(fgets(filebuf, LINE_MAX, post_txt) != NULL) {
		/* The first empty line in the file is the end of the header */
		if (filebuf[0] == '\n' && headers) {
			headers=false;
			continue;
		}
		if (filebuf[strlen(filebuf) - 1 ] == '\n')
			filebuf[strlen(filebuf) - 1] = '\0';
		if (headers) {
			if (STARTS_WITH(filebuf, "Title: "))  {
				asprintf(&key, "%s_title", filename);
				val = filebuf + strlen("Title ");
				cdb_make_add(cache, key, strlen(key), val, strlen(val));
				XFREE(key);
				continue;
			} else if (STARTS_WITH(filebuf, "Comments: ")) {
				/* TODO:  Add this to the post
				 * hdf_set_valuef(global_cache, "Posts.%i.allow_comments=%s", pos,  filebuf + strlen("Comments: "));
				 */
				continue;
			} else if (STARTS_WITH(filebuf, "Tags: ")) {
				/* remove trailling spaces */ 
				while (isspace(filebuf[strlen(filebuf) - 1]))
					filebuf[strlen(filebuf) - 1]='\0';

				val = filebuf + strlen("Tags: ");
				asprintf(&key, "%s_tags", filename);
				cdb_make_add(cache, key, strlen(key), val, strlen(val));
				XFREE(key);
			}	
		} else {
			bufputs(ib, filebuf);
			bufputs(ib,"\n");
		}
	}
	fclose(post_txt);
	/* convert to html and keep the source */
	ob = bufnew(OUTPUT_UNIT);
	markdown(ob, ib, &mkd_xhtml);
	bufnullterm(ob);
	bufnullterm(ib);
	char *html;
	char *source;
	html= ob->data;
	source= ib->data;
	asprintf(&key, "%s_source", filename);
	cdb_make_add(cache, key, strlen(key), source, strlen(source));
	XFREE(key);

	asprintf(&key, "%s_html", filename);
	cdb_make_add(cache, key, strlen(key), html, strlen(html));
	XFREE(key);

	bufrelease(ib);
	bufrelease(ob);
}

void
update_cache(HDF *hdf)
{
	int cache_file;
	int new_cache;
	int i;
	struct cdb oldcache;
	struct cdb_make newcache;
	cdbi_t vlen, vpos;
	time_t data_mtime;
	FTS *fts;
	FTSENT *ent;
	char *postpath[2];
	char *str_mtime;
	char *cache_path;

	postpath[0]=hdf_get_valuef(hdf, "data.dir");
	postpath[1] = NULL;

	asprintf(&cache_path, "%s/cblog.cdb", hdf_get_valuef(hdf, "cache.dir"));

	if ( !file_exists(cache_path) )
		data_mtime=0;
	else {
		cache_file = open(cache_path, O_RDONLY);
		if (cdb_seek(cache_file, "mtime", 5, &vlen) > 0 ) {
			XMALLOC(str_mtime, vlen);
			cdb_bread(cache_file, str_mtime, vlen);
			str_mtime[vlen] = '\0';
			data_mtime = (time_t)strtol(str_mtime, (char **)NULL, 10);
			XFREE(str_mtime);
		}
		close(cache_file);
	}

	struct stat datadirstat;
	stat(postpath[0],&datadirstat);


	if (data_mtime != datadirstat.st_mtime) {
		data_mtime=datadirstat.st_mtime;
		char *tmpcache_path;

		asprintf(&tmpcache_path, "%s/cblog-tmp.cdb", hdf_get_valuef(hdf, "cache.dir"));
		cache_file = open(cache_path, O_RDONLY);
		new_cache = open(cache_path, O_RDWR|O_CREAT|O_TRUNC, 0644);
		cdb_init(&oldcache, cache_file);
		cdb_make_start(&newcache, new_cache);
		
		asprintf(&str_mtime, "%lld", (long long) datadirstat.st_mtime);
		cdb_make_add(&newcache, "mtime", 5, str_mtime, strlen(str_mtime));
		XFREE(str_mtime);

		fts = fts_open(postpath, FTS_LOGICAL, NULL);
		while ((ent = fts_read(fts)) != NULL) {
			char *name;
			char *filename;
			struct stat filestat;

			if (ent->fts_info != FTS_F)
				continue;

			name=ent->fts_name;
			filename= ent->fts_name;
			name += strlen(ent->fts_name) - 4;

			if (!EQUALS(name, ".txt"))
				continue;

			stat(ent->fts_accpath, &filestat );
			filename[strlen(filename) - 4 ]='\0';

			if (cdb_find(&oldcache, filename, strlen(filename)) > 0) {
				vpos = cdb_datapos(&oldcache);
				vlen = cdb_datalen(&oldcache);
				XMALLOC(str_mtime, vlen);
				cdb_read(&oldcache, str_mtime, vlen, vpos);

				if (filestat.st_mtime > (time_t) strtol(str_mtime, (char **)NULL, 10) ) {
					cache_post(&newcache, ent->fts_accpath, filename);
					cdb_make_add(&newcache, filename, strlen(filename), str_mtime, strlen(str_mtime));
				} else {
					cdb_make_add(&newcache, filename, strlen(filename), str_mtime, strlen(str_mtime));

					for (i=0; field[i] != NULL; i++) {
						char *key, *tmp;
						asprintf(&key, "%s_%s", filename, field[i]);
						if (cdb_find(&oldcache, key, strlen(key)) > 0) {
							vpos = cdb_datapos(&oldcache);
							vlen = cdb_datalen(&oldcache);
							XMALLOC(tmp, vlen);
							cdb_read(&oldcache, tmp, vlen, vpos);
							tmp[vlen]='\0';
							cdb_make_add(&newcache, key, strlen(key), tmp, strlen(tmp));
						}
						XFREE(tmp);
						XFREE(key);
					}
				}
				XFREE(str_mtime);
			} else {
				char *key;
				long long ctime;
				cache_post(&newcache, ent->fts_accpath, filename);
				asprintf(&key, "%s_ctime", filename);
				if ( (ctime = get_ctime_from_index(hdf, filename)) == 0)
					asprintf(&str_mtime, "%lld", (long long) filestat.st_mtime);
				else 
					asprintf(&str_mtime, "%lld", ctime);
				cdb_make_add(&newcache, key, strlen(key), str_mtime, strlen(str_mtime));
				cdb_make_add(&newcache, filename, strlen(filename), str_mtime, strlen(str_mtime));
				XFREE(key);
				XFREE(str_mtime);
			}
		}
		fts_close(fts);
		cdb_make_finish(&newcache);
		close(new_cache);
		close(cache_file);
		
		rename(tmpcache_path, cache_path);
		XFREE(tmpcache_path);
	}
	XFREE(cache_path);
	XFREE(postpath[0]);
}

int
sort_by_ctime(const void *a, const void *b)
{
	struct posts *post_a = *((struct posts **)a);
	struct posts *post_b = *((struct posts **)b);

	return post_b->ctime - post_a->ctime;
}

void
add_post_to_hdf(HDF *hdf, struct cdb *cdb, char *name, int pos)
{
	char *date_format;
	int i, j;
	char *key;
	char *val;
	unsigned int vlen, vpos;

	hdf_set_valuef(hdf, "Posts.%i.filename=%s", pos, name);
	for (i=0; field[i] != NULL; i++) {
		asprintf(&key,"%s_%s", name, field[i]);
		cdb_find(cdb, key, strlen(key));
		vlen = cdb_datalen(cdb);
		vpos = cdb_datapos(cdb);
		XMALLOC(val, vlen);
		cdb_read(cdb, val, vlen, vpos);
		val[vlen] = '\0';
		XFREE(key);

		if (EQUALS(field[i], "tags")) {
			char **tags = splitstr(val, ", ");
			for (j=0; tags[j] != NULL; j++)
				hdf_set_valuef(hdf, "Posts.%i.tags.%i.name=%s", pos, j, tags[j]);

			free_list(tags);
		} else if (EQUALS(field[i], "ctime")) {
			date_format = get_dateformat(hdf);
			char * time_str = time_to_str((time_t) strtol(val,NULL, 10), date_format);
			set_post_date(hdf, pos, time_str);
			XFREE(time_str);
		} else
			hdf_set_valuef(hdf, "Posts.%i.%s=%s", pos, field[i], val);
		XFREE(val);
	}
}
void
hdf_fill(HDF *hdf, char *tag, char *name)
{
	int i, a, j;
	int cache;
	struct cdb cdb;
	int posts_per_pages = hdf_get_int_value(hdf, "posts_per_pages", 1);
	int post_min=0;
	int page = hdf_get_int_value(hdf,"Query.page",1);
	int nb_pages=0;
	int nb_posts=0;
	char *cache_file, *key, *val;
	unsigned int vlen, vpos;

	asprintf(&cache_file, "%s/cblog.cdb", hdf_get_valuef(hdf, "cache.dir"));

	post_min = (page * posts_per_pages) - posts_per_pages;

	cache = open(cache_file, O_RDONLY);
	
	cdb_init(&cdb, cache);

	if (name != NULL) {
		cblog_err(-1, "%s", name);
		if (cdb_find(&cdb, name, strlen(name)) > 0 ) {
			add_post_to_hdf(hdf, &cdb, name, 0);
		}
	} else {
		/* browser by tag */
		struct posts **posts = NULL;

		struct cdb_find cdbf;
		int nb_posts_total=0;
		cdb_findinit(&cdbf, &cdb, "posts", 5);
		while (cdb_findnext(&cdbf) >0) {
			nb_posts_total++;
			struct posts *post = NULL;

			XREALLOC(posts, nb_posts_total * sizeof(struct posts *));
			XMALLOC(post, sizeof(struct posts));

			vpos = cdb_datapos(&cdb);
			vlen = cdb_datalen(&cdb);
			XMALLOC(val, vlen);
			cdb_read(&cdb, val, vlen, vpos);
			XSTRDUP(post->name, val);
			post->name[vlen] = '\0';
			XFREE(val);

			asprintf(&key, "%s_ctime", post->name);
			cdb_find(&cdb, key, strlen(key));
			vpos = cdb_datapos(&cdb);
			vlen = cdb_datalen(&cdb);
			XMALLOC(val, vlen);
			cdb_read(&cdb, val, vlen, vpos);
			val[vlen] = '\0';
			post->ctime = (time_t) strtol(val,NULL, 10);
			XFREE(val);
			XFREE(key);
			posts[nb_posts_total - 1] = post;
		}

		qsort(posts, nb_posts_total, sizeof(struct posts *), sort_by_ctime);

		if (tag != NULL) {
			for (i=0; i < nb_posts_total; i++) {
				char **tags;

				asprintf(&key, "%s_tags", posts[i]->name);
				cdb_find(&cdb, key, strlen(key));
				vpos = cdb_datapos(&cdb);
				vlen = cdb_datalen(&cdb);
				XMALLOC(val, vlen);
				cdb_read(&cdb, val, vlen, vpos);
				val[vlen] = '\0';

				tags = splitstr(val,", ");
				for(a=0; tags[a] != NULL; a++) {
					if(EQUALS(tag, tags[a])) {
						j++;
						if(( j >= post_min) && (nb_posts < posts_per_pages)) {
							add_post_to_hdf(hdf, &cdb, posts[i]->name, j);
							nb_posts++;
						}
						break;
					}
				}
				free_list(tags);
				XFREE(val);
			}
		} else {
			/* global page */
			j=0;
			for (i=0; i < nb_posts_total; i++) {
				j++;
				if(( j >= post_min) && (nb_posts < posts_per_pages)) {
					add_post_to_hdf(hdf, &cdb, posts[i]->name, j);
					nb_posts++;
				}
			}
		}
		nb_pages = j / posts_per_pages;
		if (j % posts_per_pages > 0)
			nb_pages++;
		set_nb_pages(hdf, nb_pages);
		for (i=0; i< nb_posts_total; i++) {
			XFREE(posts[i]->name);
			XFREE(posts[i]);
		}
		XFREE(posts);
	}
	XFREE(cache_file);
	return;
}

void
set_tags(HDF *hdf)
{
	bool found=false;
	int nbtags=0;
	char *cache_file;
	struct cdb cdb;
	unsigned vlen, vpos;
	char *key, *val;
	int cache, i;
	HDF *tags_hdf;

	asprintf(&cache_file, "%s/cblog.cdb", hdf_get_valuef(hdf, "cache.dir"));
	cache = open(cache_file, O_RDONLY);
	cdb_init(&cdb, cache);

	struct cdb_find cdbf;
	cdb_findinit(&cdbf, &cdb, "posts", 5);
	while (cdb_findnext(&cdbf) >0) {
		vpos = cdb_datapos(&cdb);
		vlen = cdb_datalen(&cdb);
		XMALLOC(val, vlen);
		cdb_read(&cdb, val, vlen, vpos);
		val[vlen] = '\0';
		asprintf(&key, "%s_tags", val);
		XFREE(val);

		cdb_find(&cdb, key, strlen(key));
		vlen = cdb_datalen(&cdb);
		vpos = cdb_datapos(&cdb);
		XMALLOC(val, vlen);
		cdb_read(&cdb, val, vlen, vpos);
		val[vlen] = '\0';
		XFREE(key);

		char **tags = splitstr(val, ", ");
		for (i=0; tags[i] != NULL; i++) {
			found=false;
			HDF_FOREACH(tags_hdf, hdf, "Tags") {
				if (EQUALS(tags[i], hdf_get_valuef(tags_hdf, "name"))) {
					found=true;
					hdf_set_valuef(tags_hdf, "count=%i", hdf_get_int_value(tags_hdf, "count",0) + 1);
				}
			}
			if (!found) {
				nbtags++;
				set_tag_name(hdf, nbtags, tags[i]);
				set_tag_count(hdf, nbtags, 1);
			}
		}
		free_list(tags);
		hdf_sort_obj(hdf_get_obj(hdf, "Tags"), sort_tags_by_name);
		XFREE(val);
	}
	close(cache);
	XFREE(cache_file);
}

void
get_posts(HDF *hdf, char *tag, char *name)
{
	update_cache(hdf);
	hdf_fill(hdf, tag, name);
	set_tags(hdf);
}

void
posts_cleanup()
{
	return;
}

/* 
 * Copyright (c) 2009, Bapt <bapt@etoilebsd.net>
 * All rights reserved.
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the author nor the names of any co-contributors
 *       may be used to endorse or promote products derived from this software 
 *       without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE REGENTS AND CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
      * * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#include "cblog.h"

typedef struct Posts {
	char *filename;
	int order;
	time_t date;
	time_t mdate;
	bool cached;
	SLIST_ENTRY(Posts) next;
} Posts;

typedef struct Tags {
	char *name;
	int count;
	SLIST_ENTRY(Tags) next;
} Tags;

const char *data_dir;
const char *template_dir;
const char *cache_dir;
char *feed;
int page, pages;
int posts_per_pages;
int total_posts;
int nb_posts;
int post_matching;
Posts **post_sorted;
SLIST_HEAD(, Posts) posthead;
SLIST_HEAD(, Tags) tagshead;

void
hdf_set_tags(HDF *hdf)
{
	Tags *tag;
	int i = 0;
	SLIST_FOREACH(tag, &tagshead, next) {
		hdf_set_valuef(hdf, "Tags.%i.name=%s", i, tag->name);
		hdf_set_valuef(hdf, "Tags.%i.count=%i", i, tag->count);
		i++;
	}
}

void
parse_file(HDF *hdf, Posts *post, char *str, int type)
{
	STRING post_mkd;
	string_init(&post_mkd);
	FILE *post_txt;
	char *buf, *linebuf, *lbuf, *post_title = NULL;
	bool headers = true;
	size_t len;
	bool on_page = false;
	int i;
	int post_min = (page * posts_per_pages) - posts_per_pages;

	if (type == TYPE_DATE) { 
		post_matching++;
		if ((post->order >= post_min) && (nb_posts < posts_per_pages)) {
			nb_posts++;
			on_page = true;
		}
	}
	
	if (type == TYPE_POST ) {
		if (!strcmp(post->filename, str)) {
			post_matching++;
			if (nb_posts < posts_per_pages) {
				on_page = true;
				nb_posts++;
			}
		}
	}
	asprintf(&buf, "%s/%s", data_dir, post->filename);
	post_txt = fopen(buf, "r");
	
	lbuf = NULL;
	while (feof(post_txt) == 0) {
		while ((linebuf = fgetln(post_txt, &len))) {
			if (linebuf[len - 1] == '\n') 
				linebuf[len - 1 ] = '\0';
			else {
				XMALLOC(lbuf, len + 1);
				memcpy(lbuf, linebuf, len);
				lbuf[len] = '\0';
				linebuf = lbuf;
			}
			/* if we find a empty line, we leave the headers */
			if (strlen(linebuf) == 0 && headers) {
				headers = false;
				continue;
			}
			if (headers) {
				/* Parse the headers */
				if (STARTS_WITH(linebuf, "Title: ")) {
					XSTRDUP(post_title, linebuf + strlen("Title: "));
				} else if (STARTS_WITH(linebuf, "Tags: ")) {
					char **tags = splitstr(linebuf + strlen("Tags: "), ", ");

					if (tags == NULL) 
						return;

					if (type == TYPE_TAG)
						for (i = 0; tags[i] != NULL; i++) 
							if (!strcmp(tags[i], str)) {
								post_matching++;
								if ((nb_posts < posts_per_pages) && (post_matching >= post_min)) {
									on_page = true;
									nb_posts++;
								}
							}

					for (i = 0; tags[i] != NULL; i++) {
						Tags *tag;
						bool found = false;

						SLIST_FOREACH(tag, &tagshead, next) {
							if (strcmp(tag->name, tags[i]) == 0) {
								tag->count++;
								found = true;
								break;
							}
						}
						if (!found) {
							Tags *new;
							XMALLOC(new, sizeof(Tags));
							XSTRDUP(new->name, tags[i]);
							new->count = 1;
							SLIST_INSERT_HEAD(&tagshead, new, next);
						}
						if (on_page) 
							hdf_set_valuef(hdf, POST_TAGS, post->order, i, tags[i]);
					}
				}
			} else if (on_page){
				string_append(&post_mkd, linebuf);
				string_append(&post_mkd, "\n");
			} else
				break;
		}
	}
	if (on_page && (post_title != NULL)) {
		hdf_set_valuef(hdf, POST_TITLE, post->order, post_title);
		XFREE(post_title);
	}
	XFREE(lbuf);
	fclose(post_txt);

	if (on_page) {
		/* convert markdown to html */
		MMIOT *doc;
		char *mkdbuf, *html, *date_format;

		doc = mkd_string(post_mkd.buf, post_mkd.len, MKD_NOHEADER);
		mkd_compile(doc, 0);
		len = mkd_document(doc, &mkdbuf);
		XMALLOC(html, len + 1);
		memmove(html, mkdbuf, len);
		html[len] = '\0';
		hdf_set_valuef(hdf, POST_CONTENT, post->order, html);
		XFREE(html);
		string_clear(&post_mkd);

		/* set the filename for link */
		hdf_set_valuef(hdf, "Posts.%i.filename=%s", post->order, post->filename);
		date_format = hdf_get_value(hdf, "dateformat", "%d/%m/%Y");
		if (feed != NULL) {
			date_format = DATE_FEED;
		}
		if (date_format != NULL) {
			char formated_date[256];
			struct tm *ptr;
			ptr = localtime(&(post->date));
			strftime(formated_date, 256, date_format, ptr);
			/*		cp_set_formated_date(hdf_dest, pos, formated_date); */
			hdf_set_valuef(hdf, "Posts.%i.date=%s", post->order, formated_date);
		}
	}

}

int
sort_obj_by_order(const void *a, const void *b)
{
	HDF **ha = (HDF **)a;
	HDF **hb = (HDF **)b;

	return strcasecmp(hdf_obj_name(*ha), hdf_obj_name(*hb));
}

/* convert posts to hdf */
void
convert_to_hdf(HDF *hdf, char *str, int type)
{
	int i;

	for (i = 0; i < total_posts; i++) {
		post_sorted[i]->order = i;
		parse_file(hdf, post_sorted[i], str, type);
	}
	hdf_sort_obj(hdf_get_obj(hdf, "Posts"), sort_obj_by_order);

	return;
}

/* convert any string info time_t */
time_t
str_to_time_t(char *s, char *format)
{
	struct tm date;
	time_t t;
	char *pos = strptime(s, format, &date);

	errno = 0;
	if (pos == NULL) {
		errno = EINVAL;
		err(1, "Convert '%s' to struct tm failed", s);
	}
	t = mktime(&date);
	if (t == (time_t)-1) {
		errno = EINVAL;
		err(1, "Convert struct tm (from '%s') to time_t failed", s);
	}

	return t;
}


void 
set_posts_dates()
{
	char *buf;
	char *linebuf, *lbuf;
	size_t len;
	FILE *cache;

	Posts *current_post;

	/* get the last modification time of the post */
	SLIST_FOREACH(current_post, &posthead, next) {
		struct stat filestat;
		asprintf(&buf, "%s/%s", data_dir, current_post->filename);
		stat(buf, &filestat);
		current_post->date = filestat.st_mtime;
		XFREE(buf);
	}

	/* retreive the cached dates from entries.index */
	asprintf(&buf, "%s/entries.index", cache_dir);

	/* open the index file */
	if ((cache=fopen(buf,"r")) == NULL)
		errx(1, "Unable to open the index cache file %s", buf);

	while (feof(cache) == 0) {
		while ((linebuf = fgetln(cache,&len))) {
			char **fields;
			time_t cached_date;
			if (linebuf[len - 1] == '\n') 
				linebuf[len - 1 ] = '\0';
			else {
				XMALLOC(lbuf, len + 1);
				memcpy(lbuf, linebuf, len);
				lbuf[len] = '\0';
				XSTRDUP(linebuf, lbuf);
				XFREE(lbuf);
			}
			
			/* split field[0]: filename; field[1]: timestamp */
			fields = splitstr(linebuf, "|");
			cached_date = str_to_time_t(fields[1], "%s");		

			SLIST_FOREACH(current_post, &posthead, next) {
				if (strcmp(fields[0], current_post->filename) == 0) {
					if (current_post->date >= cached_date) {
						current_post->mdate = current_post->date;
						current_post->date = cached_date;
						current_post->cached = true;
						break;
					} else {
						current_post->cached = false;
						break;
					}
				}
			}
			current_post = NULL;
			XFREE(fields);
		}
		XFREE(linebuf);
	}
	fclose(cache);
	/* append new entries in cache file */
	cache=fopen(buf,"a");
	if (cache == NULL) {
		errx(1,"Unable to open index cache file %s, in write mode",buf);
	}
	SLIST_FOREACH(current_post, &posthead, next) {
		if ( ! current_post->cached )
			fprintf( cache, "%s|%lld\n", current_post->filename, (long long int)current_post->date);
	}
	fclose(cache);
	XFREE(buf);
}

static int
get_total_posts(void)
{
	struct Posts *post;
	int count = 0;

	SLIST_FOREACH(post, &posthead, next) {
		count++;
	}

	return count;
}

static int
compare_posts_by_dates(const void *a, const void *b)
{
	Posts *pa = *((Posts **)a);
	Posts *pb = *((Posts **)b);

	return pb->date - pa->date;
}

void
sort_posts_by_date()
{
	Posts *current_post;
	int i = 0;

	total_posts = get_total_posts();
	XMALLOC(post_sorted, total_posts * sizeof(Posts *));
	SLIST_FOREACH(current_post, &posthead, next) {
		post_sorted[i++] = current_post;
	}
	qsort(post_sorted, total_posts, sizeof(Posts *),compare_posts_by_dates);
}

void
set_pages()
{
	pages = post_matching / posts_per_pages;
	if (post_matching % posts_per_pages > 0)
		pages++;
}

void
get_all_posts()
{
	FTS *fts;
	FTSENT *ent;
	char *path[2];

	SLIST_INIT(&posthead);
	SLIST_INIT(&tagshead);

	XSTRDUP(path[0], data_dir);
	path[1] = NULL;

	fts = fts_open(path, FTS_LOGICAL, NULL);
	while ((ent = fts_read(fts)) != NULL) {
		 char *name;
		 /* Only take files */
		 if (ent->fts_info != FTS_F)
			 continue;
		 XSTRDUP(name, ent->fts_name);
		 name += strlen(ent->fts_name) - 4;
		 if (strcasecmp(name, ".txt") == 0) {
			 Posts *post;
			 XMALLOC(post, sizeof(Posts));
			 XSTRDUP(post->filename, ent->fts_name);
			 post->order = 0;
			 post->cached = false;
			 SLIST_INSERT_HEAD(&posthead, post, next);
		 }
	}
}

int 
main()
{
	NEOERR *neoerr;
	char *theme;
	char *str;
	int type;
	CGI *cgi;

	type = TYPE_DATE;
	post_matching = 0;

	cgi_init(&cgi, NULL);

	neoerr = hdf_read_file(cgi->hdf, CONFFILE);
	if (neoerr != STATUS_OK)
		nerr_log_error(neoerr);

	data_dir = get_data_dir(cgi->hdf);
	if (data_dir == NULL)
		errx(1, "data.dir should be define in "CONFFILE);

	cache_dir = get_cache_dir(cgi->hdf);
	if (cache_dir == NULL)
		errx(1, "cache.dir should be define in "CONFFILE);

	posts_per_pages = get_posts_per_pages(cgi->hdf);

	page = get_query_int(cgi->hdf, "page");

	theme = get_theme(cgi->hdf);

	str = get_query_str(cgi->hdf, "post");
	if (str != NULL) {
		type = TYPE_POST;
	} else {
		str = get_query_str(cgi->hdf, "tag");
		if (str != NULL)
			type = TYPE_TAG;
	}

	feed = get_query_str(cgi->hdf, "feed");
	if (feed != NULL) {
		char genDate[256];
		struct tm *ptr;
		time_t lt;

		posts_per_pages = get_nb_feed_entries(cgi->hdf);
		theme = get_feed_tpl(cgi->hdf, feed);
		lt = time(NULL);
		ptr = localtime(&lt);
		strftime(genDate, 256, DATE_FEED, ptr);
		hdf_set_valuef(cgi->hdf, GEN_DATE, genDate);
	}
	get_all_posts();
	set_posts_dates();
	sort_posts_by_date();
	convert_to_hdf(cgi->hdf, str, type);
	set_pages();
	set_nb_pages(cgi->hdf, pages);
	hdf_set_tags(cgi->hdf);

	neoerr = cgi_display(cgi, theme);
	if (neoerr != STATUS_OK)
		nerr_log_error(neoerr);
	cgi_destroy(&cgi);

	return 0;
}

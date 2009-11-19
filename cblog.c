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
#include "comments.h"
#include <syslog.h>

/* this are wrappers to have clearsilver to work in fastcgi */
int
read_cb(void *ptr, char *data, int size)
{
	return FCGI_fread(data, sizeof(char), size, FCGI_stdin);
}

int
writef_cb(void *ptr, const char *format, va_list ap)
{
	return FCGI_vprintf(format, ap);
}

int
write_cb(void *ptr, const char *data, int size)
{
	return FCGI_fwrite((void *)data, sizeof(char), size, FCGI_stdout);
}

/* end of fcgi wrappers */


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

SLIST_HEAD(Posthead, Posts) posthead; 
SLIST_HEAD(Tagshead, Tags) tagshead;


int
sort_tags_by_name(Tags *a, Tags *b)
{
	return strcasecmp(a->name, b->name);
}

void
hdf_set_tags(HDF *hdf)
{
	Tags *tag;
	int i = 1;
	SLIST_MSORT(Tags, &tagshead, next, sort_tags_by_name);
	SLIST_FOREACH(tag, &tagshead, next) {
		set_tag_name(hdf, i, tag->name);
		set_tag_count(hdf, i, tag->count);
		i++;
	}
}

void
parse_file(HDF *hdf, Posts *post, char *str, int type)
{
	FILE *post_txt;
	char *buf, *linebuf, *lbuf, *post_title = NULL, *post_allow_comments = NULL;
	bool headers = true;
	size_t len;
	bool on_page = false;
	int i;
	int post_min = (page * posts_per_pages) - posts_per_pages;

	struct buf *ib;

	ib = bufnew(READ_UNIT);
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
				} else if (STARTS_WITH(linebuf, "Comments:")) {
					XSTRDUP(post_allow_comments, linebuf + strlen("Comments: "));
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
				bufputs(ib,linebuf);
				bufputs(ib,"\n");
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
		/* get the number of comments */
		get_comments_count(hdf,post);
		
		/* if the type is show only a post, the fill the hdf with the comments */

		if(type == TYPE_POST) {
			if (post_allow_comments != NULL)
				hdf_set_valuef(hdf, POST_ALLOW_COMMENTS, post->order, post_allow_comments);
			get_comments(hdf,post);
		}

		/* convert markdown to html using libupskirt */
		struct buf *ob;
		char *date_format;

		/* performing markdown parsing */
		ob = bufnew(OUTPUT_UNIT);
		markdown(ob, ib, &mkd_xhtml);
		
		bufnullterm(ob);
		set_post_content(hdf, post->order, ob->data);
		/* cleanup */
		bufrelease(ib);
		bufrelease(ob);

		/* set the filename for link */
		set_post_filename(hdf, post->order, post->filename);
		date_format = get_dateformat(hdf);

		if (feed != NULL)
			date_format = DATE_FEED;
		
		if (date_format != NULL)
			set_post_date(hdf, post->order, time_to_str(post->date, date_format));
	}
}

/* convert posts to hdf */
void
convert_to_hdf(HDF *hdf, char *str, int type)
{
	Posts *current_post;
	int i=0;
	SLIST_FOREACH(current_post,&posthead,next){
		current_post->order=i;
		parse_file(hdf, current_post, str, type);
		i++;
	}
	return;
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
		cblog_err(1, "Unable to open the index cache file %s", buf);

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
			cached_date = (time_t) strtol(fields[1],NULL, 0);

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
		cblog_err(1,"Unable to open index cache file %s, in write mode",buf);
	}
	SLIST_FOREACH(current_post, &posthead, next) {
		if ( ! current_post->cached )
			fprintf( cache, "%s|%lld\n", current_post->filename, (long long int)current_post->date);
	}
	fclose(cache);
	XFREE(buf);
}

static int
compare_posts_by_dates(Posts *a, Posts *b)
{
	return b->date - a->date;
}

void
sort_posts_by_date()
{
	SLIST_MSORT(Posts, &posthead, next, compare_posts_by_dates);
	/* SLIST_BUBBLE_SORT(Posts, &posthead, next, compare_posts_by_dates); */
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
	CGI *cgi;
	NEOERR *neoerr;
	STRING neoerr_str;

	openlog("CBlog", LOG_CONS|LOG_PID, LOG_DAEMON);

	/* Call the wrappers for fcgi */
	cgiwrap_init_emu(NULL, &read_cb, &writef_cb, &write_cb,
			NULL, NULL, NULL);

	while( FCGI_Accept() >= 0) {
		char *theme;
		char *str;
		int type;
		char *submit;
		char *query;

		type = TYPE_DATE;
		post_matching = 0;

		syslog_flag=CBLOG_LOG_STDOUT;
		cgi_init(&cgi, NULL);
		cgi_parse(cgi);

		neoerr = hdf_read_file(cgi->hdf, CONFFILE);
		if (neoerr != STATUS_OK) {
			nerr_error_string(neoerr, &neoerr_str);
			cblog_err(-1, neoerr_str.buf);
		}

		syslog_flag = get_syslog_flag(cgi->hdf);

		if (syslog_flag == CBLOG_LOG_SYSLOG ||
				syslog_flag == CBLOG_LOG_SYSLOG_ONLY)
			openlog("CBlog", LOG_CONS, LOG_USER);

		data_dir = get_data_dir(cgi->hdf);
		if (data_dir == NULL)
			cblog_err(1, "data.dir should be define in "CONFFILE);

		cache_dir = get_cache_dir(cgi->hdf);
		if (cache_dir == NULL)
			cblog_err(1, "cache.dir should be define in "CONFFILE);

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

		query=get_cgi_str(cgi->hdf,"RequestURI");
		/* check if there is /... in the query URI */
		if (query != NULL) {
			if (STARTS_WITH(query,get_cgi_str(cgi->hdf,"ScriptName")))
				query+=strlen(get_cgi_str(cgi->hdf,"ScriptName"));

			if (STARTS_WITH(query,get_root(cgi->hdf)))
				query+=strlen(get_root(cgi->hdf));

			if (query[0] == '/') {
				if (STARTS_WITH(query,"/tag/")) {
					size_t len;
					type=TYPE_TAG;
					str=query+5;
					/* search for the begining of query string */
					for (len = 0; len < strlen(str); len++)
						if (str[len] == '?') {
							str[len] = '\0';
							break;
						}
					hdf_set_valuef(cgi->hdf,"Query.tag=%s",str);

				}
				if (STARTS_WITH(query,"/post/")) {
					size_t len;
					type=TYPE_POST;
					str=query+6;
					/* search for the begining of query string */
					for (len = 0; len < strlen(str); len++)
						if (str[len] == '?') {
							str[len] = '\0';
							break;
						}
					hdf_set_valuef(cgi->hdf,"Query.post=%s",str);
				}
			}
		}
		submit = get_query_str(cgi->hdf,"submit");
		if ( submit != NULL &&
				!strcmp(submit,"Post") 
				&& type == TYPE_POST)
			set_comments(cgi->hdf, str);

		feed = get_query_str(cgi->hdf, "feed");
		if (feed != NULL) {
			time_t lt;

			posts_per_pages = get_nb_feed_entries(cgi->hdf);
			theme = get_feed_tpl(cgi->hdf, feed);
			lt = time(NULL);
			hdf_set_valuef(cgi->hdf, GEN_DATE, time_to_str(lt, DATE_FEED));
		}
		get_all_posts();
		set_posts_dates();
		sort_posts_by_date();
		convert_to_hdf(cgi->hdf, str, type);
		set_pages();
		set_nb_pages(cgi->hdf, pages);
		hdf_set_tags(cgi->hdf);

		neoerr = cgi_display(cgi, theme);
		if (neoerr != STATUS_OK) {
			nerr_error_string(neoerr, &neoerr_str);
			cblog_err(-1, neoerr_str.buf);
		}

		cgi_destroy(&cgi);
		if (syslog_flag == CBLOG_LOG_SYSLOG ||
				syslog_flag == CBLOG_LOG_SYSLOG_ONLY)
			closelog();

	}
	closelog();
	return EXIT_SUCCESS;
}

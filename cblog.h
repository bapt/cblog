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

#ifndef CBLOG_H
#define CBLOG_H

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/stat.h>

#include <err.h>
#include <errno.h>
#include <inttypes.h>
#include <fts.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdbool.h>

/* include Markdown and clearsilver */

#include <ClearSilver.h>
#include <mkdio.h>

#include "tools.h"

#define CONFFILE ETCDIR"/cblog.conf"

#define STARTS_WITH(string, needle) (strncasecmp(string, needle, strlen(needle)) == 0)
#define POST_TITLE "Posts.%i.title=%s"
#define POST_TAGS "Posts.%i.tags.%i.name=%s"
#define POST_CONTENT "Posts.%i.content=%s"
#define GEN_DATE "gendate=%s"

#define DATE_FEED "%a, %d %b %Y %H:%M:%S %z"

#define TYPE_DATE 0
#define TYPE_POST 1
#define TYPE_TAG 2

/* Here are the getter */
#define get_posts_per_pages(hdf) hdf_get_int_value(hdf, "posts_per_pages", 0)
#define get_query_int(hdf, name) hdf_get_int_value(hdf, "Query."name, 1)
#define get_theme(hdf) hdf_get_value(hdf, "theme", NULL)
#define get_cache_dir(hdf) hdf_get_value(hdf, "cache.dir", NULL)
#define get_data_dir(hdf) hdf_get_value(hdf, "data.dir", NULL)
#define get_query_str(hdf, name) hdf_get_value(hdf, "Query."name, NULL)
#define get_cgi_str(hdf,name) hdf_get_value(hdf, "CGI."name, NULL)
#define get_nb_feed_entries(hdf) hdf_get_int_value(hdf, "feed.nb_posts", 0)
#define get_feed_tpl(hdf, name) hdf_get_valuef(hdf, "feed.%s", name)
#define get_root(hdf) hdf_get_value(hdf,"root","")

/* Here are the setter */
#define set_nb_pages(hdf, pages) hdf_set_valuef(hdf, "nbpages=%i", pages)

typedef struct Posts {
	char *filename;
	int order;
	time_t date;
	time_t mdate;
	bool cached;
	SLIST_ENTRY(Posts) next;
} Posts;

extern const char *template_dir;
extern const char *data_dir;
extern const char *cache_dir;
extern int page;
extern int post_per_pages;
extern int nb_post;
extern int total_posts;

#define SLIST_BUBBLE_SORT(type, head, field, cmp) do { \
	int nelt; \
	type *prev, *elt, *next; \
	nelt=0; \
	SLIST_FOREACH(elt,(head),field) \
		nelt++; \
	\
	prev = NULL; \
	while (nelt--) { \
		SLIST_FOREACH(elt,(head),field) { \
			next = SLIST_NEXT(elt, field); \
			if (next && cmp(elt,next) > 0) { \
				if (elt == SLIST_FIRST((head))) { \
					SLIST_REMOVE_HEAD((head),field); \
					SLIST_INSERT_AFTER(next, elt, field); \
				} else { \
					SLIST_NEXT(prev, field) = next; \
					SLIST_NEXT(elt, field) = SLIST_NEXT(next, field); \
					SLIST_NEXT(next, field) = elt; \
				} \
				elt = next; \
			} \
			prev=elt;\
		} \
	} \
} while (0)

#define TAKE_LEFT 0
#define TAKE_RIGHT 1

#define SLIST_MSORT( type, head, field, cmp) do { \
	type *first, *tail;		/* head / tail of the sorted list */ \
	type *e;			/* element taken to add to the sorted list */ \
	size_t partsz;			/* size of one partition */ \
	size_t nmerges;		/* # of merge performed of partsz */ \
	type *left, *right;		/* current left / right pointers */ \
	size_t nleft, nright;		/* # of elt to merge starting from left / right pointers */ \
 \
	first = SLIST_FIRST(head); \
	if (first == NULL || SLIST_NEXT(first, field) == NULL) \
		/* 0 or 1 element */ \
		break; \
 \
	nmerges = 42; /* just to enter the for loop */ \
	for (partsz = 1; nmerges > 1; partsz *= 2) { \
		nmerges = 0; /* number of merge performed of partsz */ \
		left    = first; \
		tail    = NULL; \
		while (left) { \
			/* divide into [???][left][right][???] */ \
			right  = left; \
			for (nleft = 0; right && nleft < partsz; nleft++) \
				right = SLIST_NEXT(right, field); \
			nright = partsz; /* we merge at most partsz from right */ \
			while (nleft > 0 || (nright > 0 && right)) { \
				int takefrom; \
				if (nleft == 0) \
					takefrom = TAKE_RIGHT; \
				else if (nright == 0 || right == NULL) \
					takefrom = TAKE_LEFT; \
				else if (cmp(left, right) > 0) \
					takefrom = TAKE_RIGHT; \
				else \
					takefrom = TAKE_LEFT; \
 \
				/* pick e from the right list */ \
				if (takefrom == TAKE_RIGHT) { \
					e = right; \
					right = SLIST_NEXT(right, field); \
					nright--; \
				} else { \
					e = left; \
					left = SLIST_NEXT(left, field); \
					nleft--; \
				} \
 \
				if (tail == NULL) \
					first = e; \
				else \
					SLIST_NEXT(tail, field) = e; \
				tail = e; \
			} \
			left = right; \
			nmerges++; \
		} \
		SLIST_NEXT(tail, field) = NULL; \
	} \
	SLIST_FIRST(head) = first; \
 \
} while (0)

#endif


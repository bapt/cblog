#ifndef CBLOG_POSTS_H
#define CBLOG_POSTS_H

#include <sys/queue.h>

#define READ_UNIT 1024
#define OUTPUT_UNIT 1024

#define POST_ALLOW_COMMENTS "Posts.%i.allow_comments=%s"
#define POST_TITLE "Posts.%i.title=%s"
#define POST_TAGS "Posts.%i.tags.%i.name=%s"
#define POST_CONTENT "Posts.%i.content=%s"

#define TYPE_DATE 0
#define TYPE_POST 1
#define TYPE_TAG 2

#define DATE_FEED "%a, %d %b %Y %H:%M:%S %z"

/* Here are the getter */
#define get_posts_per_pages(hdf) hdf_get_int_value(hdf, "posts_per_pages", 0)
#define get_query_int(hdf, name) hdf_get_int_value(hdf, "Query."name, 1)
#define get_syslog_flag(hdf) hdf_get_int_value(hdf, "syslog", CBLOG_LOG_SYSLOG_ONLY)
#define get_theme(hdf) hdf_get_value(hdf, "theme", NULL)
#define get_cache_dir(hdf) hdf_get_value(hdf, "cache.dir", NULL)
#define get_data_dir(hdf) hdf_get_value(hdf, "data.dir", NULL)
#define get_query_str(hdf, name) hdf_get_value(hdf, "Query."name, NULL)
#define get_cgi_str(hdf,name) hdf_get_value(hdf, "CGI."name, NULL)
#define get_nb_feed_entries(hdf) hdf_get_int_value(hdf, "feed.nb_posts", 0)
#define get_feed_tpl(hdf, name) hdf_get_valuef(hdf, "feed.%s", name)
#define get_root(hdf) hdf_get_value(hdf,"root","")
#define get_dateformat(hdf)  hdf_get_value(hdf, "dateformat", "%d/%m/%Y")

/* Here are the setter */
#define set_post_date(hdf, pos, date) hdf_set_valuef(hdf, "Posts.%i.date=%s", pos, date)
#define set_post_filename(hdf, pos, name) hdf_set_valuef(hdf, "Posts.%i.filename=%s", pos, name)
#define set_post_content(hdf, pos, content) hdf_set_valuef(hdf, POST_CONTENT, pos, content)
#define get_post_content(hdf, pos) hdf_get_valuef(hdf, "Posts.%i.content", pos)

#define set_tag_name(hdf, pos, name)  hdf_set_valuef(hdf, "Tags.%i.name=%s", pos, name)
#define set_tag_count(hdf, pos, count) hdf_set_valuef(hdf, "Tags.%i.count=%i", pos, count)

void get_posts(HDF *hdf, char *str, char *name);

#define set_nb_pages(hdf, pages) hdf_set_valuef(hdf, "nbpages=%i", pages)

#define HDF_FOREACH(var, hdf, node)                 \
	for ((var) = hdf_get_child((hdf),node);         \
			(var);           \
			(var) = hdf_obj_next((var))) 

#define cache_add(cache, key, value) cdb_make_add(cache, key, strlen(key), value, strlen(value));

#endif


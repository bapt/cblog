#ifndef COMMENTS_H
#define COMMENTS_H

#include "cblog_posts.h" 

#define set_nb_comments(hdf, post, count) hdf_set_valuef(hdf, "Posts.%i.nb_comments=%i", post->order, count)
#define set_comments_author(hdf, post, count, author) hdf_set_valuef(hdf, "Posts.%i.comments.%i.author=%s", post->order, count, author)
#define set_comments_content(hdf, post, count, content) hdf_set_valuef(hdf, "Posts.%i.comments.%i.content=%s", post->order, count, content)
#define set_comments_url(hdf, post, count, url) hdf_set_valuef(hdf, "Posts.%i.comments.%i.url=%s", post->order, count, url)
#define set_comments_date(hdf, post, count, date) hdf_set_valuef(hdf, "Posts.%i.comments.%i.date=%s", post->order, count, date)
#define set_comments_ip(hdf, post, count, ip) hdf_set_valuef(hdf, "Posts.%i.comments.%i.ip=%s", post->order, count, ip)

/* Functions for comments handling */
void get_comments_count(HDF *hdf, Posts *post);
void get_comments(HDF *hdf, Posts *post);
void set_comments(HDF *hdf, char *filename);

#endif

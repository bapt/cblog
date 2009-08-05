#include "cblog.h"
#include "comments.h"

void
get_comments_count(HDF *hdf, Posts *post)
{
	int count=0;
	size_t len;
	char *comment_file;
	char *basename;
	char *buf, *lbuf;
	FILE *comment;

	strlcpy(basename,post->filename,strlen(post->filename) - 3);
	asprintf(&comment_file, "%s/%s.comments", get_cache_dir(hdf), basename);
	
	comment=fopen(comment_file,"r");
	if (comment == NULL)
		return;

	while (feof(comment) == 0) {
		while ((lbuf = fgetln(comment, &len))) {
			if (lbuf[len - 1 ] == '\n')
				lbuf[len - 1 ] = '\0';
			else {
				XMALLOC(buf,len + 1);
				memcpy(buf, lbuf, len);
				buf[len] = '\0';
				lbuf = buf;
			}

			if (STARTS_WITH(lbuf, "--" ))
				count++;
		}
	}	
	/* the comments file begins and ends with -- so remove the last counted one */
	fclose(comment);
	XFREE(lbuf);
	set_nb_comments(hdf, post, count);
}

void 
get_comments(HDF *hdf, Posts *post)
{
	int count=0;
	size_t len;
	char *comment_file;
	char *basename;
	char *buf, *lbuf;
	FILE *comment;

	strlcpy(basename,post->filename,strlen(post->filename) - 3);
	asprintf(&comment_file, "%s/%s.comments", get_cache_dir(hdf), basename);

	comment=fopen(comment_file,"r");
	if (comment == NULL)
		return;

	while (feof(comment) == 0) {
		while ((lbuf = fgetln(comment, &len))) {
			if (lbuf[len - 1 ] == '\n')
				lbuf[len - 1 ] = '\0';
			else {
				XMALLOC(buf,len + 1);
				memcpy(buf, lbuf, len);
				buf[len] = '\0';
				lbuf = buf;
			}
			if (STARTS_WITH(lbuf, "comment: ")) {
				if (lbuf[strlen("comment: ")] == '\0')
					continue;
				set_comments_content(hdf, post, count, cgi_url_unescape(lbuf + strlen("comment: ")));
			} else if (STARTS_WITH(lbuf, "name: ")) {
				if (lbuf[strlen("name: ")] == '\0')
					continue;
				set_comments_author(hdf, post, count, lbuf + strlen("name: "));
			} else if (STARTS_WITH(lbuf, "url: ")) {
				if (lbuf[strlen("url: ")] == '\0')
					continue;
				set_comments_url(hdf, post, count, lbuf + strlen("url: "));
			} else if (STARTS_WITH(lbuf, "date: ")) {
				if (lbuf[strlen("date: ")] == '\0')
					continue;
				set_comments_date(hdf,post,count,lbuf + strlen("date: "));
			} else if (STARTS_WITH(lbuf, "ip: ")) {
				if (lbuf[strlen("ip: ")] == '\0')
					continue;
				set_comments_ip(hdf,post,count,lbuf + strlen("ip: "));
			} else if (STARTS_WITH(lbuf, "--")) 
				count++;
		}
	}
	fclose(comment);
	XFREE(lbuf);
	return;
}

void
set_comments(HDF *hdf, char *filename)
{
	char *comment_file;
	char *basename;
	char *comment;
	FILE *cfd;

	/* very simple antispam */
	if (get_query_str(hdf,"test1") != NULL)
		return;

	cfd=fopen(comment_file,"a");      
	if (cfd == NULL)
		return;

	XMALLOC(basename, sizeof(filename));
	cgi_url_escape( get_query_str(hdf, "comment"), &comment);
	strlcpy(basename,filename,strlen(filename) - 3);
	asprintf(&comment_file, "%s/%s.comments", get_cache_dir(hdf), basename);  

	fprintf(cfd, "comment: %s\n", comment);
	fprintf(cfd, "name: %s\n", get_query_str(hdf, "name"));
	fprintf(cfd, "url: %s\n", get_query_str(hdf, "url"));
	fprintf(cfd, "date: %lld\n", (long long int) time(NULL));
	fprintf(cfd, "ip: %s\n", hdf_get_value(hdf,"CGI.RemoteAddress", NULL));
	fprintf(cfd, "-----\n");

	fclose(cfd);
	XFREE(basename);
}

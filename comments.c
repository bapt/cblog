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

	XMALLOC(basename,strlen(post->filename) - 3); 
	strlcpy(basename,post->filename,strlen(post->filename) - 3);
	asprintf(&comment_file, "%s/%s.comments", get_cache_dir(hdf), basename);
	
	comment=fopen(comment_file,"r");
	if (comment == NULL)
		return;

	lbuf=NULL;
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
	XFREE(basename);
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
	char *date_format;
	FILE *comment;

	XMALLOC(basename,strlen(post->filename) - 3);
	strlcpy(basename,post->filename,strlen(post->filename) - 3);
	asprintf(&comment_file, "%s/%s.comments", get_cache_dir(hdf), basename);
	XFREE(basename);
	comment=fopen(comment_file,"r");
	if (comment == NULL)
		return;

	lbuf=NULL;
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
				date_format = hdf_get_value(hdf, "dateformat", "%d/%m/%Y");
				char formated_date[256];
				struct tm *ptr;
				time_t date;
				date = str_to_time_t(lbuf + strlen("date: "),"%s");
				ptr = localtime(&date);
				strftime(formated_date, 256, date_format, ptr);
				set_comments_date(hdf,post,count,formated_date);
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
	if (strlen(get_query_str(hdf,"test1"))!=0)
		return;
	/* prevent empty name and empty comment */
	if ((strlen(get_query_str(hdf,"name")) == 0) || (strlen(get_query_str(hdf,"comment")) == 1))
		return;

	XMALLOC(basename, sizeof(filename));
	cgi_url_escape( get_query_str(hdf,"comment"), &comment);
	strlcpy(basename,filename,strlen(filename) - 3);
	asprintf(&comment_file, "%s/%s.comments", get_cache_dir(hdf), basename);  

	cfd=fopen(comment_file,"a");      
	if (cfd == NULL) {
		XFREE(basename);
		return;
	}

	fprintf(cfd, "comment: %s\n", comment);
	fprintf(cfd, "name: %s\n", get_query_str(hdf,"name"));
	fprintf(cfd, "url: %s\n", get_query_str(hdf,"url"));
	fprintf(cfd, "date: %lld\n", (long long int) time(NULL));
	fprintf(cfd, "ip: %s\n", hdf_get_value(hdf,"CGI.RemoteAddress", NULL));
	fprintf(cfd, "-----\n");

	/* Some cleanup on the hdf */
	hdf_remove_tree(hdf,"Query.name");
	hdf_remove_tree(hdf,"Query.url");
	hdf_remove_tree(hdf,"Query.comment");

	fclose(cfd);
	XFREE(basename);
}

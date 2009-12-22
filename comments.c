#include "cblog.h"
#include "comments.h"

void
get_comments_count(HDF *hdf, Posts *post)
{
	int count=0;
	char *comment_file;
	char buf[LINE_MAX];
	FILE *comment;

	asprintf(&comment_file, "%s/%s.comments", get_cache_dir(hdf), post->filename);
	
	comment=fopen(comment_file,"r");
	if (comment == NULL) {
		free(comment_file);
		return;
	}

	while (fgets(buf, LINE_MAX, comment) != NULL ) {
		if (STARTS_WITH(buf, "--" ))
			count++;
	}
	/* the comments file begins and ends with -- so remove the last counted one */
	fclose(comment);
	free(comment_file);
	set_nb_comments(hdf, post, count);
}

void 
get_comments(HDF *hdf, Posts *post)
{
	int count=0;
	char *comment_file;
	char buf[LINE_MAX];
	char *date_format;
	FILE *comment;

	asprintf(&comment_file, "%s/%s.comments", get_cache_dir(hdf), post->filename);
	comment=fopen(comment_file,"r");
	if (comment == NULL) {
		free(comment_file);
		return;
	}

	while (fgets(buf, LINE_MAX, comment) != NULL) {
			if (STARTS_WITH(buf, "comment: ")) {
				if (buf[strlen("comment: ")] == '\0')
					continue;
				set_comments_content(hdf, post, count, cgi_url_unescape(buf + strlen("comment: ")));
			} else if (STARTS_WITH(buf, "name: ")) {
				if (buf[strlen("name: ")] == '\0')
					continue;
				set_comments_author(hdf, post, count, buf + strlen("name: "));
			} else if (STARTS_WITH(buf, "url: ")) {
				if (buf[strlen("url: ")] == '\0')
					continue;
				set_comments_url(hdf, post, count, buf + strlen("url: "));
			} else if (STARTS_WITH(buf, "date: ")) {
				if (buf[strlen("date: ")] == '\0')
					continue;
				date_format = hdf_get_value(hdf, "dateformat", "%d/%m/%Y");
				time_t date;
				date = (time_t) strtol(buf + strlen("date: "), NULL, 0); 
				char *time_str = time_to_str(date, date_format);
				set_comments_date(hdf, post, count, time_str);
				free(time_str);
			} else if (STARTS_WITH(buf, "ip: ")) {
				if (buf[strlen("ip: ")] == '\0')
					continue;
				set_comments_ip(hdf,post,count,buf + strlen("ip: "));
			} else if (STARTS_WITH(buf, "--")) 
				count++;
		}
	fclose(comment);
	free(comment_file);
	return;
}

void
set_comments(HDF *hdf, char *filename)
{
	char *comment_file;
	char *comment;
	FILE *cfd;

	/* very simple antispam */
	if (strlen(get_query_str(hdf,"test1"))!=0)
		return;
	/* second one just in case */
	if (hdf_get_value(hdf, "antispamres", NULL)  == NULL)
		return;
	if (get_query_str(hdf,"antispam") == NULL)
		return;
	if (strcmp(hdf_get_value(hdf, "antispamres", NULL),get_query_str(hdf,"antispam"))!=0)
		return;
	/* prevent empty name and empty comment */
	if ((strlen(get_query_str(hdf,"name")) == 0) || (strlen(get_query_str(hdf,"comment")) == 1))
		return;

	cgi_url_escape( get_query_str(hdf,"comment"), &comment);
	asprintf(&comment_file, "%s/%s.comments", get_cache_dir(hdf), filename);  

	cfd=fopen(comment_file,"a");      
	if (cfd == NULL) {
		free(comment_file);
		return;
	}

	fprintf(cfd, "comment: %s\n", comment);
	fprintf(cfd, "name: %s\n", get_query_str(hdf,"name"));
	fprintf(cfd, "url: %s\n", get_query_str(hdf,"url"));
	fprintf(cfd, "date: %lld\n", (long long int) time(NULL));
	fprintf(cfd, "ip: %s\n", hdf_get_value(hdf,"CGI.RemoteAddress", NULL));
	fprintf(cfd, "-----\n");

	/* All is good, send an email for the new comment */
	if(hdf_get_int_value(hdf, "email.enable", 0) == 1)
	{
		char *from = hdf_get_value(hdf, "email.from", NULL);
		char *to   = hdf_get_value(hdf, "email.to", NULL);

		char *subject;
		asprintf(&subject, "New comment by %s", get_query_str(hdf,"name"));

		if(from && to)
			send_mail(from, to, subject, hdf, comment);
	}

	/* Some cleanup on the hdf */
	hdf_remove_tree(hdf,"Query.name");
	hdf_remove_tree(hdf,"Query.url");
	hdf_remove_tree(hdf,"Query.comment");

	fclose(cfd);
	free(comment_file);
}

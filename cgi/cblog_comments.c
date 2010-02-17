#include <time.h>
#include <string.h>
#include <sys/param.h>

#include "cblog_cgi.h"
#include "cblog_utils.h"
#include <syslog.h>


int
get_comments_count(char *postname)
{
	char	comment_file[MAXPATHLEN];
	char	filebuf[LINE_MAX];
	FILE	*comment;
	int		count = 0;

	snprintf(comment_file, MAXPATHLEN, CDB_PATH"/comments/%s", postname);
	comment = fopen(comment_file, "r");

	if (comment == NULL)
		return count;

	while (fgets(filebuf, LINE_MAX, comment) != NULL) {
		if (STARTS_WITH(filebuf, "--"))
			count++;
	}

	fclose(comment);
	return count;
}

void
get_comments(HDF *hdf, char *postname)
{
	char	comment_file[MAXPATHLEN];
	char	filebuf[LINE_MAX];
	FILE	*comment;
	char	*date_format;
	time_t	comment_date;
	char	date[256];
	int		count = 0;

	date_format = get_dateformat(hdf);

	snprintf(comment_file, MAXPATHLEN, CDB_PATH"/comments/%s", postname);
	comment = fopen(comment_file, "r");

	if (comment == NULL)
		return;

	while (fgets(filebuf, LINE_MAX, comment) != NULL) {
		if (STARTS_WITH(filebuf, "comment: "))
			hdf_set_valuef(hdf, "Posts.0.comments.%i.content=%s", count, cgi_url_unescape(filebuf + 9));

		else if (STARTS_WITH(filebuf, "name: "))
			hdf_set_valuef(hdf, "Posts.0.comments.%i.author=%s", count, filebuf + 6);

		else if (STARTS_WITH(filebuf, "url: "))
			hdf_set_valuef(hdf, "Posts.0.comments.%i.url=%s", count, filebuf + 5);


		else if (STARTS_WITH(filebuf, "date: ")) {
			comment_date = (time_t)strtol(filebuf + 6, NULL, 10);
			time_to_str(comment_date, date_format, date, 256);
			hdf_set_valuef(hdf, "Posts.0.comments.%i.date=%s", count, date);
		} else if (STARTS_WITH(filebuf, "--"))
			count++;
	}

	fclose(comment);
}

void
set_comment(HDF *hdf, char *postname)
{
	char	comment_file[MAXPATHLEN];
	char	*nospam, *comment;
	char	*from, *to;
	char	subject[LINE_MAX];
	FILE	*comment_fd;

	/* very simple antispam */
	if (strlen(get_query_str(hdf, "test1")) != 0)
		return;

	/* second one just in case */
	if ((nospam = hdf_get_value(hdf, "antispamres", NULL)) == NULL)
		return;

	if (get_query_str(hdf, "antispam") == NULL)
		return;

	if (!EQUALS(nospam, get_query_str(hdf, "antispam")))
		return;

	/* prevent empty name and empty comment */
	if (strlen(get_query_str(hdf, "name")) == 0 || strlen(get_query_str(hdf, "comment")) == 0)
		return;

	snprintf(comment_file, MAXPATHLEN, CDB_PATH"/comments/%s", postname);

	comment_fd = fopen(comment_file, "a");

	/* TODO Log here */

	if (comment_fd == NULL)
		return;

	cgi_url_escape(get_query_str(hdf, "comment"), &comment);
	fprintf(comment_fd, "comment: %s\n", comment);
	fprintf(comment_fd, "name: %s\n", get_query_str(hdf, "name"));
	fprintf(comment_fd, "url: %s\n", get_query_str(hdf, "url"));
	fprintf(comment_fd, "date: %lld\n", (long long int)time(NULL));
	fprintf(comment_fd, "ip: %s\n", get_cgi_str(hdf, "RemoteAddress"));
	fprintf(comment_fd, "-----\n");

	fclose(comment_fd);

	/* All is good, send an email for the new comment */
	if (hdf_get_int_value(hdf, "email.enable", 0) == 1) {
		from = hdf_get_value(hdf, "email.from", NULL);
		to   = hdf_get_value(hdf, "email.to", NULL);

		snprintf(subject, LINE_MAX, "New comment by %s", get_query_str(hdf, "name"));

		if (from && to)
			send_mail(from, to, subject,
					get_cgi_str(hdf, "RemoteAddress"),
					get_query_str(hdf, "comment"),
					get_query_str(hdf, "name"),
					get_query_str(hdf, "url"));


	}

	/* Some cleanup on the hdf */
	hdf_remove_tree(hdf, "Query.name");
	hdf_remove_tree(hdf, "Query.url");
	hdf_remove_tree(hdf, "Query.comment");
}

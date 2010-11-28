#include <time.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/param.h>
#include <sys/stat.h>

#include "cblog_cgi.h"
#include "cblog_utils.h"
#include <syslog.h>

int
get_comments_count(char *postname)
{
	char		comment_file[MAXPATHLEN];
	int			commentfd;
	int			count = 0, j = 0, nbel = 0;
	struct stat	comment_stat;
	size_t		next;
	char		*buffer = NULL, *bufstart;

	snprintf(comment_file, MAXPATHLEN, CDB_PATH"/comments/%s", postname);

	if (stat(comment_file, &comment_stat) == -1)
		return count;

	if ((buffer = malloc((comment_stat.st_size + 1) * sizeof(char))) == NULL)
		return count;

	if ((commentfd = open(comment_file, O_RDONLY)) == -1 ) {
		free(buffer);
		return count;
	}

	if (read(commentfd, buffer, comment_stat.st_size) != comment_stat.st_size) {
		free(buffer);
		return count;
	}

	buffer[comment_stat.st_size] = '\0';

	if (close(commentfd) == -1) {
		free(buffer);
		return count;
	}

	bufstart = buffer;
	nbel = splitchr(buffer, '\n');
	next = strlen(buffer);

	for (j=0; j <= nbel; j++) {
		if (STARTS_WITH(buffer, "--"))
			count++;

		buffer += next + 1;
		next = strlen(buffer);
	}

	free(buffer);

	return count;
}

void
get_comments(HDF *hdf, char *postname)
{
	char		comment_file[MAXPATHLEN];
	int			commentfd;
	char		*date_format;
	time_t		comment_date;
	char		date[256];
	int			count = 0;
	int			nbel = 0, j = 0;
	char		*buffer = NULL, *bufstart;
	struct stat	comment_stat;
	size_t		next;

	date_format = get_dateformat(hdf);

	snprintf(comment_file, MAXPATHLEN, CDB_PATH"/comments/%s", postname);
	if (stat(comment_file, &comment_stat) == -1)
		return;

	if ((buffer = malloc((comment_stat.st_size + 1) * sizeof(char))) == NULL)
		return;

	if ((commentfd = open(comment_file, O_RDONLY)) == -1 ) {
		free(buffer);
		return;
	}

	if (read(commentfd, buffer, comment_stat.st_size) != comment_stat.st_size) {
		free(buffer);
		return;
	}

	buffer[comment_stat.st_size] = '\0';

	if (close(commentfd) == -1) {
		free(buffer);
		return;
	}

	bufstart = buffer;
	nbel = splitchr(buffer, '\n');
	next = strlen(buffer);
	while (j <= nbel) {
		if (STARTS_WITH(buffer, "comment: "))
			hdf_set_valuef(hdf, "Posts.0.comments.%i.content=%s", count, cgi_url_unescape(buffer + 9));

		else if (STARTS_WITH(buffer, "name: "))
			hdf_set_valuef(hdf, "Posts.0.comments.%i.author=%s", count, buffer + 6);

		else if (STARTS_WITH(buffer, "url: "))
			hdf_set_valuef(hdf, "Posts.0.comments.%i.url=%s", count, buffer + 5);


		else if (STARTS_WITH(buffer, "date: ")) {
			comment_date = (time_t)strtol(buffer + 6, NULL, 10);
			time_to_str(comment_date, date_format, date, 256);
			hdf_set_valuef(hdf, "Posts.0.comments.%i.date=%s", count, date);
		} else if (STARTS_WITH(buffer, "--"))
			count++;

		buffer += next + 1;
		j++;
		next = strlen(buffer);
	}
	
	free(bufstart);
}

void
set_comment(HDF *hdf, char *postname)
{
	char	comment_file[MAXPATHLEN];
	char    *nospam, *comment, *name, *url;
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

	cgi_url_escape(get_query_str(hdf, "name"), &name);
	cgi_url_escape(get_query_str(hdf, "url"), &url);
	cgi_url_escape(get_query_str(hdf, "comment"), &comment);
	fprintf(comment_fd, "comment: %s\n", comment);
	fprintf(comment_fd, "name: %s\n", name);
	fprintf(comment_fd, "url: %s\n", url);
	fprintf(comment_fd, "date: %lld\n", (long long int)time(NULL));
	fprintf(comment_fd, "ip: %s\n", get_cgi_str(hdf, "RemoteAddress"));
	fprintf(comment_fd, "-----\n");

	free(comment);
	free(url);
	free(name);

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

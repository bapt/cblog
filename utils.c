#include <errno.h>
#include <string.h>
#include <syslog.h>
#include <stdbool.h>
#include "utils.h"
#include "cblog_log.h"

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
		cblog_err(1, "Convert '%s' to struct tm failed", s);
	}
	t = mktime(&date);
	if (t == (time_t)-1) {
		errno = EINVAL;
		cblog_err(1, "Convert struct tm (from '%s') to time_t failed", s);
	}

	return t;
}

char *
time_to_str(time_t source, char *format)
{
	char formated_date[256];
	struct tm *ptr;

	ptr = localtime(&source);
	strftime(formated_date, 256, format, ptr);
	return strdup(formated_date);
}

/* Send an email to an email with a specified subject
 * and body
 */
void
send_mail(const char *from, const char *to, const char *subject, HDF *hdf, const char *comment)
{
	FILE *email = popen("/usr/sbin/sendmail -t -oi", "w");
	fprintf(email, "Subject: %s\n", subject);
	fprintf(email, "To: %s\n", to);
	fprintf(email, "From: %s\n", from);
	fprintf(email, "Content-type: text/plain\n\n");
	fprintf(email, "A new comment has been submited.\n");
	fprintf(email, "Name : %s\n", get_query_str(hdf,"name"));
	fprintf(email, "URL : %s\n", get_query_str(hdf,"url"));
	fprintf(email, "IP : %s\n", hdf_get_value(hdf,"CGI.RemoteAddress", NULL));
	fprintf(email, "Comment :\n");
	fprintf(email, "%s\n", comment);
	pclose(email);
	return;

}

bool
file_exists(const char *filename)
{
	FILE *file;
	if ( (file = fopen(filename, "r")) != NULL)
{
		fclose(file);
		return true;
	}
	return false;
}

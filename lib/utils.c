#include "cblog_utils.h"
#include <ctype.h>
#include <stdio.h>
#include <stdbool.h>
#include <time.h>
#include <string.h>

int
splitchr(char *str, char sep)
{
	char	*next;
	char	*buf = str;
	int		nbel = 0;

	while ((next = strchr(buf, sep)) != NULL) {
		nbel++;
		buf = next;
		buf[0] = '\0';
		buf++;
	}

	return nbel;
}

void
time_to_str(time_t source, const char *format, char *dest, size_t size)
{
	struct tm	*ptr;

	ptr = localtime(&source);
	strftime(dest, size, format, ptr);
}

/* Send an email to an email with a specified subject
 * and body
 */
void
send_mail(const char *from, const char *to, const char *subject, const char *ip, 
		const char *comment, const char *name, const char *url)
{
	FILE *email = popen("/usr/sbin/sendmail -t -oi", "w");

	fprintf(email, "Subject: %s\n", subject);
	fprintf(email, "To: %s\n", to);
	fprintf(email, "From: %s\n", from);
	fprintf(email, "Content-type: text/plain\n\n");
	fprintf(email, "A new comment has been submited.\n");
	fprintf(email, "Name : %s\n", name);
	fprintf(email, "URL : %s\n", url);
	fprintf(email, "IP : %s\n", ip);
	fprintf(email, "Comment :\n");
	fprintf(email, "%s\n", comment);

	pclose(email);

	return;
}

char *
trimspace(char *str)
{
	char *line = str;

	/* remove spaces at the beginning */
	while (true) {
		if (isspace(line[0]))
			line++;
		else
			break;
	}
	/* remove spaces at the end */
	while (true) {
		if (isspace(line[strlen(line) - 1]))
			line[strlen(line) - 1] = '\0';
		else
			break;
	}
	return line;
}


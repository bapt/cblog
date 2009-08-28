#include "utils.h"

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
		err(1, "Convert '%s' to struct tm failed", s);
	}
	t = mktime(&date);
	if (t == (time_t)-1) {
		errno = EINVAL;
		err(1, "Convert struct tm (from '%s') to time_t failed", s);
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

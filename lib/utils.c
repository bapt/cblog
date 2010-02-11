#include "cblog_utils.h"
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

#include <cdb.h>

#define EQUALS(string, needle) (strcasecmp(string, needle) == 0)
#define STARTS_WITH(string, needle) (strncasecmp(string, needle, strlen(needle)) == 0)

char	*db_get(struct cdb *);
int		splitchr(char *str, char sep);

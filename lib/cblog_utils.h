#ifndef	CBLOG_LIB_CBLOG_UTILS_H
#define	CBLOG_LIB_CBLOG_UTILS_H

#include <time.h>
#include <sqlite3.h>

#define EQUALS(string, needle) (strcasecmp(string, needle) == 0)
#define STARTS_WITH(string, needle) (strncasecmp(string, needle, strlen(needle)) == 0)

int splitchr(char *, char);
void send_mail(const char *, const char *, const char *, 
    const char *, const char *, const char *, const char *);
int sql_exec(sqlite3 *s, const char *, ...);
char *trimspace(char *str);

#endif	/* ndef CBLOG_LIB_CBLOG_UTILS_H */

#ifndef	CBLOG_LIB_CBLOG_UTILS_H
#define	CBLOG_LIB_CBLOG_UTILS_H

#include <sqlite3.h>
#include <stdint.h>

#define EQUALS(string, needle) (strcasecmp(string, needle) == 0)
#define STARTS_WITH(string, needle) (strncasecmp(string, needle, strlen(needle)) == 0)

int splitchr(char *, char);
void send_mail(const char *, const char *, const char *, 
    const char *, const char *, const char *, const char *);
int sql_exec(sqlite3 *s, const char *, ...);

int sql_text(sqlite3 *s, char **, const char *, ...);
int sql_int(sqlite3 *s, int64_t *, const char *, ...);

#endif	/* ndef CBLOG_LIB_CBLOG_UTILS_H */

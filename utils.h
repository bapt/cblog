#ifndef UTILS_H
#define UTILS_H

#include <time.h>
#include <ClearSilver.h>
#include "tools.h"

#define get_query_str(hdf, name) hdf_get_value(hdf, "Query."name, NULL)
#define STARTS_WITH(string, needle) (strncasecmp(string, needle, strlen(needle)) == 0)
#define EQUALS(string, needle) (strcasecmp(string, needle) == 0)

time_t str_to_time_t(char *s, char *format);
char *time_to_str(time_t source, char *format);
void send_mail(const char *from, const char *to, const char *subject, HDF *hdf, const char *comment);
bool file_exists(const char *filename);

#endif

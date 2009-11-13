#ifndef UTILS_H
#define UTILS_H

#include "tools.h"
#include <time.h>
#include <errno.h>
#include <ClearSilver.h>

#define get_query_str(hdf, name) hdf_get_value(hdf, "Query."name, NULL)

time_t str_to_time_t(char *s, char *format);
char *time_to_str(time_t source, char *format);
void send_mail(const char *from, const char *to, const char *subject, HDF *hdf, const char *comment);

#endif

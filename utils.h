#ifndef UTILS_H
#define UTILS_H

#include "tools.h"
#include <time.h>
#include <errno.h>

time_t str_to_time_t(char *s, char *format);
char *time_to_str(time_t source, char *format);

#endif

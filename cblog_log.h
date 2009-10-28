#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <syslog.h>

#define CBLOG_LOG_SYSLOG 0
#define CBLOG_LOG_SYSLOG_ONLY 1
#define CBLOG_LOG_STDOUT 3

int syslog_flag;
void cblog_err(int eval, const char * messages, ...);

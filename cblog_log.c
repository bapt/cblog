#include "cblog_log.h"

void
cblog_err(int eval, const char * message, ...)
{
	va_list args;
	va_start(args, message);
	vsyslog(LOG_ERR,message, args);
}
void
cblog_warn(const char * message, ...)
{
	va_list args;
	va_start(args, message);
	vsyslog(LOG_WARNING,message, args);
}
void
cblog_info(const char * message, ...)
{
	va_list args;
	va_start(args, message);
	vsyslog(LOG_INFO,message, args);
}

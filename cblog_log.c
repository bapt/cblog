#include "cblog_log.h"

void
cblog_err(int eval, const char * message, ...)
{
	va_list args;
	char *output=NULL;
	va_start(args, message);
	if (syslog_flag == CBLOG_LOG_SYSLOG || 
			syslog_flag == CBLOG_LOG_SYSLOG_ONLY) {
		vsyslog(LOG_ERR,message, args);
		if (syslog_flag == CBLOG_LOG_SYSLOG_ONLY)
			exit(eval);
	}
	printf("Content-Type : text/HTML\n\n");
	asprintf(&output,message, args);
	printf("<HTML><HEAD><TITLE>CBlog : ERROR</TITLE></HEAD><BODY><H1>Cblog : ERROR</H1><H2>%s</BODY></HTML>\n",output);
	free(output);
	exit(EXIT_SUCCESS);
}

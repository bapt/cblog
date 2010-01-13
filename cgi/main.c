#include <fcgi_stdio.h>
#include <syslog.h>

#include "cblog_cgi.h"

/* this are wrappers to have clearsilver to work in fastcgi */
int
read_cb(void *ptr, char *data, int size)
{
	return FCGI_fread(data, sizeof(char), size, FCGI_stdin);
}

int
writef_cb(void *ptr, const char *format, va_list ap)
{
    FCGI_vprintf(format, ap);
    return 0;
}

int
write_cb(void *ptr, const char *data, int size)
{
	return FCGI_fwrite((void *)data, sizeof(char), size, FCGI_stdout);
}

/* end of fcgi wrappers */

int
main(int argc, char **argv, char **envp)
{
	openlog("CBlog", LOG_CONS|LOG_ERR, LOG_DAEMON);
	cgiwrap_init_emu(NULL, &read_cb, &writef_cb, &write_cb,
			NULL, NULL, NULL);
	cgiwrap_init_std(argc, argv, envp);
	while( FCGI_Accept() >= 0) {
	/*	cgi_init(&cgi, NULL);
		cgi_parse(cgi); */
		cblogcgi();
/*		cgi_destroy(&cgi);
		syslog(LOG_ERR, "coucou"); */
	}
	closelog();
	return EXIT_SUCCESS;
}

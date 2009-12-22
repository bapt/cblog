#include <fcgi_stdio.h>
#include <syslog.h>

#include "cblog.h"
#include "cblog_posts.h"

/* this are wrappers to have clearsilver to work in fastcgi */
int
read_cb(void *ptr, char *data, int size)
{
	return FCGI_fread(data, sizeof(char), size, FCGI_stdin);
}

int
writef_cb(void *ptr, const char *format, va_list ap)
{
	return FCGI_vprintf(format, ap);
}

int
write_cb(void *ptr, const char *data, int size)
{
	return FCGI_fwrite((void *)data, sizeof(char), size, FCGI_stdout);
}

/* end of fcgi wrappers */

int
main()
{
	openlog("CBlog", LOG_CONS|LOG_ERR, LOG_DAEMON);
	parse_conf();
	cgiwrap_init_emu(NULL, &read_cb, &writef_cb, &write_cb,
			NULL, NULL, NULL);
	while( FCGI_Accept() >= 0) {
		cblog_main();
		cblog_err(1, "coucou");
	}
	cblog_err(1, "bye");
	closelog();
	clean_conf();
	posts_cleanup();
	return EXIT_SUCCESS;
}

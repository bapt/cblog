#include <unistd.h>
#include <err.h>
#include <fcgi_stdio.h>
#include <syslog.h>
#include <signal.h>

#include "cblog_cgi.h"

HDF *conf;

void
read_conf() {
	HDF *hdf;
	NEOERR *neoerr;
	if (access(CONFFILE, R_OK) != 0) {
		cblog_err(-1, "%s: can't access file", CONFFILE);
		return;
	}
	neoerr = hdf_init(&hdf);
	if (neoerr != STATUS_OK) {
		cblog_err(-1, "%s: hdf_init hdf", CONFFILE);
	}
	nerr_ignore(&neoerr);

	neoerr = hdf_read_file(hdf, CONFFILE);
	if (neoerr != STATUS_OK) {
		cblog_err(-1, "%s: hdf_read_file error", CONFFILE);
		return;
	}
	nerr_ignore(&neoerr);

	neoerr = hdf_copy(conf, "", hdf);
	if (neoerr != STATUS_OK) {
		cblog_err(-1, "%s: hdf_copy error", CONFFILE);
		return;
	}
}

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
	NEOERR *neoerr;

	signal(SIGHUP, read_conf);

	if (access(CONFFILE, R_OK) != 0)
		errx(1, "%s: file not found", CONFFILE);

	neoerr = hdf_init(&conf);
	if (neoerr != STATUS_OK) {
		errx(1, "hdf_init conf");
	}
	nerr_ignore(&neoerr);

	neoerr = hdf_read_file(conf, CONFFILE);
	if (neoerr != STATUS_OK) {
		errx(1, "hdf_read %s", CONFFILE);
	}
	nerr_ignore(&neoerr);

	openlog("CBlog", LOG_CONS|LOG_ERR, LOG_DAEMON);
	cgiwrap_init_emu(NULL, &read_cb, &writef_cb, &write_cb,
		NULL, NULL, NULL);
	cgiwrap_init_std(argc, argv, envp);

	while (FCGI_Accept() >= 0) {
		/*	cgi_init(&cgi, NULL);
		cgi_parse(cgi); */
		cblogcgi(conf); 
		/*	cgi_destroy(&cgi);
		syslog(LOG_ERR, "coucou"); */
	}
	closelog();
	return EXIT_SUCCESS;
}

/* vim: set sw=4 sts=4 ts=4 : */

#include <unistd.h>
#include <err.h>
#include <fcgi_stdio.h>
#include <syslog.h>
#include <signal.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netdb.h>

#include "cblog_cgi.h"

#ifndef UNIX_PATH_MAX
#define UNIX_PATH_MAX 108
#endif

HDF *conf;
int fd;
char *unix_sock_path = NULL;

static char *mandatory_config[] = {
	"title",
	"url",
	"dateformat",
	"hdf.loadpaths.tpl",
	"db_path",
	"theme",
	"posts_per_pages",
	NULL,
};

int
check_conf(HDF *conf)
{
	int i = 0;
	while (mandatory_config[i]) {
		if (! hdf_get_obj(conf, mandatory_config[i]))
			return i;
		i++;
	}
	return -1;
}

void
read_conf(int signal /* unused */)
{
	HDF *hdf;
	NEOERR *neoerr;
	int ret;

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

	if ((ret = check_conf(hdf)) != -1) {
		cblog_err(-1, "%s: %s is mandatory", CONFFILE, mandatory_config[ret]);
		return;
	}
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

static void
close_socket(int dummy) {
	close(fd);
	if (unix_sock_path != NULL)
		unlink(unix_sock_path);

	exit(0);
}

static int
bind_socket(char *url) {
	char *p = url;
	char *port;
	int er;
	struct sockaddr_un un;
	struct addrinfo *ai;
	struct addrinfo hints;

	if (!strncmp(p, "unix:", sizeof("unix:") - 1)) {
		p += sizeof("unix:") - 1;

		if (strlen(p) >= UNIX_PATH_MAX)
			cblog_err(-1, "Socket path too long, exceeds %d characters\n", UNIX_PATH_MAX);
		
		memset(&un, 0, sizeof(struct sockaddr_un));
		if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
			err(-1, "socket()");

		un.sun_family = AF_UNIX;
		strcpy(un.sun_path, p);
		unix_sock_path = p;

		if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (int[]){1}, sizeof(int)) < 0)
			err(1, "setsockopt()");

		if (bind(fd, (struct sockaddr *) &un, sizeof(struct sockaddr_un)) == -1)
			err(-1, "bind()");

	} else if (!strncmp(p, "tcp:", sizeof("tcp:") - 1)) {
		p += sizeof("tcp:") - 1;

		port = strchr(p, ':');
		if (!port)
			cblog_err(-1, "invalid url\n");

		port[0] = '\n';
		port++;

		memset(&hints, 0, sizeof hints);

		hints.ai_family = PF_UNSPEC;
		hints.ai_flags = AI_PASSIVE;
		hints.ai_socktype = SOCK_STREAM;
		if ((er = getaddrinfo(p, port, &hints, &ai)) != 0)
			cblog_err(-1, "getaddrinfo(): %s", gai_strerror(er));

		if ((fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol)) == -1)
			err(-1, "socket()");

		if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (int[]){1}, sizeof(int)) < 0)
			err(1, "setsockopt()");

		if (bind(fd, ai->ai_addr, ai->ai_addrlen) == -1)
			err(-1, "bind()");

	}

	signal(SIGINT, close_socket);
	signal(SIGKILL, close_socket);
	signal(SIGQUIT, close_socket);
	signal(SIGTERM, close_socket);


	if (listen(fd, 1024) < 0)
		err(1, "listen()");

	if (dup2(fd, 0) < 0)
		err(-1, "dup2()");

	if (close(fd) < 0)
		err(-1, "close()");

	return 0;
}

/* end of fcgi wrappers */

int
main(int argc, char **argv, char **envp)
{
	NEOERR *neoerr;
	int ret;

	signal(SIGHUP, read_conf);
	signal(SIGPIPE, SIG_IGN);

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

	if ((ret = check_conf(conf)) != -1) {
		errx(1, "check_conf %s: %s is mandatory", CONFFILE, mandatory_config[ret]);
	}

	openlog("CBlog", LOG_CONS|LOG_ERR, LOG_DAEMON);
	cgiwrap_init_emu(NULL, &read_cb, &writef_cb, &write_cb,
		NULL, NULL, NULL);
	cgiwrap_init_std(argc, argv, envp);
	if (argc == 2) {
		daemon(0,0);
		bind_socket(argv[1]);
	}

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

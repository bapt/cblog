#include <unistd.h>
#include <err.h>
#include <syslog.h>
#include <signal.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netdb.h>

#include <event2/event.h>
#include <event2/http.h>
#include <event2/http_struct.h>
#include <event2/buffer.h>
#include <event2/bufferevent.h>
#include <event2/util.h>
#include <event2/keyvalq_struct.h>

#include "cblogweb.h"

#ifndef UNIX_PATH_MAX
#define UNIX_PATH_MAX 108
#endif

HDF *conf;
int fd;
char *unix_sock_path = NULL;

static char *mandatory_config[] = {
	"author",
	"title",
	"url",
	"dateformat",
	"db_path",
	"theme",
	"posts_per_pages",
	"templates",
	"interface",
	"port",
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
		syslog(LOG_ERR, "%s: can't access file", CONFFILE);
		return;
	}
	neoerr = hdf_init(&hdf);
	if (neoerr != STATUS_OK) {
		syslog(LOG_ERR, "%s: hdf_init hdf", CONFFILE);
	}
	nerr_ignore(&neoerr);

	neoerr = hdf_read_file(hdf, CONFFILE);
	if (neoerr != STATUS_OK) {
		syslog(LOG_ERR, "%s: hdf_read_file error", CONFFILE);
		return;
	}
	nerr_ignore(&neoerr);

	if ((ret = check_conf(hdf)) != -1) {
		syslog(LOG_ERR, "%s: %s is mandatory", CONFFILE, mandatory_config[ret]);
		return;
	}
	neoerr = hdf_copy(conf, "", hdf);
	if (neoerr != STATUS_OK) {
		syslog(LOG_ERR, "%s: hdf_copy error", CONFFILE);
		return;
	}
}

/* end of fcgi wrappers */

int
main(int argc, char **argv, char **envp)
{
	struct event_base *eb;
	struct evhttp *eh;
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
	if ((eb = event_base_new()) == NULL)
		err(EXIT_FAILURE, "event_base_new return NULL");
	if ((eh = evhttp_new(eb)) == NULL)
		err(EXIT_FAILURE, "evhttp_new return NULL");

	evhttp_set_gencb(eh, cblog, conf);

	sqlite3_initialize();
	const char *interface = hdf_get_valuef(conf, "interface");
	int port = hdf_get_int_value(conf, "port", 8080);
	evhttp_bind_socket(eh, interface, port);

	if (argc == 2) {
		if (strcmp(argv[1], "-d") == 0)
			daemon(0,0);
		else
			err(EXIT_FAILURE, "unknown option: %s", argv[1]);
	} else if (argc > 2) {
			err(EXIT_FAILURE, "Too many options");
	}

	event_base_dispatch(eb);

	evhttp_free(eh);
	event_base_free(eb);
	hdf_destroy(&conf);

	sqlite3_shutdown();
	closelog();
	return EXIT_SUCCESS;
}
/* vim: set sw=4 sts=4 ts=4 : */

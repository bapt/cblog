#include <unistd.h>
#include <err.h>
#include <syslog.h>
#include <signal.h>
#include <string.h>
#include <sys/types.h>
#include <sys/event.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netdb.h>
#include <sys/sbuf.h>
#include <fcntl.h>
#include <errno.h>

#include "cblogweb.h"

#ifndef UNIX_PATH_MAX
#define UNIX_PATH_MAX 108
#endif

HDF *conf;
int fd, kq;
static int nbevq = 0;
static struct kevent ke;
const char *unix_sock_path = NULL;

static char *mandatory_config[] = {
	"author",
	"title",
	"url",
	"dateformat",
	"db_path",
	"theme",
	"posts_per_pages",
	"stemplates",
	"interface",
	"port",
	NULL,
};

struct client {
	int fd;
	struct sockaddr_storage ss;
	struct sbuf *req;
	struct kv **headers;
	uid_t uid;
	gid_t gid;
};

static void
client_free(struct client *cl)
{

	int i;

	if (cl->fd != -1)
		close(cl->fd);
	sbuf_delete(cl->req);

	if (cl->headers) {
		for (i = 0; cl->headers[i] != NULL; i++) {
			free(cl->headers[i]);
		}
		free(cl->headers);
	}

	free(cl);
}

static struct client *
client_new(int cfd)
{
	socklen_t sz;
	struct client *cl;
	int flags;

	if ((cl = malloc(sizeof(struct client))) == NULL)
		return (NULL);

	sz = sizeof(cl->ss);
	cl->fd = accept(fd, (struct sockaddr *)&(cl->ss), &sz);

	if (cl->fd < 0) {
		client_free(cl);
		return (NULL);
	}

	if (getpeereid(cl->fd, &cl->uid, &cl->gid) != 0) {
		client_free(cl);
		return (NULL);
	}

	if (-1 == (flags = fcntl(cl->fd, F_GETFL, 0)))
		flags = 0;

	fcntl(cl->fd, F_SETFL, flags | O_NONBLOCK);

	cl->req = sbuf_new_auto();
	cl->headers = NULL;

	return (cl);
}

static void
client_read(struct client *cl, long len)
{
	int r;
	char buf[BUFSIZ];

	r = read(cl->fd, buf, sizeof(buf));
	if (r < 0 && (errno == EINTR || errno == EAGAIN))
		return ;

	sbuf_bcat(cl->req, buf, r);

	if ((long)r == len) {
		sbuf_finish(cl->req);
		cl->headers = scgi_parse(sbuf_data(cl->req));
	}
}

static void
close_socket(int dummy)
{
	if (fd != -1)
		close(fd);

	if (unix_sock_path)
		unlink(unix_sock_path);

	sqlite3_shutdown();
	hdf_destroy(&conf);

	exit(dummy);
}

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

static void
serve(void) {
	struct kevent *evlist = NULL;
	struct client *cl;

	int nev, i;
	int max_queues = 0;

	if ((kq = kqueue()) == -1) {
		warn("kqueue()");
		close_socket(EXIT_FAILURE);
	}

	EV_SET(&ke, fd,  EVFILT_READ, EV_ADD, 0, 0, NULL);
	kevent(kq, &ke, 1, NULL, 0, NULL);
	nbevq++;

	for (;;) {
		if (nbevq > max_queues) {
			max_queues += 1024;
			free(evlist);
			if ((evlist = malloc(max_queues *sizeof(struct kevent))) == NULL) {
				warnx("Unable to allocate memory");
				close_socket(EXIT_FAILURE);
			}

			nev = kevent(kq, NULL, 0, evlist, max_queues, NULL);
			for (i = 0; i < nev; i++) {
				/* new client */
				if (evlist[i].udata == NULL && evlist[i].filter == EVFILT_READ) {
					if ((cl = client_new(evlist[i].ident)) == NULL)
						continue;
					EV_SET(&ke, cl->fd, EVFILT_READ, EV_ADD, 0, 0, cl);
					kevent(kq, &ke, 1, NULL, 0, NULL);
					nbevq++;
					continue;
				}
				/* Reading from client */
				if (evlist[i].filter == EVFILT_READ) {
					if (evlist[i].flags & (EV_ERROR | EV_EOF)) {
						/* Do an extra read on EOF as kqueue
						 * will send this even if there is
						 * data still available. */
						if (evlist[i].flags & EV_EOF)
							client_read(evlist[i].udata, evlist[i].data);
						client_free(evlist[i].udata);
						nbevq--;
						continue;
					}
					client_read(evlist[i].udata, evlist[i].data);
					continue;
				}
			}

		}
	}
}

int
main(int argc, char **argv, char **envp)
{
	struct sockaddr_un un;

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
	memset(&un, 0, sizeof(struct sockaddr_un));
	if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
		err(EXIT_FAILURE, "socket()");

	unix_sock_path = hdf_get_valuef(conf, "listen");
	unlink(unix_sock_path);
	un.sun_family = AF_UNIX;
	strlcpy(un.sun_path, unix_sock_path, sizeof(un.sun_path));

	if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (int[]){1}, sizeof(int)) < 0)
		err(EXIT_FAILURE, "setsockopt()");

	if (bind(fd, (struct sockaddr *) &un, sizeof(struct sockaddr_un)) == -1)
		err(EXIT_FAILURE, "bind()");

	signal(SIGINT, close_socket);
	signal(SIGKILL, close_socket);
	signal(SIGQUIT, close_socket);
	signal(SIGTERM, close_socket);

	if (listen(fd, 1024) <0) {
		warn("listen()");
		close_socket(EXIT_FAILURE);
	}

	if (argc == 2) {
		if (strcmp(argv[1], "-d") == 0)
			daemon(0,0);
		else
			err(EXIT_FAILURE, "unknown option: %s", argv[1]);
	} else if (argc > 2) {
			err(EXIT_FAILURE, "Too many options");
	}

	serve();

	close_socket(EXIT_SUCCESS);
	return (0); /* NOT REACHED */
}
/* vim: set sw=4 sts=4 ts=4 : */

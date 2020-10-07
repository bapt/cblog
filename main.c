#include <sys/param.h>

#include <ClearSilver.h>
#include <stdio.h>
#include <err.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include "cblog.h"

static struct command {
	const char *name;
	const char *shortcut;
	const char *descr;
	const int cmdtype;
} cmd[] = {
	{ "version", "v", "Version of CBlog", CBLOG_VERSION_CMD},
	{ "path", "p", "Print the path where the posts are located", CBLOG_PATH_CMD},
	{ "gen", "g", "Generate the website", CBLOG_GEN_CMD},
	{ NULL, NULL, NULL, 0},
};

static char *mandatory_config[] = {
	"author",
	"title",
	"url",
	"dateformat",
	"db_path",
	"theme",
	"posts_per_pages",
	"templates",
	NULL,
};

static int
check_conf(HDF *conf)
{

	for (int i = 0; mandatory_config[i] != NULL; i++) {
		if (! hdf_get_obj(conf, mandatory_config[i]))
			return (i);
	}
	return (-1);
}

static void
usage(const char *s)
{
	printf("Usage: %s cmd [option]\n\n\
Commands Supported:\n\
	gen\t\t-- Generate the website
	path\t\t-- Print the path where the posts are located\n\
	version\t\t-- Print cblog version\n", s);

	exit(1);
}

int
main(int argc, char *argv[])
{
	char conffile[MAXPATHLEN];
	int i, ret;
	int type = -1;
	HDF *conf;
	NEOERR *neoerr;

	if (argc == 1)
		usage(argv[0]);

	if (access(CONFFILE, R_OK) != 0) {
		if (access(CONFFILENAME, R_OK) != 0)
			err(1, "%s: can't access file", CONFFILE);
		strlcpy(conffile, CONFFILENAME, MAXPATHLEN);
	} else {
		strlcpy(conffile, CONFFILE, MAXPATHLEN);
	}

	neoerr = hdf_init(&conf);
	if (neoerr != STATUS_OK)
		errx(1, "%s: hdf_init hdf", conffile);
	nerr_ignore(&neoerr);

	neoerr = hdf_read_file(conf, conffile);
	if (neoerr != STATUS_OK)
		errx(1, "%s: hdf_read_file error", conffile);
	nerr_ignore(&neoerr);

	if ((ret = check_conf(conf)) != -1)
		errx(1, "check_conf %s: %s is mandatory", conffile, mandatory_config[ret]);

	/* find the type of the first command */
	for (i=0; cmd[i].name != NULL; i++) {
		if (EQUALS(argv[1], cmd[i].name) || EQUALS(argv[1], cmd[i].shortcut)) {
			type = cmd[i].cmdtype;
			break;
		}
	}

	switch(type) {
		case CBLOG_VERSION_CMD:
			cblogctl_version();
			exit(0);
		case CBLOG_PATH_CMD:
			cblogctl_path(conf);
			exit(0);
		case CBLOG_GEN_CMD:
			cblogctl_gen(conf);
			exit (0);
		default:
			usage(argv[0]);
			/* NOT REACHED */
	}
	return EXIT_SUCCESS;
}

#include <ClearSilver.h>
#include <stdio.h>
#include <err.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include "cblogctl.h"
#include "cblog_utils.h"

static struct command {
	const char *name;
	const char *shortcut;
	const char *descr;
	const int cmdtype;
} cmd[] = {
	{ "add", "a", "Add or modify a post", CBLOG_ADD_CMD},
	{ "del", "d", "Delete a post", CBLOG_DEL_CMD},
	{ "get", "g", "Get a post in text format", CBLOG_GET_CMD},
	{ "create", "c", "Create database", CBLOG_CREATE_CMD},
	{ "version", "v", "Version of CBlog", CBLOG_VERSION_CMD},
	{ "path", "p", "Print cblog.cdb path", CBLOG_PATH_CMD},
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
	"interface",
	"port",
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
	create\t\t-- Create a new database.\n\
	add <file_post>\n\
	\t\t-- Add or modify a post\n\
	del <file_post>\n\
	\t\t-- Delete a post\n\
	get <file_post1> [file_post2 ...]\n\
	\t\t-- Get a post in text format\n\
	path\t\t-- Print cblog.cdb path\n\
	version\t\t-- Print cblog version\n", s);

	exit(1);
}

int
main(int argc, char *argv[])
{
	int i, ret;
	int type = -1;
	HDF *conf;
	NEOERR *neoerr;

	if (argc == 1)
		usage(argv[0]);

	if (access(CONFFILE, R_OK) != 0)
		err(1, "%s: can't access file", CONFFILE);

	neoerr = hdf_init(&conf);
	if (neoerr != STATUS_OK)
		errx(1, "%s: hdf_init hdf", CONFFILE);
	nerr_ignore(&neoerr);

	neoerr = hdf_read_file(conf, CONFFILE);
	if (neoerr != STATUS_OK)
		errx(1, "%s: hdf_read_file error", CONFFILE);
	nerr_ignore(&neoerr);

	if ((ret = check_conf(conf)) != -1)
		errx(1, "check_conf %s: %s is mandatory", CONFFILE, mandatory_config[ret]);

	/* find the type of the first command */
	for (i=0; cmd[i].name != NULL; i++) {
		if (EQUALS(argv[1], cmd[i].name) || EQUALS(argv[1], cmd[i].shortcut)) {
			type = cmd[i].cmdtype;
			break;
		}
	}

	/* search for cblog_cdb value, get from env or fallback to default otherwise */
	char	*s;
	size_t	slen;
	s = getenv("CBLOG_CDB");
	if (s == NULL) {
		/* fallback to default path */
		s = CDB_PATH"/cblog.sqlite";
	}
	/* check path's size */
	slen = strlen(s);
	if ((slen + 4) >= PATH_MAX) /* keep 4 char for .tmp */
		err(-1, "database path is too long.");
	/* setup cblog_cdb and cblog_cdb_tmp variable */
	(void)memcpy(cblog_cdb, s, slen + 1);

	switch(type) {
		case CBLOG_CREATE_CMD:
			cblogctl_create();
			break;
		case CBLOG_ADD_CMD:
			if (argc != 3)
				usage(argv[0]);

			cblogctl_add(argv[2]);
			break;
		case CBLOG_DEL_CMD:
			if (argc != 3)
				usage(argv[0]);

			cblogctl_del(argv[2]);
			break;
		case CBLOG_GET_CMD:
			if (argc <= 2 )
				usage(argv[0]);

			for (i=2; i < argc; i++)
				cblogctl_get(argv[i]);

			break;
		case CBLOG_VERSION_CMD:
			cblogctl_version();
			exit(0);
		case CBLOG_PATH_CMD:
			cblogctl_path();
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

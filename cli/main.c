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
	{ "list", "l", "Lists published posts", CBLOG_LIST_CMD},
	{ "add", "a", "Add or modify a post", CBLOG_ADD_CMD},
	{ "del", "d", "Delete a post", CBLOG_DEL_CMD},
	{ "get", "g", "Get a post in text format", CBLOG_GET_CMD},
	{ "set", "s", "Set some information in the post", CBLOG_SET_CMD},
	{ "info", "i", "Retrieve information about the post", CBLOG_INFO_CMD},
	{ "create", "c", "Create database", CBLOG_CREATE_CMD},
	{ "version", "v", "Version of CBlog", CBLOG_VERSION_CMD},
	{ "path", "p", "Print cblog.cdb path", CBLOG_PATH_CMD},
	{ NULL, NULL, NULL, 0},
};

static void
usage(const char *s)
{
	printf("Usage: %s cmd [option]\n\n\
			Example:\n\
			create\n\
			add file_post\n\
			del file_post\n\
			get file_post1 file_post2 ... file_postN\n\
			set file_post key=value\n\
			info file_post1 file_post2 ... file_postN\n\
			list\n\
			path\n\
			version\n", s);

	exit(1);
}

int
main(int argc, char *argv[])
{
	int i;
	int type = -1;

	if (argc == 1) 
		usage(argv[0]);

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
		s = CDB_PATH"/cblog.cdb";
	}
	/* check path's size */
	slen = strlen(s);
	if ((slen + 4) >= PATH_MAX) /* keep 4 char for .tmp */
		err(-1, "database path is too long.");
	/* setup cblog_cdb and cblog_cdb_tmp variable */
	(void)memcpy(cblog_cdb, s, slen + 1);
	(void)sprintf(cblog_cdb_tmp, "%s.tmp", cblog_cdb);

	if (type != CBLOG_CREATE_CMD && type != CBLOG_VERSION_CMD && type != CBLOG_PATH_CMD) {
	    if (access(cblog_cdb, F_OK) != 0)
		    errx(1, "%s must exists. Make '%s create' first.", cblog_cdb, argv[0]);
	}

	switch(type) {
		case CBLOG_CREATE_CMD:
			cblogctl_create();
			break;
		case CBLOG_LIST_CMD:
			cblogctl_list();
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
		case CBLOG_SET_CMD:
			if (argc != 4)
				usage(argv[0]);

			cblogctl_set(argv[2], argv[3]);
			break;
		case CBLOG_INFO_CMD:
			if (argc <= 2 )
				usage(argv[0]);

			for (i=2; i < argc; i++)
				cblogctl_info(argv[i]);

			break;
		case CBLOG_VERSION_CMD:
			cblogctl_version();
			exit(0);
		case CBLOG_PATH_CMD:
			cblogctl_path();
			exit(0);
		default:
			usage(argv[0]);
			/* NOT REACHED */
	}
	return EXIT_SUCCESS;
}
/* vim: set sw=4 sts=4 ts=4 : */

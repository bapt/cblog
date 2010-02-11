#include <stdio.h>
#include <err.h>
#include <stdlib.h>
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
	{ "get", "g", "Get a post in text format", CBLOG_GET_CMD},
	{ "set", "s", "Set some information in the post", CBLOG_SET_CMD},
	{ "info", "i", "Retrieve information about the post", CBLOG_INFO_CMD},
	{ "init", "I", "Initialize database", CBLOG_INIT_CMD},
};

static void
usage(const char *s)
{
	printf("Usage: %s cmd [option]\n\n\
			Example:\n\
			init\n\
			add file_post\n\
			get file_post1 file_post2 ... file_postN\n\
			set file_post key=value\n\
			info file_post1 file_post2 ... file_postN\n\
			list\n", s);

	exit(1);
}

int
main(int argc, char *argv[])
{
	int i;
	int type;

	if (argc == 1) 
		usage(argv[0]);

	/* find the type of the first command */
	for (i=0; cmd[i].name != NULL; i++) {
		if (EQUALS(argv[1], cmd[i].name) || EQUALS(argv[1], cmd[i].shortcut)) {
			type = cmd[i].cmdtype;
			break;
		}
	}

	switch(type) {
		case CBLOG_INIT_CMD:
			cblogctl_init();
			break;
		case CBLOG_LIST_CMD:
			cblogctl_list();
			break;
		case CBLOG_ADD_CMD:
			if (argc != 3)
				usage(argv[0]);

			cblogctl_add(argv[2]);
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
		default:
			usage(argv[0]);
			/* NOT REACHED */
	}
}

/* vim: set sw=4 sts=4 ts=4 : */

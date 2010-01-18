#include <stdio.h>
#include <err.h>
#include <stdlib.h>
#include "cblogctl.h"

static void
usage()
{
	errx(EXIT_FAILURE, "brrrt");
}

int
main(int argc, char *argv[])
{
	int i;
	int type;

	if (argc == 1) 
		usage();

	/* find the type of the first command */
	for (i=0; cmd[i].name != NULL; i++) {
		if (EQUALS(argv[1], cmd[i].name) || EQUALS(argv[1], cmd[i].shortcut)) {
			type = cmd[i].cmdtype;
			break;
		}
	}

	switch(type) {
	case CBLOG_LIST_CMD:
		cblogctl_list();
		break;
	case CBLOG_ADD_CMD:
		if (argc != 3)
			usage();

		cblogctl_add(argv[2]);
		break;
	case CBLOG_GET_CMD:
		if (argc <= 2 )
			usage();

		for (i=2; i < argc; i++)
			cblogctl_get(argv[i]);

		break;
	case CBLOG_SET_CMD:
		if (argc != 4)
			usage();

		cblogctl_set(argv[2], argv[3]);
		break;
	case CBLOG_INFO_CMD:
		if (argc <= 2 )
			usage();

		for (i=2; i < argc; i++)
			cblogctl_info(argv[i]);

		break;
	default:
		usage();
		/* NOT REACHED */
	}
}

/* vim: set sw=4 sts=4 ts=4 : */

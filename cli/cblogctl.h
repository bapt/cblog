#ifndef	CBLOG_CLI_CBLOGCTL_H
#define	CBLOG_CLI_CBLOGCTL_H

#include <string.h>

#define CBLOG_LIST_CMD 0
#define CBLOG_ADD_CMD 1
#define CBLOG_GET_CMD 2
#define CBLOG_SET_CMD 3
#define CBLOG_INFO_CMD 4
#define CBLOG_CREATE_CMD 5
#define CBLOG_VERSION_CMD 6
#define CBLOG_PATH_CMD 7
#define CBLOG_DEL_CMD 8

void cblogctl_create(void);
void cblogctl_list(void);
void cblogctl_info(const char *);
void cblogctl_get(const char *);
void cblogctl_add(const char *);
void cblogctl_del(const char *);
void cblogctl_version(void);
void cblogctl_path(void);

/* path the the CDB database file */
extern char	cblog_cdb[];

#endif	/* ndef CBLOG_CLI_CBLOGCTL_H */
/* vim: set sw=4 sts=4 ts=4 : */

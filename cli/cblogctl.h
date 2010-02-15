#include <string.h>

#define CBLOG_LIST_CMD 0
#define CBLOG_ADD_CMD 1
#define CBLOG_GET_CMD 2
#define CBLOG_SET_CMD 3
#define CBLOG_INFO_CMD 4
#define CBLOG_CREATE_CMD 5
#define CBLOG_VERSION_CMD 6


void cblogctl_create();
void cblogctl_list();
void cblogctl_info(const char *);
void cblogctl_get(const char *);
void cblogctl_add(const char *);
void cblogctl_set(const char *, char *);
void cblogctl_version();

/* vim: set sw=4 sts=4 ts=4 : */

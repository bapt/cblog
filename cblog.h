#ifndef	CBLOG_CLI_CBLOGCTL_H
#define	CBLOG_CLI_CBLOGCTL_H

#include <string.h>

static char *cblog_version = "CBlog 0.9.99";
static char *cblog_url = "https://github.com/bapt/cblog/";

#define CONFFILENAME "./cblog.conf"

#define CONFFILE ETCDIR"/"CONFFILENAME

#define EQUALS(string, needle) (strcmp(string, needle) == 0)
#define STARTS_WITH(string, needle) (strncasecmp(string, needle, strlen(needle)) == 0)

#define HDF_FOREACH(var, hdf, node)		    \
    for ((var) = hdf_get_child((hdf), node);	    \
	    (var);				    \
	    (var) = hdf_obj_next((var)))

#define DEFAULT_POSTS_PER_PAGES 5

#define CBLOG_GEN_CMD 0
#define CBLOG_ADD_CMD 1
#define CBLOG_VERSION_CMD 2
#define CBLOG_PATH_CMD 3

void cblogctl_add(const char *);
void cblogctl_version(void);
void cblogctl_path(HDF *conf);
void cblogctl_gen(HDF *conf);
int splitchr(char *, char);

#endif	/* ndef CBLOG_CLI_CBLOGCTL_H */

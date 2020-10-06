#ifndef	CBLOG_CLI_CBLOGCTL_H
#define	CBLOG_CLI_CBLOGCTL_H

#include <string.h>

#define CONFFILE ETCDIR"/cblog.conf"

#define HDF_FOREACH(var, hdf, node)		    \
    for ((var) = hdf_get_child((hdf), node);	    \
	    (var);				    \
	    (var) = hdf_obj_next((var)))

#define get_cblog_db(hdf) hdf_get_value(hdf, "db_path", "/var/db/cblog")

#define CBLOG_LIST_CMD 0
#define CBLOG_ADD_CMD 1
#define CBLOG_GET_CMD 2
#define CBLOG_SET_CMD 3
#define CBLOG_INFO_CMD 4
#define CBLOG_CREATE_CMD 5
#define CBLOG_VERSION_CMD 6
#define CBLOG_PATH_CMD 7
#define CBLOG_DEL_CMD 8
#define CBLOG_GEN_CMD 9

void cblogctl_create(void);
void cblogctl_list(void);
void cblogctl_info(const char *);
void cblogctl_get(const char *);
void cblogctl_add(const char *);
void cblogctl_del(const char *);
void cblogctl_version(void);
void cblogctl_path(void);
void cblogctl_gen(HDF *conf);

/* path the the CDB database file */
extern char	cblog_cdb[];

#endif	/* ndef CBLOG_CLI_CBLOGCTL_H */

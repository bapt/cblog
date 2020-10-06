#ifndef	CBLOG_CLI_CBLOGCTL_H
#define	CBLOG_CLI_CBLOGCTL_H

#include <string.h>

#define CONFFILE ETCDIR"/cblog.conf"

#define HDF_FOREACH(var, hdf, node)		    \
    for ((var) = hdf_get_child((hdf), node);	    \
	    (var);				    \
	    (var) = hdf_obj_next((var)))

#define get_cblog_db(hdf) hdf_get_value(hdf, "db_path", "/var/db/cblog")

#define CBLOG_GEN_CMD 0
#define CBLOG_ADD_CMD 1
#define CBLOG_VERSION_CMD 2
#define CBLOG_PATH_CMD 3

void cblogctl_add(const char *);
void cblogctl_version(void);
void cblogctl_path(HDF *conf);
void cblogctl_gen(HDF *conf);

/* path the the CDB database file */
extern char	cblog_cdb[];

#endif	/* ndef CBLOG_CLI_CBLOGCTL_H */

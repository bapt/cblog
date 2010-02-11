#include "cblog_utils.h"
#include <stdlib.h>
#include <err.h>

char *
db_get(struct cdb *cdb) {
	int		vpos, vlen;
	char	*val;

	vpos = cdb_datapos(cdb);
	vlen = cdb_datalen(cdb);

	val = malloc(vlen + 1);
	if (val == NULL)
		errx(1, "Unable to allocate memory");

	cdb_read(cdb, val, vlen, vpos);
	val[vlen] = '\0';

	return val;
}

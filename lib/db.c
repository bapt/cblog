#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <err.h>
#include <assert.h>

#include "cblog_utils.h"

int
sql_exec(sqlite3 *s, const char *sql, ...)
{
	va_list ap;
	const char *sql_to_exec;
	char *sqlbuf = NULL;
	char *errmsg;
	int ret = -1;
	
	assert(s != NULL);
	assert(sql != NULL);

	if (strchr(sql, '%') != NULL) {
		va_start(ap, sql);
		sqlbuf = sqlite3_vmprintf(sql, ap);
		va_end(ap);
		sql_to_exec = sqlbuf;
	} else {
		sql_to_exec = sql;
	}

	if (sqlite3_exec(s, sql_to_exec, NULL, NULL, &errmsg) != SQLITE_OK) {
		warnx("sqlite: %s", errmsg);
		sqlite3_free(errmsg);
		goto cleanup;
	}

	ret = 0;
cleanup:
	if (sqlbuf != NULL)
		sqlite3_free(sqlbuf);

	return (ret);
}

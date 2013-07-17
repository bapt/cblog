#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
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
		goto cleanup;
	}

	ret = 0;
cleanup:
	if (sqlbuf != NULL)
		sqlite3_free(sqlbuf);

	return (ret);
}

int
sql_text(sqlite3 *s, char **dest, const char *sql, ...)
{
	va_list ap;
	sqlite3_stmt *stmt = NULL;
	const char *sql_to_exec;
	char *sqlbuf = NULL;
	int ret = 0;

	assert(s != NULL);
	assert(sql != NULL);
	assert(dest != NULL);

	if (strchr(sql, '%') != NULL) {
		va_start(ap, sql);
		sqlbuf = sqlite3_vmprintf(sql, ap);
		va_end(ap);
		sql_to_exec = sqlbuf;
	} else {
		sql_to_exec = sql;
	}

	if (sqlite3_prepare_v2(s, sql_to_exec, -1, &stmt, 0) != SQLITE_OK) {
		warnx("sqlite: %s", sqlite3_errmsg(s));
		ret = 1;
		goto cleanup;
	}

	if (sqlite3_step(stmt) == SQLITE_ROW)
		asprintf(dest, "%s", sqlite3_column_text(stmt, 0));

cleanup:
	if (sqlbuf != NULL)
		sqlite3_free(sqlbuf);
	if (stmt != NULL)
		sqlite3_finalize(stmt);

	return (ret);
}

int
sql_int(sqlite3 *s, int64_t *dest, const char *sql, ...)
{
	va_list ap;
	sqlite3_stmt *stmt = NULL;
	const char *sql_to_exec;
	char *sqlbuf = NULL;
	int ret = 0;

	assert(s != NULL);
	assert(sql != NULL);
	assert(dest != NULL);

	if (strchr(sql, '%') != NULL) {
		va_start(ap, sql);
		sqlbuf = sqlite3_vmprintf(sql, ap);
		va_end(ap);
		sql_to_exec = sqlbuf;
	} else {
		sql_to_exec = sql;
	}

	if (sqlite3_prepare_v2(s, sql_to_exec, -1, &stmt, 0) != SQLITE_OK) {
		warnx("sqlite: %s", sqlite3_errmsg(s));
		ret = 1;
		goto cleanup;
	}

	if (sqlite3_step(stmt) == SQLITE_ROW)
		*dest = sqlite3_column_int64(stmt, 0);

cleanup:
	if (sqlbuf != NULL)
		sqlite3_free(sqlbuf);
	if (stmt != NULL)
		sqlite3_finalize(stmt);

	return (ret);
}

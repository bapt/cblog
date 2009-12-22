/* $Id: tools.h,v 1.8 2009/12/03 11:29:27 imil Exp $ */

/*
 * Copyright (c) 2009 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Emile "iMil" Heitor <imil@NetBSD.org> .
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#ifndef _TOOLS_H
#define _TOOLS_H

#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>
#include <limits.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <err.h>
#include <ctype.h>
#include <sys/utsname.h>

#if WANT_NBCOMPAT_H /* needed for strsep on SunOS < 5.11 */
#include <nbcompat.h>
#include <nbcompat/string.h>
#endif

#define MAXLEN LINE_MAX
#define MIDLEN 256
#define SMLLEN 32
#define D_WARN 5
#define D_INFO 10
#define T_FALSE 0
#define T_TRUE 1
#define STR_FORWARD 0
#define STR_BACKWARD 1

#define R_READ(fd, buf, len)						\
	if (read(fd, buf, len) < 0) {					\
		warn("read()");								\
		pthread_exit(NULL);							\
	}

#define R_CLOSE(fd)							\
	do {									\
		close(fd);							\
		pthread_exit(NULL);					\
	} while (/* CONSTCOND */ 0)

#define XMALLOC(elm, size)						\
	do {										\
		elm = malloc(size);						\
		if (elm == NULL)						\
			err(1, "can't allocate memory\n");	\
		memset(elm, 0, size);					\
	} while (/* CONSTCOND */ 0)

#define XSTRDUP(dest, src)												\
	do {																\
		if (src == NULL)												\
			dest = NULL;												\
		else {															\
			dest = strdup(src);											\
			if (dest == NULL)											\
				err(1, "can't strdup %s\n", src);						\
		}																\
	} while (/* CONSTCOND */ 0)

#define XREALLOC(elm, size)												\
	do {																\
		void *telm;														\
		if (elm == NULL)												\
			XMALLOC(elm, size);											\
		else {															\
			telm = realloc(elm, size);									\
			if (telm == NULL)											\
				err(1, "can't allocate memory\n");						\
			elm = telm;													\
		}																\
	} while (/* CONSTCOND */ 0)

#define DSTSRC_CHK(dst, src)					\
	if (dst == NULL) {							\
		warn("NULL destination");				\
		break;									\
	}											\
	if (src == NULL) {							\
		warn("NULL source");					\
		break;									\
	}


#define XSTRCPY(dst, src)						\
	do {										\
		DSTSRC_CHK(dst, src);					\
		strcpy(dst, src);						\
	} while (/* CONSTCOND */ 0)

#define XSTRCAT(dst, src)						\
	do {										\
		DSTSRC_CHK(dst, src);					\
		strcat(dst, src);						\
	} while (/* CONSTCOND */ 0)

#define XSTRMEMCAT(dst, src, size)										\
	do {																\
		XREALLOC(dst, size);											\
		XSTRCAT(dst, src);												\
	} while (/* CONSTCOND */ 0)

#define XFREE(elm)		   					\
	do {									\
		if (elm != NULL) {					\
			free(elm);						\
			elm = NULL;						\
		}									\
	} while (/* CONSTCOND */ 0)

#define XSNPRINTF(dst, size, fmt...) 			\
	do {										\
		char *pdst;								\
		pdst = safe_snprintf(size, fmt);		\
		XFREE(dst);								\
		dst = pdst;								\
} while (/* CONSTCOND */ 0)

typedef uint8_t T_Bool;

#ifndef SLIST_FOREACH_MUTABLE /* from DragonFlyBSD */
#define SLIST_FOREACH_MUTABLE(var, head, field, tvar)		\
	for ((var) = SLIST_FIRST((head));						\
	    (var) && ((tvar) = SLIST_NEXT((var), field), 1);	\
		 (var) = (tvar))
#endif

extern int trimcr(char *);
extern char **splitstr(char *, const char *);
extern void free_list(char **);
extern int min(int, int);
extern int max(int, int);
extern int listlen(const char **);
extern char **exec_list(const char *, const char *);
extern T_Bool is_listed(const char **, const char *);
extern void do_log(const char *, const char *, ...);
extern void trunc_str(char *, char, int);
extern char *safe_snprintf(int, const char *, ...);
extern char *strreplace(char *, const char *, const char *);
extern char *getosarch(void);
extern char *getosrelease(void);

#endif

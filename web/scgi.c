/*-
 * Copyright (c) 2014 Baptiste Daroussin <bapt@FreeBSD.org>
 * All rights reserved.
 *~
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *~
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/param.h>

#include <stdlib.h>
#define _WITH_DPRINTF
#include <stdio.h>
#include <ctype.h>
#include <unistd.h>

#include "cblogweb.h"

struct kv **
scgi_parse(char *raw) {
	int l;
	const char *err = NULL;
	const char *key;
	char *walk;
	struct kv **headers;
	struct kv *header;

	walk = raw;
	/* read length */
	while (*walk && isdigit(*walk))
		walk++;
	*walk = '\0';

	l = strtonum(raw, 0, INT_MAX, &err);
	if (err != NULL)
		return (NULL);
	walk++;
	raw = walk;

	while (walk - raw < l) {
		key = walk;
		walk += strlen(walk);
		o = ucl_object_fromstring_common(walk, strlen(walk), UCL_STRING_PARSE_INT);
		header = ucl_object_insert_key(header, o, key, strlen(key), false);
		walk++;
	}

	o = ucl_object_find_key(header, "CONTENT_LENGTH");
	if (o == NULL || o->type != UCL_INT) {
		ucl_object_unref(header);
		return (NULL);
	}

	raw++;
	parser = ucl_parser_new(0);
	if (!ucl_parser_add_chunk(parser, (const unsigned char *)raw, ucl_object_toint(o))) {
		ucl_object_unref(header);
		return (NULL);
	}
	scgi = ucl_object_insert_key(NULL, header, "header", 6, false);
	scgi = ucl_object_insert_key(scgi, ucl_parser_get_object(parser), "data", 4, false);

	ucl_parser_free(parser);

	return (scgi);
}

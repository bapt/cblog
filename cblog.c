/*
 * Copyright (c) 2009, Bapt <bapt@etoilebsd.net>
 * All rights reserved.
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the author nor the names of any co-contributors
 *       may be used to endorse or promote products derived from this software 
 *       without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE REGENTS AND CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
      * * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#define _GNU_SOURCE

#include "buffer.h"
#include "cblog.h"
#include "cblog_posts.h"
#include "cblog_conf.h"

HDF *config;

char *mandatory_config[] = { 
	"title", 
	"url", 
	"dateformat", 
	"hdf.loadpaths.tpl", 
	"data.dir", 
	"cache.dir", 
	"theme" , 
	"posts_per_pages" };

struct Status *cblog_status;
HDF *config;

void
prepare_index(HDF* hdf)
{
      	get_posts(hdf, NULL, NULL);
}

void
prepare_post(HDF *hdf, char **path)
{
	int i;
	for (i=0; path[i] != NULL; i++);
	get_posts(hdf, NULL, path[i-1]); 
}

void
prepare_posts_per_tag(HDF *hdf ,char **tags)
{
	struct buf *b;
	int i;
	b = bufnew(READ_UNIT);
	for (i=1; tags[i] != NULL; i++) {
		bufputs(b,tags[i]);
		bufputs(b,"/");
	}
	bufnullterm(b);
	char *request=b->data;
	request[strlen(request) - 1]='\0';
      	get_posts(hdf, request, NULL);
	bufrelease(b);
}

void
show_not_found(HDF *hdf)
{
	hdf_set_valuef(hdf,"Page.title=%s","<h1>Unable find the requested page</h1>");
}


int
parse_conf()
{
	NEOERR *neoerr;
	STRING neoerr_str;
	hdf_init(&config);
	neoerr = hdf_read_file(config, CONFFILE);
	if (neoerr != STATUS_OK) {
		nerr_error_string(neoerr, &neoerr_str);
		cblog_err(-1, neoerr_str.buf);
		string_clear(&neoerr_str);
		return -1;
	}
	return 0;
}

int 
reload_conf(HDF *hdf)
{
	int i=0;
/*	HDF *config_tmp;
	hdf_init(&config_tmp); */
/*	if (config == NULL)
		hdf_init(&config);*/
//	struct stat filestat;
/*	stat(CONFFILE, &filestat);
	if (cblog_status == NULL) {
		cblog_status = malloc(sizeof(struct Status *));
		cblog_status->conf_mdate=filestat.st_mtime;
		if(parse_conf(config_tmp)) {
			hdf_destroy(&config_tmp);
			return -1;
		}	
		*/
	parse_conf(hdf);
/*	} else if (cblog_status->conf_mdate <= filestat.st_mtime) {
		if (parse_conf(config_tmp)) {
			hdf_destroy(&config_tmp);
			return -1;	
		}
	} else {
		hdf_destroy(&config_tmp);
		return 0;
	} */
	
	for (i=0; i < 8; i++) {
		if(hdf_get_valuef(hdf,mandatory_config[i]) == NULL) {
			cblog_err(2, "%s is a mandatory option, keeping the old config", mandatory_config[i]);
//			hdf_destroy(&config_tmp);
			return -1;
		}

	}
	/* Only if everything is ok we reload the conf */
//	hdf_copy(config,"" , config_tmp);
//	hdf_destroy(&config_tmp);
	return 0;
}

int
cblog_main()
{
	CGI *cgi;
	NEOERR *neoerr;
	STRING neoerr_str;
	char **requesturi;
	char **path;

	int type;
	char *theme;
	char *req;

	type = TYPE_DATE;

	cgi_init(&cgi, NULL);
	cgi_parse(cgi);

	//reload_conf(cgi->hdf);
	hdf_copy(cgi->hdf, "", config);
	theme=strdup(hdf_get_valuef(cgi->hdf, "theme"));
	XSTRDUP(req, get_cgi_str(cgi->hdf,"RequestURI"));
	requesturi=splitstr(req, "?");
	path=splitstr(requesturi[0],"/");
	/* check if there is /... in the query URI */
	if(path[0]==NULL) {
		prepare_index(cgi->hdf);
	} else if (STARTS_WITH(path[0], "tag")) {
		prepare_posts_per_tag(cgi->hdf, path);
	} else if (STARTS_WITH(path[0], "post")) {
		prepare_post(cgi->hdf, path);
/*	} else if (STARTS_WITH(path[0], "page")) {
		prepare_static_page(cgi->hdf, path);*/
/*	} else if (STARTS_WITH(path[0], "index.rss")) {
		prepare_feed(cgi->hdf);
		free(theme);
		theme=strdup(get_feed_tpl(cgi->hdf, "rss"));
	} else if (STARTS_WITH(path[0], "index.atom")) {
		prepare_feed(cgi->hdf);
		free(theme);
		theme=strdup(get_feed_tpl(cgi->hdf, "atom"));*/
	} else {
		show_not_found(cgi->hdf);
	}
	neoerr = cgi_display(cgi, theme);
	if (neoerr != STATUS_OK) {
		nerr_error_string(neoerr, &neoerr_str);
		cblog_err(-1, neoerr_str.buf);
		string_clear(&neoerr_str);
	}
	/* cleanup every thing */
	free_list(path);
	free_list(requesturi);
	XFREE(req);
	XFREE(theme);
	return 0;
}

void
clean_conf()
{
	hdf_destroy(&config);
}

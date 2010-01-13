#include <fcntl.h>
#include <unistd.h>
#include <cdb.h>
#include <ctype.h>
#include <stdbool.h>
#include <string.h>
#include <syslog.h>
#include "../cli/cblogctl.h"
#include <err.h>

#include "cblog_cgi.h"


static char *mandator_config[] = {
    "title",
    "url",
    "dateformat",
    "hdf.loadpaths.tpl",
    "db_path",
    "theme",
    "post_per_pages",
    NULL,
};

static struct pages {
    const char *name;
    int type;
} page[] = {
    { "/post", CBLOG_POST },
    { "/tag", CBLOG_TAG },
    { "/index.rss", CBLOG_RSS },
    { "/index.atom", CBLOG_ATOM },
    { NULL, -1 },
};

struct posts {
    char *name;
    time_t ctime;
};

struct tags {
    char *name;
    int count;
};

int
sort_by_name(const void *a, const void *b)
{
    struct tags *ta = *((struct tags **)a);
    struct tags *tb = *((struct tags **)b);

    return strcasecmp(ta->name, tb->name);
}


int
sort_by_ctime(const void *a, const void *b)
{
    struct posts *post_a = *((struct posts **)a);
    struct posts *post_b = *((struct posts **)b);

    return post_b->ctime - post_a ->ctime;
}

char *
trimspace(char *str)
{
    char *line=str;
    /* remove spaces at the beginning */
    while(true) {
	if(isspace(line[0]))
	    line++;
	else
	    break;
    }
    /* remove spaces at the end */
    while(true) {
	if(isspace(line[strlen(line) - 1]))
	    line[strlen(line) - 1]='\0';
	else
	    break;
    }
    return line;
}

char *
db_get(struct cdb *cdb) {
    int vpos, vlen;
    char *val;

    vpos = cdb_datapos(cdb);
    vlen = cdb_datalen(cdb);

    XMALLOC(val, vlen + 1);
    cdb_read(cdb, val, vlen, vpos);
    val[vlen]='\0';

    return val;
}

int
splitchr(char *str, char sep)
{
    char *next;
    char *buf=str;
    int nbel=0;
    while (( next = strchr(buf, sep)) != NULL) {
	nbel++;
	buf=next;
	buf[0]='\0';
	buf++;
    }
    return nbel;

}

void
cblog_err(int eval, const char * message, ...)
{
    va_list args;
    va_start(args, message);
    vsyslog(LOG_ERR,message, args);
}


char *
time_to_str(time_t source, char *format)
{
    char formated_date[256];
    struct tm *ptr;

    ptr = localtime(&source);
    strftime(formated_date, 256, format, ptr);
    return strdup(formated_date);
}


void
add_post_to_hdf(HDF *hdf, struct cdb *cdb, char *name, int pos)
{
	int i, j;
	char *key, *val;

	hdf_set_valuef(hdf, "Posts.%i.filename=%s", pos, name);
	for (i=0; field[i] != NULL; i++) {
	    asprintf(&key,"%s_%s", name, field[i]);
	    cdb_find(cdb, key, strlen(key));
	    val = db_get(cdb);
	    char *val_to_free=val;
	    XFREE(key);

	    if (EQUALS(field[i], "tags")) {
		int nbel = splitchr(val, ',');
		for (j=0; j <= nbel; j++) {
		    size_t next = strlen(val);
		    char *valtrimed = trimspace(val);
		    hdf_set_valuef(hdf, "Posts.%i.tags.%i.name=%s", pos, j, valtrimed);
		    val+=next+1;
		}
	    } else if (EQUALS(field[i], "ctime")) {
		set_post_date(hdf, pos, val);
	    } else
		hdf_set_valuef(hdf, "Posts.%i.%s=%s", pos, field[i], val);

	    XFREE(val_to_free);
	}
}

void
set_tags(HDF *hdf)
{
    int db;
    int i, j, nbtags=0;
    struct cdb cdb;
    struct cdb_find cdbf;
    char *val, *key;
    bool found;
    struct tags **taglist=NULL;

    db = open(get_cblog_db(hdf), O_RDONLY);
    cdb_init(&cdb, db);

    cdb_findinit(&cdbf, &cdb, "posts", 5);
    while (cdb_findnext(&cdbf) > 0) {
	val=db_get(&cdb);
	asprintf(&key, "%s_tags", val);
	XFREE(val);

	cdb_find(&cdb, key, strlen(key));
	val=db_get(&cdb);
	XFREE(key);
	char *val_to_free=val;
	int nbel = splitchr(val, ',');
	for (i=0; i <= nbel; i++) {
	    found=false;
	    size_t next = strlen(val);
	    char *tagcmp = trimspace(val);

	    for (j=0; j < nbtags; j++) {
		if(EQUALS(tagcmp, taglist[j]->name)) {
		    found=true;
		    taglist[j]->count++;
		    break;
		}
	    }
	    if(!found) {
		nbtags++;
		XREALLOC(taglist, nbtags * sizeof(struct tags*));
		struct tags *tag;
		XMALLOC(tag, sizeof(struct tags));
		XSTRDUP(tag->name, tagcmp);
		tag->count=1;
		taglist[nbtags - 1] = tag;
	    }
	    val+= next + 1;
	}
	XFREE(val_to_free);
    }

    qsort(taglist, nbtags, sizeof(struct tags *), sort_by_name);

    for (i=0; i<nbtags; i++) {
	set_tag_name(hdf, i, taglist[i]->name);
	set_tag_count(hdf, i, taglist[i]->count);
	XFREE(taglist[i]->name);
	XFREE(taglist[i]);
    }
    XFREE(taglist);

    cdb_free(&cdb);
    close(db);
}

void
build_post(HDF *hdf, char *postname)
{
    int db;
    struct cdb cdb;
    char *key;

    db = open(get_cblog_db(hdf), O_RDONLY);
    cdb_init(&cdb, db);

    asprintf(&key, "%s_title", postname);
    if (cdb_find(&cdb, key, strlen(key)) > 0)
	add_post_to_hdf(hdf, &cdb, postname, 0);
    XFREE(key);

    cdb_free(&cdb);
    close(db);
}

void
build_index(HDF *hdf, char *tagname)
{
    int db;
    int first_post=0, nb_posts=0, max_post, total_posts=0;
    int nb_pages=0, page;
    int i, j=0, k;
    struct cdb cdb;
    struct cdb_find cdbf;
    char *val, *key;
    struct posts **posts = NULL;

    max_post = hdf_get_int_value(hdf, "posts_per_pages", DEFAULT_POSTS_PER_PAGES);
    page = hdf_get_int_value(hdf, "Query.page", 1);
    first_post = (page * max_post) - max_post;

    db = open(get_cblog_db(hdf), O_RDONLY);
    cdb_init(&cdb, db);

    cdb_findinit(&cdbf, &cdb, "posts", 5);
    while (cdb_findnext(&cdbf) > 0) {
	struct posts *post = NULL;

	total_posts++;

	XREALLOC(posts, total_posts * sizeof(struct posts *));
	XMALLOC(post, sizeof(struct posts));

	/* fetch the post key name */
	post->name = db_get(&cdb);
	
	asprintf(&key, "%s_ctime", post->name);
	if ( cdb_find(&cdb, key, strlen(key)) > 0){
	    val = db_get(&cdb);
	    post->ctime = (time_t) strtol(val, NULL, 10);
	    XFREE(val);

	} else
	    post->ctime = time(NULL);

	XFREE(key);
	posts[total_posts - 1 ] = post;
    }

    qsort(posts, total_posts, sizeof(struct posts *), sort_by_ctime);

    if ( tagname != NULL ) {
	for (i=0; i < total_posts; i++) {
	    asprintf(&key, "%s_tags", posts[i]->name);

	    if (cdb_find(&cdb, key, strlen(key)) > 0) {
		val = db_get(&cdb);
		char *tag = val;
		int nbel = splitchr(val, ',');
		for (k=0; k<= nbel; k++) {
		    size_t next = strlen(val);
		    char *tagcmp = trimspace(val);
		    if (EQUALS(tagname, tagcmp)) {
			j++;
			if ((j >=  first_post) && (nb_posts < max_post)) {
			    add_post_to_hdf(hdf, &cdb, posts[i]->name, j);
			    nb_posts++;
			    break;
			}
		    }
		    val+=next + 1;
		}
		XFREE(tag);
	    }
	    XFREE(posts[i]->name);
	    XFREE(posts[i]);
	    XFREE(key);
	}
    } else {
	for (i=0; i < total_posts; i++) {
	    j++;
	    if ( (j >= first_post) && (nb_posts < max_post) ) {
		add_post_to_hdf(hdf, &cdb, posts[i]->name, j);
		nb_posts++;
	    }
	    XFREE(posts[i]->name);
	    XFREE(posts[i]);
	}
    }
    XFREE(posts);

    nb_pages = j / max_post;
    if (j % max_post > 0)
	nb_pages++;

    set_nb_pages(hdf, nb_pages);

    cdb_free(&cdb);
    close(db);
}

void
cblogcgi()
{
    CGI *cgi;
    NEOERR *neoerr;
    STRING neoerr_str;
    char *requesturi;
    int type, i;
    /* read the configuration file */

    type=CBLOG_ROOT;

    neoerr = cgi_init(&cgi, NULL);
    nerr_ignore(&neoerr);

    neoerr = cgi_parse(cgi);
    nerr_ignore(&neoerr);

    neoerr = hdf_read_file(cgi->hdf, CONFFILE);
    if (neoerr != STATUS_OK) {
	type=CBLOG_ERR;
	cblog_err(-1, "Error while reading configuration file");
    }
    nerr_ignore(&neoerr);

    requesturi = get_cgi_str(cgi->hdf, "RequestURI");
    splitchr(requesturi, '?');

    /* find the beginig of the request */
    if ( requesturi != NULL)
	if (requesturi[1] != '\0') {
	    for (i = 0; page[i].name != NULL; i++) {
		if (STARTS_WITH(requesturi, page[i].name)) {
		    type=page[i].type;
		    break;
		}
	    }
	    if ( type == CBLOG_ROOT )
		hdf_set_valuef(cgi->hdf, "err_msg=Unknown request: %s", requesturi);
	}

    switch (type) {
	case CBLOG_POST:
	    requesturi++;
	    while(requesturi[0] != '/')
		requesturi++;
	    requesturi++;
	    build_post(cgi->hdf, requesturi);
	    break;
	case CBLOG_TAG:
	    requesturi++;
	    while (requesturi[0] != '/')
		requesturi++;
	    requesturi++;
	    build_index(cgi->hdf, requesturi);
	    break;
	case CBLOG_RSS:
	    hdf_set_valuef(cgi->hdf, "Query.feed=rss");
	    build_index(cgi->hdf, NULL);
	    break;
	case CBLOG_ATOM:
	    hdf_set_valuef(cgi->hdf, "Query.feed=atom");
	    build_index(cgi->hdf, NULL);
	    break;
	case CBLOG_ROOT:
	    build_index(cgi->hdf, NULL);
	    break;
    }


    if (hdf_get_value(cgi->hdf, "err_msg", NULL) == NULL) {
	if ( EQUALS(hdf_get_value(cgi->hdf, "Query.feed", "fail"), "rss")) {
	    HDF *hdf;
	    HDF_FOREACH(hdf, cgi->hdf, "Posts") {
		int date = hdf_get_int_value(hdf, "date", time(NULL) );
		char * time_str = time_to_str(date, DATE_FEED);
		hdf_set_valuef(hdf, "date=%s", time_str);
		XFREE(time_str);
	    }
	    neoerr = cgi_display(cgi, hdf_get_value(cgi->hdf, "feed.atom", "rss.cs"));
	} else if ( EQUALS(hdf_get_value(cgi->hdf, "Query.feed", "fail"), "atom")) {
	    HDF *hdf;
	    HDF_FOREACH(hdf, cgi->hdf, "Posts") {
		int date = hdf_get_int_value(hdf, "date", time(NULL) );
		char * time_str = time_to_str(date, DATE_FEED);
		hdf_set_valuef(hdf, "date=%s", time_str);
		XFREE(time_str);
	    }
	    neoerr = cgi_display(cgi, hdf_get_value(cgi->hdf, "feed.atom", "atom.cs"));
	} else {
	    HDF *hdf;
	    set_tags(cgi->hdf);
	    HDF_FOREACH(hdf, cgi->hdf, "Posts") {
		int date = hdf_get_int_value(hdf, "date", time(NULL) );
		char *date_format = get_dateformat(hdf);
		char * time_str = time_to_str(date, date_format);
		hdf_set_valuef(hdf, "date=%s", time_str);
		XFREE(time_str);
	    }
	    neoerr = cgi_display(cgi, get_cgi_theme(cgi->hdf));
	}
    }
    
    if (neoerr != STATUS_OK) {
	nerr_error_string(neoerr, &neoerr_str);
	cblog_err(-1, neoerr_str.buf);
	string_clear(&neoerr_str);
    }
    nerr_ignore(&neoerr);
    cgi_destroy(&cgi);
}

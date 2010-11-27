#ifndef	CBLOG_CGI_CBLOG_CGI_H
#define	CBLOG_CGI_CBLOG_CGI_H

#include <fcgi_stdio.h>
#include <ClearSilver.h>

#define CBLOG_POST 0
#define CBLOG_TAG 1
#define CBLOG_ATOM 2
#define CBLOG_ROOT 3
#define CBLOG_ERR 4
#define CBLOG_YYYY 5
#define CBLOG_YYYY_MM 6
#define CBLOG_YYYY_MM_DD 7

#define CRITERIA_TAGNAME 1
#define CRITERIA_TIME_T 2
#define CRITERIA_FEEDS 3

#define DEFAULT_POSTS_PER_PAGES 10
#define DEFAULT_THEME "default"
#define DEFAULT_DB CDB_PATH"/cblog.cdb"

#define DATE_FEED "%a, %d %b %Y %H:%M:%S %z"

#define get_cgi_str(hdf,name) hdf_get_value(hdf, "CGI."name, NULL)
#define get_query_str(hdf,name) hdf_get_value(hdf, "Query."name, NULL)
#define get_cgi_theme(hdf) hdf_get_value(hdf, "theme", DEFAULT_THEME)
#define get_dateformat(hdf)  hdf_get_value(hdf, "dateformat", "%d/%m/%Y")
#define get_cblog_db(hdf) hdf_get_value(hdf, "db_path", DEFAULT_DB)

#define set_post_date(hdf, pos, date) hdf_set_valuef(hdf, "Posts.%i.date=%s", pos, date)
#define set_nb_pages(hdf, pages) hdf_set_valuef(hdf, "nbpages=%i", pages)
#define set_tag_name(hdf, pos, name)  hdf_set_valuef(hdf, "Tags.%i.name=%s", pos, name)
#define set_tag_count(hdf, pos, count) hdf_set_valuef(hdf, "Tags.%i.count=%i", pos, count)

#define CONFFILE ETCDIR"/cblog.conf"

#define HDF_FOREACH(var, hdf, node)		    \
    for ((var) = hdf_get_child((hdf), node);	    \
	    (var);				    \
	    (var) = hdf_obj_next((var)))

void	cblogcgi(HDF *conf);
int		get_comments_count(char *postname);
void	get_comments(HDF *hdf, char *postname);
void	set_comment(HDF *hdf, char *postname);
void	cblog_err(int eval, const char * message, ...);

#endif	/* ndef CBLOG_CGI_CBLOG_CGI_H */

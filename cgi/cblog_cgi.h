#include <ClearSilver.h>
#define CBLOG_POST 0
#define CBLOG_TAG 1
#define CBLOG_RSS 2
#define CBLOG_ATOM 3
#define CBLOG_ROOT 4
#define CBLOG_ERR 5

#define DEFAULT_POSTS_PER_PAGES 10
#define DEFAULT_THEME "default"
#define DEFAULT_DB "/usr/local/cblog/cblog.cdb"

#define DATE_FEED "%a, %d %b %Y %H:%M:%S %z"

#define get_cgi_str(hdf,name) hdf_get_value(hdf, "CGI."name, NULL)
#define get_cgi_theme(hdf) hdf_get_value(hdf, "theme", DEFAULT_THEME)
#define get_dateformat(hdf)  hdf_get_value(hdf, "dateformat", "%d/%m/%Y")
#define get_cblog_db(hdf) hdf_get_value(hdf, "db_path", DEFAULT_DB)

#define set_post_date(hdf, pos, date) hdf_set_valuef(hdf, "Posts.%i.date=%s", pos, date)
#define set_nb_pages(hdf, pages) hdf_set_valuef(hdf, "nbpages=%i", pages)
#define set_tag_name(hdf, pos, name)  hdf_set_valuef(hdf, "Tags.%i.name=%s", pos, name)
#define set_tag_count(hdf, pos, count) hdf_set_valuef(hdf, "Tags.%i.count=%i", pos, count)

#define CONFFILE ETCDIR"/cblog.conf"

#define EQUALS(string, needle) (strcasecmp(string, needle) == 0)
#define STARTS_WITH(string, needle) (strncasecmp(string, needle, strlen(needle)) == 0)

#define HDF_FOREACH(var, hdf, node)		    \
    for ((var) = hdf_get_child((hdf), node);	    \
	    (var);				    \
	    (var) = hdf_obj_next((var)))

void cblogcgi();

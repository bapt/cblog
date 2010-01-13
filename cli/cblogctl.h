#include <string.h>

#define CBLOG_LIST_CMD 0
#define CBLOG_ADD_CMD 1
#define CBLOG_GET_CMD 2
#define CBLOG_SET_CMD 3
#define CBLOG_INFO_CMD 4

#define CACHE "/usr/local/cblog"

#define EQUALS(string, needle) (strcasecmp(string, needle) == 0)
#define STARTS_WITH(string, needle) (strncasecmp(string, needle, strlen(needle)) == 0)

static struct command {
    const char *name;
    const char *shortcut;
    const char *descr;
    const int cmdtype;
} cmd[] = {
    { "list", "l", "Lists published posts", CBLOG_LIST_CMD},
    { "add", "a", "Add or modify a post", CBLOG_ADD_CMD},
    { "get", "g", "Get a post in text format", CBLOG_GET_CMD},
    { "set", "s", "Set some information in the post", CBLOG_SET_CMD},
    { "info", "i", "Retrieve information about the post", CBLOG_INFO_CMD},
};

static char *field[] = {
    "title",
    "tags",
    "source",
    "html",
    "ctime",
    "published",
    "comments",
    NULL
};

void cblogctl_list();
void cblogctl_info(const char *);
void cblogctl_get(const char *);
void cblogctl_add(const char *);
void cblogctl_set(const char *, char *);

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

#define XFREE(elm)		   					\
	do {									\
		if (elm != NULL) {					\
			free(elm);						\
			elm = NULL;						\
		}									\
	} while (/* CONSTCOND */ 0)

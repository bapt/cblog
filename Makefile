PROG=	cblog.cgi
SRCS+=	main.c utils.c cblog.c cblog_posts.c cblog_log.c tools.c array.c  buffer.c markdown.c renderers.c
PREFIX?=	/usr/local
DEFINE=-DETCDIR=\"${PREFIX}/etc\"
INCLUDE=-I${PREFIX}/include -I${PREFIX}/include/ClearSilver -I.
CFLAGS+=	-Wall -ggdb -O0 ${INCLUDE} ${DEFINE}
LDADD+=	-L${PREFIX}/lib -lz -lneo_cgi -lneo_cs -lneo_utl -lfcgi -lcdb

.include <bsd.prog.mk>

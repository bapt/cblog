PROG=	cblog.cgi
SRCS+=	utils.c cblog.c cblog_log.c tools.c comments.c  array.c  buffer.c markdown.c renderers.c
PREFIX?=	/usr/local
DEFINE=-DETCDIR=\"${PREFIX}/etc\"
INCLUDE=-I${PREFIX}/include -I${PREFIX}/include/ClearSilver
CFLAGS+=	-Wall -g ${INCLUDE} ${DEFINE}
LDADD+=	-L${PREFIX}/lib -lz -lneo_cgi -lneo_cs -lneo_utl

.include <bsd.prog.mk>

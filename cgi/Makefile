#CC= 	clang
#LD= 	llvm-ld
PROG=	cblog.cgi
SRCS+=	main.c cblog_cgi.c
PREFIX?=	/usr/local
DEFINE=-DETCDIR=\"${PREFIX}/etc\"
INCLUDE=-I${PREFIX}/include -I${PREFIX}/include/ClearSilver -I.
CFLAGS+=	-Wall -ggdb -O0 ${INCLUDE} ${DEFINE}
LDADD+=	-L${PREFIX}/lib -lz -lneo_cgi -lneo_cs -lneo_utl -lfcgi -lcdb

.include <bsd.prog.mk>

#CC= 	clang
#LD= 	llvm-ld
PROG=	cblog.cgi
SRCS+=	main.c cblog_cgi.c cblog_comments.c
PREFIX?=	/usr/local
DEFINE=-DETCDIR=\"${PREFIX}/etc\"
INCLUDE=-I${PREFIX}/include -I${PREFIX}/include/ClearSilver -I. -I../lib
CDB_PATH?=	/usr/local/cblog
CFLAGS+=	-Wall -ggdb -O0 ${INCLUDE} ${DEFINE} -DCDB_PATH=\"${CDB_PATH}\" -Werror
LDADD+=	-L${PREFIX}/lib -lz -lneo_cgi -lneo_cs -lneo_utl -lfcgi -lcdb -L../lib -lcblog_utils

.include <bsd.prog.mk>

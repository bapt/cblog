#CC= 	clang
#LD= 	llvm-ld
PROG=	cblog.cgi
SRCS+=	main.c cblog_cgi.c
PREFIX?=	/usr/local
DEFINE=-DETCDIR=\"${PREFIX}/etc\"
INCLUDE=-I${PREFIX}/include -I${PREFIX}/include/ClearSilver -I.
CDB_PATH?=	/usr/local/cblog
CFLAGS+=	-Wall -ggdb -O0 ${INCLUDE} ${DEFINE} -DCDB_PATH=\"${CDB_PATH}\"
LDADD+=	-L${PREFIX}/lib -lz -lneo_cgi -lneo_cs -lneo_utl -lfcgi -lcdb

.include <bsd.prog.mk>

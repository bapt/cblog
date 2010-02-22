PROG=cblogctl
SRCS+= main.c cblogctl.c buffer.c markdown.c renderers.c array.c
PREFIX?= 	/usr/local
DESTDIR= ${PREFIX}/
BINDIR= bin
INCLUDE= -I${PREFIX}/include -I. -I../lib
CFLAGS+= 	-Wall -ggdb -O0 ${INCLUDE} -Werror
CDB_PATH?=	/usr/local/cblog
CFLAGS+= -DCDB_PATH=\"${CDB_PATH}\"
LDADD+= -L${PREFIX}/lib -lcdb -L../lib -lcblog_utils
NO_MAN=yes

.include <bsd.prog.mk>

PREFIX?=	/usr/local
WEBDIR?=	${PREFIX}/www

include config.mk

WEBSRCS=	web/main.c web/cblogweb.c web/cblog_comments.c
LIBSRCS=	lib/db.c lib/utils.c
CLISRCS=	cli/main.c cli/cblogctl.c

WEBOBJS=	${WEBSRCS:.c=.o}
CLIOBJS=	${CLISRCS:.c=.o}
LIBOBJS=	${LIBSRCS:.c=.o}

WEB=	cblogweb
CLI=	cblogctl
LIB=	libcblog_utils.a

WEBLIBS=	-lcblog_utils -lz -lneo_cgi -lneo_cs -lneo_utl -lsqlite3 -levent
CLILIBS=	-lcblog_utils -lsqlite3 -lsoldout -lneo_cs -lneo_utl

all:	${CLI} ${WEB}

.c.o:
	${CC} ${CFLAGS} -Werror -Wall -DETCDIR=\"${SYSCONFDIR}\" -DCDB_PATH=\"${CDB_PATH}\" -Ilib ${INCLUDES} ${CSINCLUDES} -o $@ -c $< ${CFLAGS}

${LIB}: ${LIBOBJS}
	${AR} cr ${LIB} ${LIBOBJS}
	${RANLIB} ${LIB}

${WEB}: ${LIB} ${WEBOBJS}
	${CC} ${LDFLAGS} ${CFLAGS} ${LIBDIR} ${LIBDIR} -L. ${WEBOBJS} -o $@ ${WEBLIBS}

${CLI}: ${LIB} ${CLIOBJS}
	${CC} ${LDFLAGS} ${CFLAGS} ${LIBDIR} -L. ${CLIOBJS} -o $@ ${CLILIBS}

clean:
	rm -f ${WEB} ${CLI} ${LIB} cli/*.o lib/*.o web/*.o

install: all
	install -d ${DESTDIR}${WEBDIR}
	install -m 755 ${WEB} ${DESTDIR}${WEBDIR}
	install -m 755 ${CLI} ${DESTDIR}${BINDIR}
	sed -e 's,_CDB_PATH_,${CDB_PATH},' samples/cblog.conf > cblog.conf
	install -d ${DESTDIR}${DATADIR}
	install -m 644 cblog.conf ${DESTDIR}${DATADIR}/
	cp -r samples ${DESTDIR}${DATADIR}
	install -m 644 web/cblogweb.1 ${DESTDIR}${MANDIR}/man1

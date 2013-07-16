PREFIX?=	/usr/local
WEBDIR?=	${PREFIX}/www

include config.mk

WEBSRCS=	web/main.c web/cblogweb.c web/cblog_comments.c
LIBSRCS=	lib/db.c lib/utils.c
CLISRCS=	cli/main.c cli/cblogctl.c
CONVERTSRCS=	convert/main.c

WEBOBJS=	${WEBSRCS:.c=.o}
CLIOBJS=	${CLISRCS:.c=.o}
CONVERTOBJS=	${CONVERTSRCS:.c=.o}
LIBOBJS=	${LIBSRCS:.c=.o}

WEB=	cblogweb
CLI=	cblogctl
CONVERT=	cblogconvert
LIB=	libcblog_utils.a

WEBLIBS=	-lcblog_utils -lz -lneo_cgi -lneo_cs -lneo_utl -lsqlite3 -levent-2.0
CLILIBS=	-lcblog_utils -lsqlite3 -lsoldout
CONVERTLIBS=	-lcblog_utils -lcdb -lsqlite3 -lneo_cgi -lneo_utl -lneo_cs -lz

all:	${CLI} ${WEB} ${CONVERT}

.c.o:
	${CC} ${CFLAGS} -Werror -Wall -DETCDIR=\"${SYSCONFDIR}\" -DCDB_PATH=\"${CDB_PATH}\" -Ilib ${INCLUDES} ${CSINCLUDES} -o $@ -c $< ${CFLAGS}

${LIB}: ${LIBOBJS}
	${AR} cr ${LIB} ${LIBOBJS}
	${RANLIB} ${LIB}

${WEB}: ${LIB} ${WEBOBJS}
	${CC} ${LDFLAGS} ${CFLAGS} ${LIBDIR} ${LIBDIR}/event2 -L. ${WEBOBJS} -o $@ ${WEBLIBS}

${CLI}: ${LIB} ${CLIOBJS}
	${CC} ${LDFLAGS} ${CFLAGS} ${LIBDIR} -L. ${CLIOBJS} -o $@ ${CLILIBS}

${CONVERT}: ${LIB} ${CONVERTOBJS}
	${CC} ${LDFLAGS} ${CFLAGS} ${LIBDIR} -L. ${CONVERTOBJS} -o $@ ${CONVERTLIBS}

clean:
	rm -f ${WEB} ${CLI} ${LIB} ${CONVERT} cli/*.o lib/*.o web/*.o

install: all
	install -d ${DESTDIR}${WEBDIR}
	install -m 755 ${WEB} ${DESTDIR}${WEBDIR}
	install -m 755 ${CLI} ${DESTDIR}${BINDIR}
	sed -e 's,_CDB_PATH_,${CDB_PATH},' samples/cblog.conf > cblog.conf
	install -d ${DESTDIR}${DATADIR}
	install -m 644 cblog.conf ${DESTDIR}${DATADIR}/
	cp -r samples ${DESTDIR}${DATADIR}
	install -m 644 web/cblogweb.1 ${DESTDIR}${MANDIR}/man1

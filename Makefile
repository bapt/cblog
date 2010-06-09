PREFIX?=	/usr/local

include config.mk

CGISRCS=	cgi/main.c cgi/cblog_cgi.c cgi/cblog_comments.c
LIBSRCS=	lib/db.c lib/utils.c
CLISRCS=	cli/main.c cli/cblogctl.c cli/buffer.c cli/markdown.c cli/renderers.c cli/array.c

CGIOBJS=	${CGISRCS:.c=.o}
CLIOBJS=	${CLISRCS:.c=.o}
LIBOBJS=	${LIBSRCS:.c=.o}

CGI=	cblog.cgi
CLI=	cblogctl
LIB=	libcblog_utils.a

CGILIBS=	-lfcgi -lcblog_utils -lcdb -lz -lneo_cgi -lneo_cs -lneo_utl
CLILIBS=	-lcblog_utils -lcdb

all:	${CLI} ${CGI}

.c.o:
	${CC} ${CFLAGS} -DETCDIR=\"${SYSCONFDIR}\" -DCDB_PATH=\"${CDB_PATH}\" -Ilib ${INCLUDES} ${CSINCLUDES} -o $@ -c $< ${CFLAGS}

${LIB}: ${LIBOBJS}
	${AR} cr ${LIB} ${LIBOBJS}
	${RANLIB} ${LIB}

${CGI}: ${LIB} ${CGIOBJS}
	${CC} ${LDFLAGS} ${CFLAGS} ${LIBDIR} -L. ${CGIOBJS} -o $@ ${CGILIBS}

${CLI}: ${LIB} ${CLIOBJS}
	${CC} ${LDFLAGS} ${CFLAGS} ${LIBDIR} -L. ${CLIOBJS} -o $@ ${CLILIBS}

clean:
	rm -f ${CGI} ${CLI} ${LIB} cli/*.o lib/*.o cgi/*.o

install: all
	install -d ${DESTDIR}${CGIDIR}
	install -m 755 ${CGI} ${DESTDIR}${CGIDIR}
	install -m 755 ${CLI} ${DESTDIR}${BINDIR}
	sed -e 's,_CDB_PATH_,${CDB_PATH},' samples/cblog.conf > cblog.conf
	install -d ${DESTDIR}${DATADIR}
	install -m 644 cblog.conf ${DESTDIR}${DATADIR}/
	cp -r samples ${DESTDIR}${DATADIR}
	install -m 644 cgi/cblog.cgi.1 ${DESTDIR}${MANDIR}/man1

PREFIX?=	/usr/local

include config.mk

SRCS=	main.c cblog.c
OBJS=	${SRCS:.c=.o}

PROG=	cblog

LIBS=	-lsoldout -lneo_cs -lneo_utl

all:	${PROG}

.c.o:	cblog.h
	${CC} ${CFLAGS} -Werror -Wall -DETCDIR=\"${SYSCONFDIR}\" -Iexternal ${INCLUDES} ${CSINCLUDES} -o $@ -c $< ${CFLAGS}

${PROG}: ${OBJS}
	${CC} ${LDFLAGS} ${CFLAGS} ${LIBDIR} -L. ${OBJS} -o $@ ${LIBS}

clean:
	rm -f ${PROG} ${OBJS}

install: all
	install -d ${DESTDIR}${WEBDIR}
	install -m 755 ${PROG} ${DESTDIR}${BINDIR}
	sed -e 's,_CDB_PATH_,${CDB_PATH},' samples/cblog.conf > cblog.conf
	install -d ${DESTDIR}${DATADIR}
	install -m 644 cblog.conf ${DESTDIR}${DATADIR}/
	cp -r samples ${DESTDIR}${DATADIR}

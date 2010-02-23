#CC= 	clang
#LD= 	llvm-ld
PROG=	cblog.cgi
SRCS+=	main.c cblog_cgi.c cblog_comments.c
PREFIX?=	/usr/local
DESTDIR= ${PREFIX}
BINDIR= /libexec/cgi-bin/
MANDIR= /man/man
DEFINE=-DETCDIR=\"${PREFIX}/etc\"
INCLUDE=-I${PREFIX}/include -I${PREFIX}/include/ClearSilver -I. -I../lib
CDB_PATH?=	/usr/local/cblog
CFLAGS+=	-Wall -ggdb -O0 ${INCLUDE} ${DEFINE} -DCDB_PATH=\"${CDB_PATH}\" -Werror
LDADD+=	-L${PREFIX}/lib -lz -lneo_cgi -lneo_cs -lneo_utl -lfcgi -lcdb -L../lib -lcblog_utils

#if exists(cblogctl)
CDB_PATH!= cblogctl p
#endif

beforeinstall:
	@mkdir -p ${PREFIX}/etc ${PREFIX}/etc/rc.d
	@sed -e 's,_CDB_PATH_,${CDB_PATH},' ../samples/cblog.conf > ${PREFIX}/etc/
	@mkdir -p ${DESTDIR}/share/cblog
	@cp -r ../samples/templates ${DESTDIR}/share/cblog
	@sed -e 's,_APP_,${DESTDIR}${BINDIR}${PROG},' ../samples/spawn_cblog > ${PREFIX}/etc/rc.d/spawn_cblog
	@mkdir -p ${DESTDIR}${BINDIR}

.include <bsd.prog.mk>

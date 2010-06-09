#Default includes
INCLUDES=	-I${PREFIX}/include

#Default libdir
LIBDIR=		-L${PREFIX}/lib

#ClearSilver includes
CSINCLUDES=	-I${PREFIX}/include/ClearSilver

#Default cblog cdb path (database for cblog)
CDB_PATH=	${PREFIX}/cblog

# CGI install dir
CGIDIR=		${PREFIX}/libexec/cgi-bin

# Bindir
BINDIR=		${PREFIX}/bin

# Mandir
MANDIR=		${PREFIX}/man

# data dir
DATADIR=	${PREFIX}/share/cblog

# Config dir
SYSCONFDIR=		${PREFIX}/etc

CC?=	gcc
AR?=	ar
RANLIB?=	ranlib

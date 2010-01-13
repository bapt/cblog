PROG=cblogctl
SRCS+= main.c cblogctl.c buffer.c markdown.c renderers.c array.c
PREFIX?= 	/usr/local
INCLUDE= -I${PREFIX}/include -I.
CFLAGS+= 	-Wall -ggdb -O0 ${INCLUDE}
LDADD+= -L${PREFIX}/lib -lcdb
NO_MAN=yes

.include <bsd.prog.mk>

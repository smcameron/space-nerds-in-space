COMMONOBJS=mathutils.o snis_alloc.o snis_socket_io.o snis_marshal.o
SERVEROBJS=${COMMONOBJS} snis_server.o
CLIENTOBJS=${COMMONOBJS} snis_client.o snis_font.o
SSGL=ssgl/libssglclient.a
LIBS=-Lssgl -lssglclient -lrt -lm

PROGS=snis_server snis_client

MYCFLAGS=-g --pedantic -Wall -pthread -std=gnu99
GTKCFLAGS=`pkg-config --cflags gtk+-2.0`
GTKLDFLAGS=`pkg-config --libs gtk+-2.0` \
        `pkg-config --libs gthread-2.0` \

all:	${COMMONOBJS} ${SERVEROBJS} ${CLIENTOBJS} ${PROGS}

snis_server.o:	snis.h snis_server.c snis_packet.h snis_marshal.h
	gcc ${MYCFLAGS} ${GTKCFLAGS} -c snis_server.c

snis_client.o:	snis.h snis_client.c snis_font.h my_point.h snis_packet.h snis_marshal.h
	gcc ${MYCFLAGS} ${GTKCFLAGS} -c snis_client.c

snis_socket_io.o:	snis_socket_io.h
	gcc ${MYCFLAGS} ${GTKCFLAGS} -c snis_socket_io.c

snis_marshal.o:	snis_marshal.h snis_marshal.c
	gcc ${MYCFLAGS} ${GTKCFLAGS} -c snis_marshal.c

snis_font.o:	snis_font.h snis_font.c my_point.h
	gcc ${MYCFLAGS} ${GTKCFLAGS} -c snis_font.c

mathutils.o:	mathutils.h mathutils.c
	gcc ${MYCFLAGS} ${GTKCFLAGS} -c mathutils.c

snis_alloc.o:	snis_alloc.h snis_alloc.c
	gcc ${MYCFLAGS} ${GTKCFLAGS} -c snis_alloc.c

snis_server:	${SERVEROBJS} ${SSGL}
	gcc ${MYCFLAGS} -o snis_server ${GTKCFLAGS} ${SERVEROBJS} ${GTKLDFLAGS} ${LIBS}

snis_client:	${CLIENTOBJS} ${SSGL}
	gcc ${MYCFLAGS} -o snis_client ${GTKCFLAGS}  ${CLIENTOBJS} ${GTKLDFLAGS} ${LIBS}

${SSGL}:
	(cd ssgl ; make )

clean:
	rm -f ${SERVEROBJS} ${CLIENTOBJS} ${PROGS} ${SSGL}
	( cd ssgl; make clean )


# To compile withaudio, WITHAUDIO=yes, 
# for no audio support, change to WITHAUDIO=no, 
WITHAUDIO=yes
# WITHAUDIO=no

PREFIX=/usr
DATADIR=${PREFIX}/share/space-nerds-in-space

ifeq (${WITHAUDIO},yes)
SNDLIBS=`pkg-config --libs portaudio-2.0 vorbisfile`
SNDFLAGS=-DWITHAUDIOSUPPORT `pkg-config --cflags portaudio-2.0` -DDATADIR=\"${DATADIR}\"
OGGOBJ=ogg_to_pcm.o
SNDOBJS=wwviaudio.o
else
SNDLIBS=
SNDFLAGS=-DWWVIAUDIO_STUBS_ONLY
OGGOBJ=
SNDOBJS=wwviaudio.o
endif


COMMONOBJS=mathutils.o snis_alloc.o snis_socket_io.o snis_marshal.o bline.o
SERVEROBJS=${COMMONOBJS} snis_server.o names.o
CLIENTOBJS=${COMMONOBJS} ${OGGOBJ} ${SNDOBJS} snis_client.o snis_font.o
SSGL=ssgl/libssglclient.a
LIBS=-Lssgl -lssglclient -lrt -lm

PROGS=snis_server snis_client

MYCFLAGS=-g --pedantic -Wall -Werror -pthread -std=gnu99
GTKCFLAGS=`pkg-config --cflags gtk+-2.0`
GTKLDFLAGS=`pkg-config --libs gtk+-2.0` \
        `pkg-config --libs gthread-2.0` \

all:	${COMMONOBJS} ${SERVEROBJS} ${CLIENTOBJS} ${PROGS}

ogg_to_pcm.o:   ogg_to_pcm.c ogg_to_pcm.h Makefile
	$(CC) ${DEBUG} ${PROFILE_FLAG} ${OPTIMIZE_FLAG} `pkg-config --cflags vorbisfile` \
		-pthread ${WARNFLAG} -c ogg_to_pcm.c

bline.o:	bline.c bline.h
	$(CC) ${DEBUG} ${PROFILE_FLAG} ${OPTIMIZE_FLAG} -pthread -c bline.c

wwviaudio.o:    wwviaudio.c wwviaudio.h ogg_to_pcm.h my_point.h Makefile
	$(CC) ${WARNFLAG} ${DEBUG} ${PROFILE_FLAG} ${OPTIMIZE_FLAG} \
		${SNDFLAGS} \
		-pthread `pkg-config --cflags vorbisfile` \
		-c wwviaudio.c

snis_server.o:	snis.h snis_server.c snis_packet.h snis_marshal.h sounds.h
	gcc ${MYCFLAGS} ${GTKCFLAGS} -c snis_server.c

snis_client.o:	snis.h snis_client.c snis_font.h my_point.h snis_packet.h snis_marshal.h sounds.h wwviaudio.h
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
	gcc ${MYCFLAGS} ${SNDFLAGS} -o snis_client ${GTKCFLAGS}  ${CLIENTOBJS} ${GTKLDFLAGS} ${LIBS} ${SNDLIBS}

${SSGL}:
	(cd ssgl ; make )

clean:
	rm -f ${SERVEROBJS} ${CLIENTOBJS} ${PROGS} ${SSGL}
	( cd ssgl; make clean )


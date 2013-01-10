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


COMMONOBJS=mathutils.o snis_alloc.o snis_socket_io.o snis_marshal.o \
		bline.o shield_strength.o
SERVEROBJS=${COMMONOBJS} snis_server.o names.o starbase-comms.o infinite-taunt.o
CLIENTOBJS=${COMMONOBJS} ${OGGOBJ} ${SNDOBJS} snis_ui_element.o snis_graph.o \
	snis_client.o snis_font.o snis_text_input.o \
	snis_typeface.o snis_gauge.o snis_button.o snis_sliders.o snis_text_window.o \
	stl_parser.o entity.o matrix.o
SSGL=ssgl/libssglclient.a
LIBS=-Lssgl -lssglclient -lrt -lm

PROGS=snis_server snis_client

#MYCFLAGS=-g --pedantic -Wall -Werror -pthread -std=gnu99
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

shield_strength.o:	shield_strength.c shield_strength.h
	gcc ${MYCFLAGS} -c shield_strength.c

snis_server.o:	snis.h snis_server.c snis_packet.h snis_marshal.h sounds.h starbase-comms.h
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

starbase-comms.o:	starbase-comms.h starbase-comms.c
	gcc ${MYCFLAGS} ${GTKCFLAGS} -c starbase-comms.c

infinite-taunt.o:	infinite-taunt.h infinite-taunt.c
	gcc ${MYCFLAGS} ${GTKCFLAGS} -c infinite-taunt.c

infinite-taunt:	infinite-taunt.c infinite-taunt.h
	gcc -DTEST_TAUNT -o infinite-taunt ${MYCFLAGS} ${GTKCFLAGS} infinite-taunt.c

names:	names.h names.c
	gcc -DTEST_NAMES -o names ${MYCFLAGS} ${GTKCFLAGS} names.c

snis_graph.o:	snis_graph.h snis_graph.c
	gcc ${MYCFLAGS} ${GTKCFLAGS} -c snis_graph.c

snis_typeface.o:	snis_typeface.h snis_typeface.c
	gcc ${MYCFLAGS} ${GTKCFLAGS} -c snis_typeface.c

snis_gauge.o:	snis_gauge.h snis_gauge.c snis_graph.h
	gcc ${MYCFLAGS} ${GTKCFLAGS} -c snis_gauge.c

snis_button.o:	snis_button.h snis_button.c snis_graph.h
	gcc ${MYCFLAGS} ${GTKCFLAGS} -c snis_button.c

snis_sliders.o:	snis_sliders.h snis_sliders.c snis_graph.h
	gcc ${MYCFLAGS} ${GTKCFLAGS} -c snis_sliders.c

snis_text_window.o:	snis_text_window.h snis_text_window.c snis_graph.h \
			snis_font.h snis_typeface.h
	gcc ${MYCFLAGS} ${GTKCFLAGS} -c snis_text_window.c

snis_ui_element.o:	snis_ui_element.h snis_ui_element.c
	gcc ${MYCFLAGS} ${GTKCFLAGS} -c snis_ui_element.c

snis_text_input.o:	snis_text_input.h snis_text_input.c
	gcc ${MYCFLAGS} ${GTKCFLAGS} -c snis_text_input.c

matrix.o:	matrix.h matrix.c
	gcc ${MYCFLAGS} ${GTKCFLAGS} -c matrix.c

stl_parser.o:	stl_parser.c stl_parser.h vertex.h triangle.h mesh.h
	gcc ${MYCFLAGS} ${GTKCFLAGS} -c stl_parser.c

stl_parser:	stl_parser.o stl_parser.h vertex.h triangle.h mesh.h
	gcc -DTEST_STL_PARSER ${MYCFLAGS} ${GTKCFLAGS} -o stl_parser stl_parser.c -lm

entity.o:	entity.c entity.h mathutils.h vertex.h triangle.h mesh.h stl_parser.h
	gcc ${MYCFLAGS} ${GTKCFLAGS} -c entity.c

test_matrix:	matrix.c matrix.h
	gcc ${MYCFLAGS} ${GTKCFLAGS} -DTEST_MATRIX -o test_matrix matrix.c -lm

${SSGL}:
	(cd ssgl ; make )

clean:
	rm -f ${SERVEROBJS} ${CLIENTOBJS} ${PROGS} ${SSGL} stl_parser
	( cd ssgl; make clean )


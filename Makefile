# To compile withaudio, WITHAUDIO=yes, 
# for no audio support, change to WITHAUDIO=no, 
WITHAUDIO=yes
# WITHAUDIO=no

PREFIX=/usr
DATADIR=${PREFIX}/share/space-nerds-in-space

CC=gcc

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

ifeq (${E},1)
STOP_ON_WARN=-Werror
else
STOP_ON_WARN=
endif


COMMONOBJS=mathutils.o snis_alloc.o snis_socket_io.o snis_marshal.o \
		bline.o shield_strength.o stacktrace.o
SERVEROBJS=${COMMONOBJS} snis_server.o names.o starbase-comms.o infinite-taunt.o \
		power-model.o

CLIENTOBJS=${COMMONOBJS} ${OGGOBJ} ${SNDOBJS} snis_ui_element.o snis_graph.o \
	snis_client.o snis_font.o snis_text_input.o \
	snis_typeface.o snis_gauge.o snis_button.o snis_label.o snis_sliders.o snis_text_window.o \
	snis_damcon_systems.o mesh.o \
	stl_parser.o entity.o matrix.o my_point.o liang-barsky.o joystick.o
SSGL=ssgl/libssglclient.a
LIBS=-Lssgl -lssglclient -lrt -lm

PROGS=snis_server snis_client

MODELS=freighter.stl laser.stl planet.stl spaceship.stl starbase.stl torpedo.stl \
	tanker.stl destroyer.stl transport.stl battlestar.stl cruiser.stl tetrahedron.stl \
	flat-tetrahedron.stl big-flat-tetrahedron.stl asteroid.stl asteroid2.stl asteroid3.stl \
	asteroid4.stl wormhole.stl starbase2.stl starbase3.stl starbase4.stl spacemonster.stl \
	asteroid-miner.stl spaceship2.stl spaceship3.stl planet1.stl planet2.stl planet3.stl

#MYCFLAGS=-g --pedantic -Wall -Werror -pthread -std=gnu99
MYCFLAGS=-g --pedantic -Wall ${STOP_ON_WARN} -pthread -std=gnu99 -rdynamic
GTKCFLAGS=`pkg-config --cflags gtk+-2.0`
GTKLDFLAGS=`pkg-config --libs gtk+-2.0` \
        `pkg-config --libs gthread-2.0` \

all:	${COMMONOBJS} ${SERVEROBJS} ${CLIENTOBJS} ${PROGS} ${MODELS}

starbase.stl:	starbase.scad wedge.scad
	openscad -o starbase.stl starbase.scad

%.stl:	%.scad
	openscad -o $@ $<

my_point.o:   my_point.c my_point.h Makefile
	$(CC) ${MYCFLAGS} -c my_point.c

mesh.o:   mesh.c mesh.h Makefile
	$(CC) ${MYCFLAGS} -c mesh.c

power-model.o:   power-model.c power-model.h Makefile
	$(CC) ${MYCFLAGS} -c power-model.c

stacktrace.o:   stacktrace.c stacktrace.h Makefile
	$(CC) ${MYCFLAGS} -c stacktrace.c

liang-barsky.o:   liang-barsky.c liang-barsky.h Makefile
	$(CC) ${MYCFLAGS} -c liang-barsky.c

joystick.o:   joystick.c joystick.h compat.h Makefile 
	$(CC) ${MYCFLAGS} -c joystick.c

ogg_to_pcm.o:   ogg_to_pcm.c ogg_to_pcm.h Makefile
	$(CC) ${MYCFLAGS} `pkg-config --cflags vorbisfile` -c ogg_to_pcm.c

bline.o:	bline.c bline.h
	$(CC) ${MYCFLAGS} -c bline.c

wwviaudio.o:    wwviaudio.c wwviaudio.h ogg_to_pcm.h my_point.h Makefile
	$(CC) ${MYCFLAGS} ${SNDFLAGS} `pkg-config --cflags vorbisfile` \
		-c wwviaudio.c

shield_strength.o:	shield_strength.c shield_strength.h
	$(CC) ${MYCFLAGS} -c shield_strength.c

snis_server.o:	snis.h snis_server.c snis_packet.h snis_marshal.h sounds.h starbase-comms.h
	$(CC) ${MYCFLAGS} ${GTKCFLAGS} -c snis_server.c

snis_client.o:	snis.h snis_client.c snis_font.h my_point.h snis_packet.h snis_marshal.h sounds.h wwviaudio.h snis-logo.h placeholder-system-points.h
	$(CC) ${MYCFLAGS} ${GTKCFLAGS} -c snis_client.c

snis_socket_io.o:	snis_socket_io.h
	$(CC) ${MYCFLAGS} ${GTKCFLAGS} -c snis_socket_io.c

snis_marshal.o:	snis_marshal.h snis_marshal.c
	$(CC) ${MYCFLAGS} ${GTKCFLAGS} -c snis_marshal.c

snis_font.o:	snis_font.h snis_font.c my_point.h
	$(CC) ${MYCFLAGS} ${GTKCFLAGS} -c snis_font.c

mathutils.o:	mathutils.h mathutils.c
	$(CC) ${MYCFLAGS} ${GTKCFLAGS} -c mathutils.c

snis_alloc.o:	snis_alloc.h snis_alloc.c
	$(CC) ${MYCFLAGS} ${GTKCFLAGS} -c snis_alloc.c

snis_damcon_systems.o:	snis_damcon_systems.c snis_damcon_systems.h
	$(CC) ${MYCFLAGS} ${GTKCFLAGS} -c snis_damcon_systems.c

snis_server:	${SERVEROBJS} ${SSGL}
	$(CC) ${MYCFLAGS} -o snis_server ${GTKCFLAGS} ${SERVEROBJS} ${GTKLDFLAGS} ${LIBS}

snis_client:	${CLIENTOBJS} ${SSGL}
	$(CC) ${MYCFLAGS} ${SNDFLAGS} -o snis_client ${GTKCFLAGS}  ${CLIENTOBJS} ${GTKLDFLAGS} ${LIBS} ${SNDLIBS}

starbase-comms.o:	starbase-comms.h starbase-comms.c
	$(CC) ${MYCFLAGS} ${GTKCFLAGS} -c starbase-comms.c

infinite-taunt.o:	infinite-taunt.h infinite-taunt.c
	$(CC) ${MYCFLAGS} -c infinite-taunt.c

infinite-taunt:	infinite-taunt.c infinite-taunt.h
	$(CC) -DTEST_TAUNT -o infinite-taunt ${MYCFLAGS} infinite-taunt.c

names:	names.h names.c
	$(CC) -DTEST_NAMES -o names ${MYCFLAGS} ${GTKCFLAGS} names.c

snis_graph.o:	snis_graph.h snis_graph.c
	$(CC) ${MYCFLAGS} ${GTKCFLAGS} -c snis_graph.c

snis_typeface.o:	snis_typeface.h snis_typeface.c
	$(CC) ${MYCFLAGS} ${GTKCFLAGS} -c snis_typeface.c

snis_gauge.o:	snis_gauge.h snis_gauge.c snis_graph.h
	$(CC) ${MYCFLAGS} ${GTKCFLAGS} -c snis_gauge.c

snis_button.o:	snis_button.h snis_button.c snis_graph.h
	$(CC) ${MYCFLAGS} ${GTKCFLAGS} -c snis_button.c

snis_label.o:	snis_label.h snis_label.c snis_graph.h
	$(CC) ${MYCFLAGS} ${GTKCFLAGS} -c snis_label.c

snis_sliders.o:	snis_sliders.h snis_sliders.c snis_graph.h
	$(CC) ${MYCFLAGS} ${GTKCFLAGS} -c snis_sliders.c

snis_text_window.o:	snis_text_window.h snis_text_window.c snis_graph.h \
			snis_font.h snis_typeface.h
	$(CC) ${MYCFLAGS} ${GTKCFLAGS} -c snis_text_window.c

snis_ui_element.o:	snis_ui_element.h snis_ui_element.c
	$(CC) ${MYCFLAGS} ${GTKCFLAGS} -c snis_ui_element.c

snis_text_input.o:	snis_text_input.h snis_text_input.c
	$(CC) ${MYCFLAGS} ${GTKCFLAGS} -c snis_text_input.c

matrix.o:	matrix.h matrix.c
	$(CC) ${MYCFLAGS} ${GTKCFLAGS} -c matrix.c

stl_parser.o:	stl_parser.c stl_parser.h vertex.h triangle.h mesh.h
	$(CC) ${MYCFLAGS} ${GTKCFLAGS} -c stl_parser.c

stl_parser:	stl_parser.o stl_parser.h vertex.h triangle.h mesh.h
	$(CC) -DTEST_STL_PARSER ${MYCFLAGS} ${GTKCFLAGS} -o stl_parser stl_parser.c -lm

entity.o:	entity.c entity.h mathutils.h vertex.h triangle.h mesh.h stl_parser.h snis_alloc.h
	$(CC) ${MYCFLAGS} ${GTKCFLAGS} -c entity.c

test_matrix:	matrix.c matrix.h
	$(CC) ${MYCFLAGS} ${GTKCFLAGS} -DTEST_MATRIX -o test_matrix matrix.c -lm

${SSGL}:
	(cd ssgl ; make )

clean:
	rm -f ${SERVEROBJS} ${CLIENTOBJS} ${PROGS} ${SSGL} stl_parser ${MODELS} 
	( cd ssgl; make clean )


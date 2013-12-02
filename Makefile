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

ifeq (${O},1)
DEBUGFLAG=
OPTIMIZEFLAG=-O3
else
DEBUGFLAG=-g
OPTIMIZEFLAG=
endif

ifeq (${P},1)
PROFILEFLAG=-pg
OPTIMIZEFLAG=-O3
DEBUGFLAG=
else
PROFILEFLAG=
endif

ifeq (${ILDA},1)
ILDAFLAG=-DWITH_ILDA_SUPPORT
else
ILDAFLAG=
endif

# Arch pkg-config seems to be broken for lua5.2, so we have
# this "... || echo" hack thing.
#
LUALIBS=`pkg-config --libs lua5.2 || echo '-llua'`
LUACFLAGS=`pkg-config --cflags lua5.2 || echo ''`

PNGLIBS=`pkg-config --libs libpng`
PNGCFLAGS=`pkg-config --cflags libpng`

COMMONOBJS=mathutils.o snis_alloc.o snis_socket_io.o snis_marshal.o \
		bline.o shield_strength.o stacktrace.o snis_ship_type.o \
		snis_faction.o
SERVEROBJS=${COMMONOBJS} snis_server.o names.o starbase-comms.o infinite-taunt.o \
		power-model.o quat.o matrix.o snis_event_callback.o space-part.o

CLIENTOBJS=${COMMONOBJS} ${OGGOBJ} ${SNDOBJS} snis_ui_element.o snis_graph.o \
	snis_client.o snis_font.o snis_text_input.o \
	snis_typeface.o snis_gauge.o snis_button.o snis_label.o snis_sliders.o snis_text_window.o \
	snis_damcon_systems.o mesh.o \
	stl_parser.o entity.o matrix.o my_point.o liang-barsky.o joystick.o quat.o

LIMCLIENTOBJS=${COMMONOBJS} ${OGGOBJ} ${SNDOBJS} snis_ui_element.o snis_limited_graph.o \
	snis_limited_client.o snis_font.o snis_text_input.o \
	snis_typeface.o snis_gauge.o snis_button.o snis_label.o snis_sliders.o snis_text_window.o \
	snis_damcon_systems.o mesh.o \
	stl_parser.o entity.o matrix.o my_point.o liang-barsky.o joystick.o quat.o fleet.o

SSGL=ssgl/libssglclient.a
LIBS=-Lssgl -lssglclient -lrt -lm ${LUALIBS} ${PNGLIBS}
#
# NOTE: if you get
#
# /usr/bin/ld: cannot find -llua5.2
# collect2: ld returned 1 exit status 
#
# try:
#
# sudo ln -s /usr/lib/i386-linux-gnu/liblua5.1.so.0 /usr/local/lib/liblua5.2.so
#
# or, if on x86_64:
#
# sudo ln -s /usr/lib/x86_64-linux-gnu/liblua5.1.so.0 /usr/local/lib/liblua5.2.so
#
# Also, you will need to install lua5.2 and liblua5.2-dev
# "sudo apt-get install liblua5.2-dev lua5.2"
#


PROGS=snis_server snis_client snis_limited_client

# model directory
MD=share/snis/models

MODELS=${MD}/freighter.stl \
	${MD}/laser.stl \
	${MD}/planet.stl \
	${MD}/spaceship.stl \
	${MD}/starbase.stl \
	${MD}/torpedo.stl \
	${MD}/tanker.stl \
	${MD}/destroyer.stl \
	${MD}/transport.stl \
	${MD}/battlestar.stl \
	${MD}/cruiser.stl \
	${MD}/tetrahedron.stl \
	${MD}/flat-tetrahedron.stl \
	${MD}/big-flat-tetrahedron.stl \
	${MD}/asteroid.stl \
	${MD}/asteroid2.stl \
	${MD}/asteroid3.stl \
	${MD}/asteroid4.stl \
	${MD}/wormhole.stl \
	${MD}/starbase2.stl \
	${MD}/starbase3.stl \
	${MD}/starbase4.stl \
	${MD}/starbase5.stl \
	${MD}/spacemonster.stl \
	${MD}/asteroid-miner.stl \
	${MD}/spaceship2.stl \
	${MD}/spaceship3.stl \
	${MD}/planet1.stl \
	${MD}/planet2.stl \
	${MD}/planet3.stl \
	${MD}/dragonhawk.stl \
	${MD}/skorpio.stl \
	${MD}/disruptor.stl \
	${MD}/research-vessel.stl \
	${MD}/long-triangular-prism.stl \
	${MD}/ship-icon.stl \
	${MD}/heading_indicator.stl \
	${MD}/axis.stl \
	${MD}/conqueror.stl \
	${MD}/scrambler.stl \
	${MD}/swordfish.stl \
	${MD}/wombat.stl \
	${MD}/spaceship_turret.stl
	

MYCFLAGS=${DEBUGFLAG} ${PROFILEFLAG} ${OPTIMIZEFLAG} ${ILDAFLAG}\
	--pedantic -Wall ${STOP_ON_WARN} -pthread -std=gnu99 -rdynamic
GTKCFLAGS=$(subst -I,-isystem ,$(shell pkg-config --cflags gtk+-2.0))
GLEXTCFLAGS=$(subst -I,-isystem ,$(shell pkg-config --cflags gtkglext-1.0)) ${PNGCFLAGS}
GTKLDFLAGS=`pkg-config --libs gtk+-2.0` \
        `pkg-config --libs gthread-2.0`
GLEXTLDFLAGS=`pkg-config --libs gtkglext-1.0` 
VORBISFLAGS=$(subst -I,-isystem ,$(shell pkg-config --cflags vorbisfile))

ifeq (${V},1)
Q=
ECHO=true
else
Q=@
ECHO=echo
endif

COMPILE=$(CC) ${MYCFLAGS} ${LUACFLAGS} -c -o $@ $< && $(ECHO) '  COMPILE' $<
GTKCOMPILE=$(CC) ${MYCFLAGS} ${GTKCFLAGS} -c -o $@ $< && $(ECHO) '  COMPILE' $<
LIMCOMPILE=$(CC) -DWITHOUTOPENGL=1 ${MYCFLAGS} ${GTKCFLAGS} -c -o $@ $< && $(ECHO) '  COMPILE' $<
GLEXTCOMPILE=$(CC) ${MYCFLAGS} ${GTKCFLAGS} ${GLEXTCFLAGS} -c -o $@ $< && $(ECHO) '  COMPILE' $<
VORBISCOMPILE=$(CC) ${MYCFLAGS} ${VORBISFLAGS} ${SNDFLAGS} -c -o $@ $< && $(ECHO) '  COMPILE' $<

CLIENTLINK=$(CC) ${MYCFLAGS} ${SNDFLAGS} -o $@ ${GTKCFLAGS} ${GLEXTCFLAGS} ${CLIENTOBJS} ${GTKLDFLAGS} ${GLEXTLDFLAGS} ${LIBS} ${SNDLIBS} && $(ECHO) '  LINK' $@
LIMCLIENTLINK=$(CC) ${MYCFLAGS} ${SNDFLAGS} -o $@ ${GTKCFLAGS} ${LIMCLIENTOBJS} ${GLEXTLDFLAGS} ${LIBS} ${SNDLIBS} && $(ECHO) '  LINK' $@
SERVERLINK=$(CC) ${MYCFLAGS} -o $@ ${GTKCFLAGS} ${SERVEROBJS} ${GTKLDFLAGS} ${LIBS} && $(ECHO) '  LINK' $@
OPENSCAD=openscad -o $@ $< && $(ECHO) '  OPENSCAD' $<

all:	${COMMONOBJS} ${SERVEROBJS} ${CLIENTOBJS} ${LIMCLIENTOBJS} ${PROGS} ${MODELS}

starbase.stl:	starbase.scad wedge.scad
	$(Q)$(OPENSCAD)

%.stl:	%.scad
	$(Q)$(OPENSCAD)

my_point.o:   my_point.c my_point.h Makefile
	$(Q)$(COMPILE)

mesh.o:   mesh.c mesh.h vertex.h Makefile
	$(Q)$(COMPILE)

power-model.o:   power-model.c power-model.h Makefile
	$(Q)$(COMPILE)

stacktrace.o:   stacktrace.c stacktrace.h Makefile
	$(Q)$(COMPILE)

snis_ship_type.o:   snis_ship_type.c snis_ship_type.h Makefile
	$(Q)$(COMPILE)

snis_faction.o:   snis_faction.c snis_faction.h Makefile
	$(Q)$(COMPILE)

liang-barsky.o:   liang-barsky.c liang-barsky.h Makefile
	$(Q)$(COMPILE)

joystick.o:   joystick.c joystick.h compat.h Makefile 
	$(Q)$(COMPILE)

ogg_to_pcm.o:   ogg_to_pcm.c ogg_to_pcm.h Makefile
	$(Q)$(VORBISCOMPILE)

bline.o:	bline.c bline.h
	$(Q)$(COMPILE)

wwviaudio.o:    wwviaudio.c wwviaudio.h ogg_to_pcm.h my_point.h Makefile
	$(Q)$(VORBISCOMPILE)

shield_strength.o:	shield_strength.c shield_strength.h
	$(Q)$(COMPILE)

snis_server.o:	snis_server.c snis.h snis_packet.h snis_marshal.h sounds.h starbase-comms.h
	$(Q)$(COMPILE)

snis_client.o:	snis_client.c snis.h snis_font.h my_point.h snis_packet.h snis_marshal.h sounds.h wwviaudio.h snis-logo.h placeholder-system-points.h vertex.h quat.o
	$(Q)$(GLEXTCOMPILE)

snis_limited_client.c:	snis_client.c
	cp snis_client.c snis_limited_client.c

snis_limited_client.o:	snis_limited_client.c snis.h snis_font.h my_point.h snis_packet.h snis_marshal.h sounds.h wwviaudio.h snis-logo.h placeholder-system-points.h vertex.h quat.o
	$(Q)$(LIMCOMPILE)

snis_socket_io.o:	snis_socket_io.c snis_socket_io.h
	$(Q)$(COMPILE)

snis_marshal.o:	snis_marshal.c snis_marshal.h
	$(Q)$(COMPILE)

snis_font.o:	snis_font.c snis_font.h my_point.h
	$(Q)$(COMPILE)

mathutils.o:	mathutils.c mathutils.h
	$(Q)$(COMPILE)

snis_alloc.o:	snis_alloc.c snis_alloc.h
	$(Q)$(COMPILE)

snis_damcon_systems.o:	snis_damcon_systems.c snis_damcon_systems.h
	$(Q)$(COMPILE)

# snis_server:	${SERVEROBJS} ${SSGL}
#	$(CC) ${MYCFLAGS} -o snis_server ${GTKCFLAGS} ${SERVEROBJS} ${GTKLDFLAGS} ${LIBS}

snis_server:	${SERVEROBJS} ${SSGL}
	$(Q)$(SERVERLINK)

snis_client:	${CLIENTOBJS} ${SSGL}
	$(Q)$(CLIENTLINK)

snis_limited_client:	${LIMCLIENTOBJS} ${SSGL}
	$(Q)$(LIMCLIENTLINK)

#	$(CC) ${MYCFLAGS} ${SNDFLAGS} -o snis_client ${GTKCFLAGS}  ${CLIENTOBJS} ${GTKLDFLAGS} ${LIBS} ${SNDLIBS}

starbase-comms.o:	starbase-comms.c starbase-comms.h
	$(Q)$(COMPILE)	

infinite-taunt.o:	infinite-taunt.c infinite-taunt.h
	$(Q)$(COMPILE)

infinite-taunt:	infinite-taunt.c infinite-taunt.h
	$(CC) -DTEST_TAUNT -o infinite-taunt ${MYCFLAGS} infinite-taunt.c

names:	names.c names.h
	$(CC) -DTEST_NAMES -o names ${MYCFLAGS} ${GTKCFLAGS} names.c

snis_limited_graph.c:	snis_graph.c
	cp snis_graph.c snis_limited_graph.c

snis_limited_graph.o:	snis_limited_graph.c snis_graph.h
	$(Q)$(LIMCOMPILE)

snis_graph.o:	snis_graph.c snis_graph.h
	$(Q)$(GLEXTCOMPILE)

snis_typeface.o:	snis_typeface.c snis_typeface.h
	$(Q)$(GTKCOMPILE)

snis_gauge.o:	snis_gauge.c snis_gauge.h snis_graph.h
	$(Q)$(GTKCOMPILE)

snis_button.o:	snis_button.c snis_button.h snis_graph.h
	$(Q)$(GTKCOMPILE)

snis_label.o:	snis_label.c snis_label.h snis_graph.h
	$(Q)$(GTKCOMPILE)

snis_sliders.o:	snis_sliders.c snis_sliders.h snis_graph.h
	$(Q)$(GTKCOMPILE)

snis_text_window.o:	snis_text_window.c snis_text_window.h snis_graph.h \
			snis_font.h snis_typeface.h
	$(Q)$(GTKCOMPILE)

snis_ui_element.o:	snis_ui_element.c snis_ui_element.h
	$(Q)$(GTKCOMPILE)

snis_text_input.o:	snis_text_input.c snis_text_input.h
	$(Q)$(GTKCOMPILE)

matrix.o:	matrix.c matrix.h
	$(Q)$(COMPILE)

stl_parser.o:	stl_parser.c stl_parser.h vertex.h triangle.h mesh.h
	$(Q)$(COMPILE)

stl_parser:	stl_parser.o stl_parser.h vertex.h triangle.h mesh.h
	$(CC) -DTEST_STL_PARSER ${MYCFLAGS} ${GTKCFLAGS} -o stl_parser stl_parser.c matrix.c mesh.c mathutils.c -lm

entity.o:	entity.c entity.h mathutils.h vertex.h triangle.h mesh.h stl_parser.h snis_alloc.h
	$(Q)$(GTKCOMPILE)

names.o:	names.c names.h
	$(Q)$(COMPILE)

quat.o:	quat.c quat.h
	$(Q)$(COMPILE)

fleet.o:	fleet.c fleet.h
	$(Q)$(COMPILE)

test_matrix:	matrix.c matrix.h
	$(CC) ${MYCFLAGS} ${GTKCFLAGS} -DTEST_MATRIX -o test_matrix matrix.c -lm

test-space-partition:	space-part.c space-part.h
	$(CC) ${MYCFLAGS} -g -DTEST_SPACE_PARTITION -o test-space-partition space-part.c -lm

snis_event_callback.o:	snis_event_callback.h snis_event_callback.c
	$(CC) ${MYCFLAGS} -c snis_event_callback.c

${SSGL}:
	(cd ssgl ; make )

mostly-clean:
	rm -f ${SERVEROBJS} ${CLIENTOBJS} ${PROGS} ${SSGL} stl_parser snis_limited_graph.c snis_limited_client.c test-space-partition
	( cd ssgl; make clean )

test_marshal:	snis_marshal.c snis_marshal.h
	$(CC) -DTEST_MARSHALL -o test_marshal snis_marshal.c

test-quat:	test-quat.c quat.o matrix.o mathutils.o
	gcc -Wall -Wextra --pedantic -o test-quat test-quat.c quat.o matrix.o mathutils.o -lm

test-fleet: quat.o fleet.o mathutils.o
	gcc -DTESTFLEET=1 -c -o test-fleet.o fleet.c
	gcc -DTESTFLEET=1 -o test-fleet test-fleet.o mathutils.o quat.o -lm

clean:	mostly-clean
	rm -f ${MODELS} test_marshal


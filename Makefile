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
		snis_faction.o mtwist.o infinite-taunt.o snis_damcon_systems.o
SERVEROBJS=${COMMONOBJS} snis_server.o names.o starbase-comms.o \
		power-model.o quat.o vec4.o matrix.o snis_event_callback.o space-part.o fleet.o \
		commodities.o

CLIENTOBJS=${COMMONOBJS} ${OGGOBJ} ${SNDOBJS} shader.o graph_dev_opengl.o snis_ui_element.o snis_graph.o \
	snis_client.o snis_font.o snis_text_input.o \
	snis_typeface.o snis_gauge.o snis_button.o snis_label.o snis_sliders.o snis_text_window.o \
	mesh.o material.o \
	stl_parser.o entity.o matrix.o my_point.o liang-barsky.o joystick.o quat.o vec4.o

LIMCLIENTOBJS=${COMMONOBJS} ${OGGOBJ} ${SNDOBJS} graph_dev_gdk.o snis_ui_element.o snis_limited_graph.o \
	snis_limited_client.o snis_font.o snis_text_input.o \
	snis_typeface.o snis_gauge.o snis_button.o snis_label.o snis_sliders.o snis_text_window.o \
	mesh.o material.o \
	stl_parser.o entity.o matrix.o my_point.o liang-barsky.o joystick.o quat.o vec4.o fleet.o

SSGL=ssgl/libssglclient.a
LIBS=-lGLEW -Lssgl -lssglclient -lrt -lm ${LUALIBS} ${PNGLIBS}
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
	${MD}/starbase6.stl \
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
	${MD}/cargocontainer.stl \
	${MD}/spaceship_turret.stl \
	${MD}/spaceship_turret_base.stl

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

graph_dev_opengl.o : graph_dev_opengl.c Makefile
	$(Q)$(GLEXTCOMPILE)

graph_dev_gdk.o : graph_dev_gdk.c Makefile
	$(Q)$(GTKCOMPILE)

material.o : material.c Makefile
	$(Q)$(GTKCOMPILE)

shader.o : shader.c Makefile
	$(Q)$(COMPILE)

starbase.stl:	starbase.scad wedge.scad
	$(Q)$(OPENSCAD)

%.stl:	%.scad
	$(Q)$(OPENSCAD)

%.scad_params.h: %.scad
	awk -f extract_scad_params.awk $< > $@

my_point.o:   my_point.c Makefile
	$(Q)$(COMPILE)

mesh.o:   mesh.c Makefile
	$(Q)$(COMPILE)

power-model.o:   power-model.c Makefile
	$(Q)$(COMPILE)

stacktrace.o:   stacktrace.c Makefile
	$(Q)$(COMPILE)

snis_ship_type.o:   snis_ship_type.c Makefile
	$(Q)$(COMPILE)

snis_faction.o:   snis_faction.c Makefile
	$(Q)$(COMPILE)

liang-barsky.o:   liang-barsky.c Makefile
	$(Q)$(COMPILE)

joystick.o:   joystick.c Makefile
	$(Q)$(COMPILE)

ogg_to_pcm.o:   ogg_to_pcm.c Makefile
	$(Q)$(VORBISCOMPILE)

bline.o:	bline.c Makefile
	$(Q)$(COMPILE)

wwviaudio.o:    wwviaudio.c Makefile
	$(Q)$(VORBISCOMPILE)

shield_strength.o:	shield_strength.c Makefile
	$(Q)$(COMPILE)

snis_server.o:	snis_server.c Makefile
	$(Q)$(COMPILE)

snis_client.o:	snis_client.c Makefile
	$(Q)$(GLEXTCOMPILE)

snis_limited_client.c:	snis_client.c Makefile
	cp snis_client.c snis_limited_client.c

snis_limited_client.o:	snis_limited_client.c Makefile
	$(Q)$(LIMCOMPILE)

snis_socket_io.o:	snis_socket_io.c Makefile
	$(Q)$(COMPILE)

snis_marshal.o:	snis_marshal.c Makefile
	$(Q)$(COMPILE)

snis_font.o:	snis_font.c Makefile
	$(Q)$(COMPILE)

mathutils.o:	mathutils.c Makefile
	$(Q)$(COMPILE)

snis_alloc.o:	snis_alloc.c Makefile
	$(Q)$(COMPILE)

snis_damcon_systems.o:	snis_damcon_systems.c Makefile
	$(Q)$(COMPILE)

snis_server:	${SERVEROBJS} ${SSGL} Makefile
	$(Q)$(SERVERLINK)

snis_client:	${CLIENTOBJS} ${SSGL} Makefile
	$(Q)$(CLIENTLINK)

snis_limited_client:	${LIMCLIENTOBJS} ${SSGL} Makefile
	$(Q)$(LIMCLIENTLINK)

starbase-comms.o:	starbase-comms.c Makefile
	$(Q)$(COMPILE)	

infinite-taunt.o:	infinite-taunt.c Makefile
	$(Q)$(COMPILE)

infinite-taunt:	infinite-taunt.o Makefile
	$(CC) -DTEST_TAUNT -o infinite-taunt ${MYCFLAGS} mtwist.o infinite-taunt.c

names:	names.c names.h
	$(CC) -DTEST_NAMES -o names ${MYCFLAGS} ${GTKCFLAGS} names.c

snis_limited_graph.c:	snis_graph.c Makefile
	cp snis_graph.c snis_limited_graph.c

snis_limited_graph.o:	snis_limited_graph.c Makefile
	$(Q)$(LIMCOMPILE)

snis_graph.o:	snis_graph.c Makefile
	$(Q)$(GLEXTCOMPILE)

snis_typeface.o:	snis_typeface.c Makefile
	$(Q)$(GTKCOMPILE)

snis_gauge.o:	snis_gauge.c Makefile
	$(Q)$(GTKCOMPILE)

snis_button.o:	snis_button.c Makefile
	$(Q)$(GTKCOMPILE)

snis_label.o:	snis_label.c Makefile
	$(Q)$(GTKCOMPILE)

snis_sliders.o:	snis_sliders.c Makefile
	$(Q)$(GTKCOMPILE)

snis_text_window.o:	snis_text_window.c Makefile
	$(Q)$(GTKCOMPILE)

snis_ui_element.o:	snis_ui_element.c Makefile
	$(Q)$(GTKCOMPILE)

snis_text_input.o:	snis_text_input.c Makefile
	$(Q)$(GTKCOMPILE)

matrix.o:	matrix.c Makefile
	$(Q)$(COMPILE)

stl_parser.o:	stl_parser.c Makefile
	$(Q)$(COMPILE)

stl_parser:	stl_parser.c matrix.o mesh.o mathutils.o quat.o mtwist.o Makefile
	$(CC) -DTEST_STL_PARSER ${MYCFLAGS} ${GTKCFLAGS} -o stl_parser stl_parser.c matrix.o mesh.o mathutils.o quat.o mtwist.o -lm

entity.o:	entity.c Makefile
	$(Q)$(GTKCOMPILE)

names.o:	names.c Makefile
	$(Q)$(COMPILE)

space-part.o:	space-part.c Makefile
	$(Q)$(COMPILE)

quat.o:	quat.c Makefile
	$(Q)$(COMPILE)

vec4.o:	vec4.c Makefile
	$(Q)$(COMPILE)

mtwist.o:	mtwist.c Makefile
	$(Q)$(COMPILE)

fleet.o:	fleet.c Makefile
	$(Q)$(COMPILE)

commodities.o:	commodities.c Makefile
	$(Q)$(COMPILE)

test-matrix:	matrix.c Makefile
	$(CC) ${MYCFLAGS} ${GTKCFLAGS} -DTEST_MATRIX -o test-matrix matrix.c -lm

test-space-partition:	space-part.c Makefile
	$(CC) ${MYCFLAGS} -g -DTEST_SPACE_PARTITION -o test-space-partition space-part.c -lm

snis_event_callback.o:	snis_event_callback.c Makefile
	$(Q)$(COMPILE)

${SSGL}:
	(cd ssgl ; make )

mostly-clean:
	rm -f ${SERVEROBJS} ${CLIENTOBJS} ${LIMCLIENTOBJS} ${PROGS} ${SSGL} stl_parser snis_limited_graph.c snis_limited_client.c test-space-partition
	( cd ssgl; make clean )

test-marshal:	snis_marshal.c stacktrace.o Makefile
	$(CC) -DTEST_MARSHALL -o test-marshal snis_marshal.c stacktrace.o

test-quat:	test-quat.c quat.o matrix.o mathutils.o mtwist.o Makefile
	gcc -Wall -Wextra --pedantic -o test-quat test-quat.c quat.o matrix.o mathutils.o mtwist.o -lm

test-fleet: quat.o fleet.o mathutils.o mtwist.o Makefile
	gcc -DTESTFLEET=1 -c -o test-fleet.o fleet.c
	gcc -DTESTFLEET=1 -o test-fleet test-fleet.o mathutils.o quat.o mtwist.o -lm

test-mtwist: mtwist.o test-mtwist.c Makefile
	gcc -o test-mtwist mtwist.o test-mtwist.c

test-commodities:	commodities.o Makefile
	gcc -DTESTCOMMODITIES=1 -c commodities.c -o test-commodities.o
	gcc -DTESTCOMMODITIES=1 -o test-commodities test-commodities.o

test-obj-parser:	test-obj-parser.c stl_parser.o mesh.o mtwist.o mathutils.o matrix.o quat.o Makefile
	gcc -o test-obj-parser stl_parser.o mtwist.o mathutils.o matrix.o mesh.o quat.o -lm test-obj-parser.c

test :	test-matrix test-space-partition test-marshal test-quat test-fleet test-mtwist

clean:	mostly-clean
	rm -f ${MODELS} test_marshal

Makefile.depend :
	makedepend -w0 -f- *.c | grep -v /usr > Makefile.depend

include Makefile.depend


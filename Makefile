# To compile withaudio, WITHAUDIO=yes, 
# for no audio support, change to WITHAUDIO=no, 
WITHAUDIO=yes
# WITHAUDIO=no

PREFIX=/usr/local
DATADIR=${PREFIX}/share/snis
CONFIGFILEDIR=${DATADIR}
CONFIGSRCDIR=share/snis
CONFIGFILES=${CONFIGSRCDIR}/commodities.txt \
	${CONFIGSRCDIR}/factions.txt \
	${CONFIGSRCDIR}/ship_types.txt

MODELSRCDIR=share/snis/models
MODELDIR=${DATADIR}/models
SOUNDDIR=${DATADIR}/sounds
SOUNDSRCDIR=share/snis/sounds
SOUNDFILES=${SOUNDSRCDIR}/Attribution.txt \
	${SOUNDSRCDIR}/big_explosion.ogg \
	${SOUNDSRCDIR}/bigshotlaser.ogg \
	${SOUNDSRCDIR}/changescreen.ogg \
	${SOUNDSRCDIR}/comms-hail.ogg \
	${SOUNDSRCDIR}/crewmember-has-joined.ogg \
	${SOUNDSRCDIR}/dangerous-radiation.ogg \
	${SOUNDSRCDIR}/entering-high-security-area.ogg \
	${SOUNDSRCDIR}/flak_gun_sound.ogg \
	${SOUNDSRCDIR}/fuel-levels-critical.ogg \
	${SOUNDSRCDIR}/incoming-fire-detected.ogg \
	${SOUNDSRCDIR}/laser-fail.ogg \
	${SOUNDSRCDIR}/leaving-high-security-area.ogg \
	${SOUNDSRCDIR}/new-starship.ogg \
	${SOUNDSRCDIR}/offscreen.ogg \
	${SOUNDSRCDIR}/onscreen.ogg \
	${SOUNDSRCDIR}/proximity-alert.ogg \
	${SOUNDSRCDIR}/red-alert.ogg \
	${SOUNDSRCDIR}/reverse.ogg \
	${SOUNDSRCDIR}/science-data-acquired.ogg \
	${SOUNDSRCDIR}/science-probe.ogg \
	${SOUNDSRCDIR}/short-warpdrive.ogg \
	${SOUNDSRCDIR}/slider-noise.ogg \
	${SOUNDSRCDIR}/spaceship-crash.ogg \
	${SOUNDSRCDIR}/torpedo-loading.ogg \
	${SOUNDSRCDIR}/transporter-sound.ogg \
	${SOUNDSRCDIR}/tty-chatter.ogg \
	${SOUNDSRCDIR}/warning-hull-breach-imminent.ogg \
	${SOUNDSRCDIR}/warpdrive.ogg
TEXTUREDIR=${DATADIR}/textures
TEXTURESRCDIR=share/snis/textures
TEXTUREFILES=${TEXTURESRCDIR}/AreaTex.h \
	${TEXTURESRCDIR}/asteroid1-0.png \
	${TEXTURESRCDIR}/asteroid1-1.png \
	${TEXTURESRCDIR}/asteroid1-2.png \
	${TEXTURESRCDIR}/asteroid1-3.png \
	${TEXTURESRCDIR}/asteroid1-4.png \
	${TEXTURESRCDIR}/asteroid1-5.png \
	${TEXTURESRCDIR}/asteroid2-0.png \
	${TEXTURESRCDIR}/asteroid2-1.png \
	${TEXTURESRCDIR}/asteroid2-2.png \
	${TEXTURESRCDIR}/asteroid2-3.png \
	${TEXTURESRCDIR}/asteroid2-4.png \
	${TEXTURESRCDIR}/asteroid2-5.png \
	${TEXTURESRCDIR}/Attribution.txt \
	${TEXTURESRCDIR}/green-laser-texture.png \
	${TEXTURESRCDIR}/image0.png \
	${TEXTURESRCDIR}/image1.png \
	${TEXTURESRCDIR}/image2.png \
	${TEXTURESRCDIR}/image3.png \
	${TEXTURESRCDIR}/image4.png \
	${TEXTURESRCDIR}/image5.png \
	${TEXTURESRCDIR}/nebula0.png \
	${TEXTURESRCDIR}/nebula1.png \
	${TEXTURESRCDIR}/nebula2.png \
	${TEXTURESRCDIR}/nebula3.png \
	${TEXTURESRCDIR}/nebula4.png \
	${TEXTURESRCDIR}/nebula5.png \
	${TEXTURESRCDIR}/orange-haze0.png \
	${TEXTURESRCDIR}/orange-haze1.png \
	${TEXTURESRCDIR}/orange-haze2.png \
	${TEXTURESRCDIR}/orange-haze3.png \
	${TEXTURESRCDIR}/orange-haze4.png \
	${TEXTURESRCDIR}/orange-haze5.png \
	${TEXTURESRCDIR}/planetary-ring0.png \
	${TEXTURESRCDIR}/planetary-ring1.png \
	${TEXTURESRCDIR}/planet-texture0-0.png \
	${TEXTURESRCDIR}/planet-texture0-1.png \
	${TEXTURESRCDIR}/planet-texture0-2.png \
	${TEXTURESRCDIR}/planet-texture0-3.png \
	${TEXTURESRCDIR}/planet-texture0-4.png \
	${TEXTURESRCDIR}/planet-texture0-5.png \
	${TEXTURESRCDIR}/planet-texture0.png \
	${TEXTURESRCDIR}/planet-texture1-0.png \
	${TEXTURESRCDIR}/planet-texture1-1.png \
	${TEXTURESRCDIR}/planet-texture1-2.png \
	${TEXTURESRCDIR}/planet-texture1-3.png \
	${TEXTURESRCDIR}/planet-texture1-4.png \
	${TEXTURESRCDIR}/planet-texture1-5.png \
	${TEXTURESRCDIR}/planet-texture1.png \
	${TEXTURESRCDIR}/planet-texture2-0.png \
	${TEXTURESRCDIR}/planet-texture2-1.png \
	${TEXTURESRCDIR}/planet-texture2-2.png \
	${TEXTURESRCDIR}/planet-texture2-3.png \
	${TEXTURESRCDIR}/planet-texture2-4.png \
	${TEXTURESRCDIR}/planet-texture2-5.png \
	${TEXTURESRCDIR}/planet-texture2.png \
	${TEXTURESRCDIR}/planet-texture3-0.png \
	${TEXTURESRCDIR}/planet-texture3-1.png \
	${TEXTURESRCDIR}/planet-texture3-2.png \
	${TEXTURESRCDIR}/planet-texture3-3.png \
	${TEXTURESRCDIR}/planet-texture3-4.png \
	${TEXTURESRCDIR}/planet-texture3-5.png \
	${TEXTURESRCDIR}/planet-texture3.png \
	${TEXTURESRCDIR}/planet-texture4-0.png \
	${TEXTURESRCDIR}/planet-texture4-1.png \
	${TEXTURESRCDIR}/planet-texture4-2.png \
	${TEXTURESRCDIR}/planet-texture4-3.png \
	${TEXTURESRCDIR}/planet-texture4-4.png \
	${TEXTURESRCDIR}/planet-texture4-5.png \
	${TEXTURESRCDIR}/red-laser-texture.png \
	${TEXTURESRCDIR}/red-torpedo-texture.png \
	${TEXTURESRCDIR}/SearchTex.h \
	${TEXTURESRCDIR}/space-blue-plasma.png \
	${TEXTURESRCDIR}/spark-texture.png \
	${TEXTURESRCDIR}/sun.png \
	${TEXTURESRCDIR}/test0.png \
	${TEXTURESRCDIR}/test1.png \
	${TEXTURESRCDIR}/test2.png \
	${TEXTURESRCDIR}/test3.png \
	${TEXTURESRCDIR}/test4.png \
	${TEXTURESRCDIR}/test5.png \
	${TEXTURESRCDIR}/thrust.png \
	${TEXTURESRCDIR}/warp-effect.png \
	${TEXTURESRCDIR}/wormhole.png

LUASCRIPTDIR=${DATADIR}/luascripts
LUASRCDIR=share/snis/luascripts
LUASCRIPTS=${LUASRCDIR}/CLEAR_ALL.LUA \
	${LUASRCDIR}/COLLISION.LUA \
	${LUASRCDIR}/HELLO.LUA \
	${LUASRCDIR}/initialize.lua \
	${LUASRCDIR}/SILLY-SPACE-BATTLE.LUA \
	${LUASRCDIR}/TRAINING-MISSION-1.LUA \
	${LUASRCDIR}/TRAINING-MISSION-2.LUA
MATERIALDIR=${DATADIR}/materials
MATERIALSRCDIR=share/snis/materials
MATERIALFILES=${MATERIALSRCDIR}/nebula0.mat \
	${MATERIALSRCDIR}/nebula10.mat \
	${MATERIALSRCDIR}/nebula11.mat \
	${MATERIALSRCDIR}/nebula12.mat \
	${MATERIALSRCDIR}/nebula13.mat \
	${MATERIALSRCDIR}/nebula14.mat \
	${MATERIALSRCDIR}/nebula15.mat \
	${MATERIALSRCDIR}/nebula16.mat \
	${MATERIALSRCDIR}/nebula17.mat \
	${MATERIALSRCDIR}/nebula18.mat \
	${MATERIALSRCDIR}/nebula19.mat \
	${MATERIALSRCDIR}/nebula1.mat \
	${MATERIALSRCDIR}/nebula2.mat \
	${MATERIALSRCDIR}/nebula3.mat \
	${MATERIALSRCDIR}/nebula4.mat \
	${MATERIALSRCDIR}/nebula5.mat \
	${MATERIALSRCDIR}/nebula6.mat \
	${MATERIALSRCDIR}/nebula7.mat \
	${MATERIALSRCDIR}/nebula8.mat \
	${MATERIALSRCDIR}/nebula9.mat

SHADERDIR=${PREFIX}/share/snis/shader
SHADERSRCDIR=share/snis/shader
SHADERS=${SHADERSRCDIR}/color_by_w.frag \
	${SHADERSRCDIR}/color_by_w.vert \
	${SHADERSRCDIR}/fs-effect-copy.frag \
	${SHADERSRCDIR}/fs-effect-copy.vert \
	${SHADERSRCDIR}/line-single-color.frag \
	${SHADERSRCDIR}/line-single-color.vert \
	${SHADERSRCDIR}/per_vertex_color.frag \
	${SHADERSRCDIR}/per_vertex_color.vert \
	${SHADERSRCDIR}/point_cloud.frag \
	${SHADERSRCDIR}/point_cloud-intensity-noise.frag \
	${SHADERSRCDIR}/point_cloud-intensity-noise.vert \
	${SHADERSRCDIR}/point_cloud.vert \
	${SHADERSRCDIR}/single_color.frag \
	${SHADERSRCDIR}/single-color-lit-per-pixel.frag \
	${SHADERSRCDIR}/single-color-lit-per-pixel.vert \
	${SHADERSRCDIR}/single-color-lit-per-vertex.frag \
	${SHADERSRCDIR}/single-color-lit-per-vertex.vert \
	${SHADERSRCDIR}/single_color.vert \
	${SHADERSRCDIR}/skybox.frag \
	${SHADERSRCDIR}/skybox.vert \
	${SHADERSRCDIR}/smaa-blend.shader \
	${SHADERSRCDIR}/smaa-edge.shader \
	${SHADERSRCDIR}/smaa-high.shader \
	${SHADERSRCDIR}/SMAA.hlsl \
	${SHADERSRCDIR}/smaa-neighborhood.shader \
	${SHADERSRCDIR}/textured-and-lit-per-vertex.frag \
	${SHADERSRCDIR}/textured-and-lit-per-vertex.vert \
	${SHADERSRCDIR}/textured-cubemap-and-lit-per-vertex.frag \
	${SHADERSRCDIR}/textured-cubemap-and-lit-per-vertex.vert \
	${SHADERSRCDIR}/textured-cubemap-and-lit-with-annulus-shadow-per-pixel.frag \
	${SHADERSRCDIR}/textured-cubemap-and-lit-with-annulus-shadow-per-pixel.vert \
	${SHADERSRCDIR}/textured.frag \
	${SHADERSRCDIR}/textured-particle.frag \
	${SHADERSRCDIR}/textured-particle.vert \
	${SHADERSRCDIR}/textured.vert \
	${SHADERSRCDIR}/textured-with-sphere-shadow-per-pixel.frag \
	${SHADERSRCDIR}/textured-with-sphere-shadow-per-pixel.vert \
	${SHADERSRCDIR}/wireframe_filled.frag \
	${SHADERSRCDIR}/wireframe_filled.vert \
	${SHADERSRCDIR}/wireframe_transparent.frag \
	${SHADERSRCDIR}/wireframe-transparent-sphere-clip.frag \
	${SHADERSRCDIR}/wireframe-transparent-sphere-clip.vert \
	${SHADERSRCDIR}/wireframe_transparent.vert

MANSRCDIR=.
MANPAGES=${MANSRCDIR}/snis_client.6.gz ${MANSRCDIR}/snis_server.6.gz
MANDIR=${PREFIX}/share/man/man6

CC=gcc

ifeq (${WITHAUDIO},yes)
SNDLIBS:=$(shell pkg-config --libs portaudio-2.0 vorbisfile)
SNDFLAGS:=-DWITHAUDIOSUPPORT $(shell pkg-config --cflags portaudio-2.0) -DDATADIR=\"${DATADIR}\"
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

# Arch pkg-config seems to be broken for lua5.2, so we have
# this "... || echo" hack thing.
#
LUALIBS:=$(shell pkg-config --libs lua5.2 || echo '-llua')
LUACFLAGS:=$(shell pkg-config --cflags lua5.2 || echo '')

PNGLIBS:=$(shell pkg-config --libs libpng)
PNGCFLAGS:=$(shell pkg-config --cflags libpng)

SDLLIBS:=$(shell pkg-config sdl --libs)
SDLCFLAGS:=$(shell pkg-config sdl --cflags)

COMMONOBJS=mathutils.o snis_alloc.o snis_socket_io.o snis_marshal.o \
		bline.o shield_strength.o stacktrace.o snis_ship_type.o \
		snis_faction.o mtwist.o infinite-taunt.o snis_damcon_systems.o \
		string-utils.o
SERVEROBJS=${COMMONOBJS} snis_server.o names.o starbase-comms.o \
		power-model.o quat.o vec4.o matrix.o snis_event_callback.o space-part.o fleet.o \
		commodities.o

COMMONCLIENTOBJS=${COMMONOBJS} ${OGGOBJ} ${SNDOBJS} snis_ui_element.o snis_font.o snis_text_input.o \
	snis_typeface.o snis_gauge.o snis_button.o snis_label.o snis_sliders.o snis_text_window.o \
	mesh.o material.o stl_parser.o entity.o matrix.o my_point.o liang-barsky.o joystick.o quat.o vec4.o

CLIENTOBJS=${COMMONCLIENTOBJS} shader.o graph_dev_opengl.o opengl_cap.o snis_graph.o snis_client.o

LIMCLIENTOBJS=${COMMONCLIENTOBJS} graph_dev_gdk.o snis_limited_graph.o snis_limited_client.o

SDLCLIENTOBJS=${COMMONCLIENTOBJS} shader.o graph_dev_opengl.o opengl_cap.o snis_graph.o mesh_viewer.o

SSGL=ssgl/libssglclient.a
LIBS=-lGLEW -lGL -Lssgl -lssglclient -lrt -lm ${LUALIBS} ${PNGLIBS}
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


PROGS=snis_server snis_client snis_limited_client mesh_viewer

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
	${MD}/spaceship_turret_base.stl \
	${MD}/vanquisher.stl \
	${MD}/enforcer.stl

MYCFLAGS=-DPREFIX=${PREFIX} ${DEBUGFLAG} ${PROFILEFLAG} ${OPTIMIZEFLAG}\
	--pedantic -Wall ${STOP_ON_WARN} -pthread -std=gnu99 -rdynamic
GTKCFLAGS:=$(subst -I,-isystem ,$(shell pkg-config --cflags gtk+-2.0))
GLEXTCFLAGS:=$(subst -I,-isystem ,$(shell pkg-config --cflags gtkglext-1.0)) ${PNGCFLAGS}
GTKLDFLAGS:=$(shell pkg-config --libs gtk+-2.0) $(shell pkg-config --libs gthread-2.0)
GLEXTLDFLAGS:=$(shell pkg-config --libs gtkglext-1.0)
VORBISFLAGS:=$(subst -I,-isystem ,$(shell pkg-config --cflags vorbisfile))

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
SDLCOMPILE=$(CC) ${MYCFLAGS} ${SDLCFLAGS} -c -o $@ $< && $(ECHO) '  COMPILE' $<

CLIENTLINK=$(CC) ${MYCFLAGS} ${SNDFLAGS} -o $@ ${GTKCFLAGS} ${GLEXTCFLAGS} ${CLIENTOBJS} ${GTKLDFLAGS} ${GLEXTLDFLAGS} ${LIBS} ${SNDLIBS} && $(ECHO) '  LINK' $@
LIMCLIENTLINK=$(CC) ${MYCFLAGS} ${SNDFLAGS} -o $@ ${GTKCFLAGS} ${LIMCLIENTOBJS} ${GLEXTLDFLAGS} ${LIBS} ${SNDLIBS} && $(ECHO) '  LINK' $@
SDLCLIENTLINK=$(CC) ${MYCFLAGS} ${SNDFLAGS} -o $@ ${SDLCFLAGS} ${SDLCLIENTOBJS} ${SDLLIBS} ${LIBS} ${SNDLIBS} && $(ECHO) '  LINK' $@
SERVERLINK=$(CC) ${MYCFLAGS} -o $@ ${GTKCFLAGS} ${SERVEROBJS} ${GTKLDFLAGS} ${LIBS} && $(ECHO) '  LINK' $@
OPENSCAD=openscad -o $@ $< && $(ECHO) '  OPENSCAD' $<

all:	${COMMONOBJS} ${SERVEROBJS} ${CLIENTOBJS} ${LIMCLIENTOBJS} ${PROGS} ${MODELS}

graph_dev_opengl.o : graph_dev_opengl.c Makefile
	$(Q)$(GLEXTCOMPILE)

opengl_cap.o : opengl_cap.c Makefile
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

mesh_viewer.o:	mesh_viewer.c Makefile
	$(Q)$(SDLCOMPILE)

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

mesh_viewer:	${SDLCLIENTOBJS} ${SSGL} Makefile
	$(Q)$(SDLCLIENTLINK)

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

string-utils.o:	string-utils.c Makefile
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
	rm -f ${SERVEROBJS} ${CLIENTOBJS} ${LIMCLIENTOBJS} ${SDLCLIENTOBJS} ${PROGS} ${SSGL} stl_parser snis_limited_graph.c snis_limited_client.c test-space-partition
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

test :	test-matrix test-space-partition test-marshal test-quat test-fleet test-mtwist test-commodities

snis_client.6.gz:	snis_client.6
	gzip -9 - < snis_client.6 > snis_client.6.gz

snis_server.6.gz:	snis_server.6
	gzip -9 - < snis_server.6 > snis_server.6.gz

install:	${PROGS} ${MODELS} ${AUDIOFILES} ${TEXTURES} \
		${MATERIALS} ${CONFIGFILES} ${SHADERS} ${LUASCRIPTS} ${MANPAGES} ${SSGL}
	mkdir -p ${PREFIX}/bin
	cp ${PROGS} ${PREFIX}/bin
	cp ssgl/ssgl_server ${PREFIX}/bin
	for x in ${PROGS} ; do \
		chmod +x ${PREFIX}/bin/$$x ; \
	done
	for d in ${MATERIALDIR} ${LUASCRIPTDIR} ${SHADERDIR} ${SOUNDDIR} \
		${TEXTUREDIR} ${MODELDIR}/wombat ${MODELDIR}/dreadknight \
		${MODELDIR}/conqueror ${SHADERDIR} ; do \
		mkdir -p $$d ; \
	done
	cp ${CONFIGFILES} ${CONFIGFILEDIR}
	cp ${SOUNDFILES} ${SOUNDDIR}
	cp ${TEXTUREFILES} ${TEXTUREDIR}
	cp ${LUASCRIPTS} ${LUASCRIPTDIR}
	cp ${MATERIALFILES} ${MATERIALDIR}
	cp ${MODELS} ${MODELDIR}
	cp ${MODELSRCDIR}/conqueror/conqueror.mtl  ${MODELDIR}/conqueror
	cp ${MODELSRCDIR}/conqueror/conqueror.obj  ${MODELDIR}/conqueror
	cp ${MODELSRCDIR}/conqueror/conqueror.png  ${MODELDIR}/conqueror
	cp ${MODELSRCDIR}/wombat/snis3006lights.png ${MODELDIR}/wombat
	cp ${MODELSRCDIR}/wombat/snis3006.mtl ${MODELDIR}/wombat
	cp ${MODELSRCDIR}/wombat/snis3006.obj ${MODELDIR}/wombat
	cp ${MODELSRCDIR}/wombat/snis3006.png ${MODELDIR}/wombat
	cp ${MODELSRCDIR}/dreadknight/dreadknight-exhaust-plumes.h ${MODELDIR}/dreadknight
	cp ${MODELSRCDIR}/dreadknight/dreadknight.mtl ${MODELDIR}/dreadknight
	cp ${MODELSRCDIR}/dreadknight/dreadknight.obj ${MODELDIR}/dreadknight
	cp ${MODELSRCDIR}/dreadknight/dreadknight.png ${MODELDIR}/dreadknight
	cp ${SHADERS} ${SHADERDIR}
	mkdir -p ${MANDIR}
	cp ${MANPAGES} ${MANDIR}

uninstall:
	if [ ! -d "${PREFIX}" ] ; then \
		echo "PREFIX is not a directory." 1>&2 ;\
		exit 1 ;\
	fi
	for x in ${PROGS} ; do \
		rm -f ${PREFIX}/bin/$$x ; \
	done
	rm -f ${PREFIX}/bin/ssgl_server
	rm -fr ${PREFIX}/share/snis
	rm -f ${MANDIR}/snis_client.6.gz ${MANDIR}/snis_client.6
	rm -f ${MANDIR}/snis_server.6.gz ${MANDIR}/snis_server.6

clean:	mostly-clean
	rm -f ${MODELS} test_marshal

depend :
	rm -f Makefile.depend
	$(MAKE) Makefile.depend

Makefile.depend :
	# Do in 2 steps so that on failure we don't get an empty but "up to date"
	# dependencies file.
	makedepend -w0 -f- *.c | grep -v /usr | sort > Makefile.depend.tmp
	mv Makefile.depend.tmp Makefile.depend

include Makefile.depend


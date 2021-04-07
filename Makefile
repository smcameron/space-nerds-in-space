# To compile with voice chat, WITHVOICECHAT=yes,
# for no voice chat, make WITHVOICECHAT=no
WITHVOICECHAT=yes
USE_SNIS_XWINDOWS_HACKS=1
USE_CUSTOM_STRLCPY=0
PKG_CONFIG?=pkg-config

# use "make OSX=1" for mac
OSX=0

# -lrt is only needed for clock_gettime() and only for glibc before 2.17
LRTLIB=$(shell ./check_for_lrt.sh -q)

INSTALL ?= install
AWK ?= awk

# Type "make O=0" to get a debug build (-g)
# "make O=1" gets an optimized (-O3) build with no debug info, which is the default
# optimize/debug (-O3, -g: default 1=yes/no, 2=no/yes)
O ?= 1

# Type "make P=1" to get a build with profiling information for use with
# gprof. This is turned off by default
# profile (default 0=no, 1=yes)
P ?= 0

# Type "make V=1" to get a verbose build with all commands printed
# By default, this is turned off.
# verbose (default 0=no, 1=yes)
V ?= 0

CC ?= gcc

# DESTDIR=.
PREFIX?=.

# object fild directory
OD=object_files

DATADIR=${DESTDIR}/${PREFIX}/share/snis
CONFIGFILEDIR=${DATADIR}
CONFIGSRCDIR=./share/snis
CONFIGFILES=${CONFIGSRCDIR}/commodities.txt \
	${CONFIGSRCDIR}/factions.txt \
	${CONFIGSRCDIR}/ship_types.txt \
	${CONFIGSRCDIR}/starbase_models.txt \
	${CONFIGSRCDIR}/user_colors.cfg \
	${CONFIGSRCDIR}/joystick_config.txt
SOLARSYSTEMSRCDIR=${CONFIGSRCDIR}/solarsystems
SOLARSYSTEMDIR=${DATADIR}/solarsystems
SOLARSYSTEMFILES=${SOLARSYSTEMSRCDIR}/default/assets.txt

ASSETSSRCDIR=share/snis

MODELSRCDIR=${ASSETSSRCDIR}/models
MODELDIR=${DATADIR}/models
SOUNDDIR=${DATADIR}/sounds
SOUNDSRCDIR=${ASSETSSRCDIR}/sounds
SOUNDFILES=${SOUNDSRCDIR}/Attribution.txt \
	${SOUNDSRCDIR}/atmospheric-friction.ogg \
	${SOUNDSRCDIR}/big_explosion.ogg \
	${SOUNDSRCDIR}/bigshotlaser.ogg \
	${SOUNDSRCDIR}/changescreen.ogg \
	${SOUNDSRCDIR}/comms-hail.ogg \
	${SOUNDSRCDIR}/crewmember-has-joined.ogg \
	${SOUNDSRCDIR}/dangerous-radiation.ogg \
	${SOUNDSRCDIR}/docking-sound.ogg \
	${SOUNDSRCDIR}/docking-system-disengaged.ogg \
	${SOUNDSRCDIR}/docking-system-engaged.ogg \
	${SOUNDSRCDIR}/entering-high-security-area.ogg \
	${SOUNDSRCDIR}/flak_gun_sound.ogg \
	${SOUNDSRCDIR}/flak_hit.ogg \
	${SOUNDSRCDIR}/fuel-levels-critical.ogg \
	${SOUNDSRCDIR}/incoming-fire-detected.ogg \
	${SOUNDSRCDIR}/laser-fail.ogg \
	${SOUNDSRCDIR}/leaving-high-security-area.ogg \
	${SOUNDSRCDIR}/mining-bot-deployed.ogg \
	${SOUNDSRCDIR}/mining-bot-standing-by.ogg \
	${SOUNDSRCDIR}/mining-bot-stowed.ogg \
	${SOUNDSRCDIR}/new-starship.ogg \
	${SOUNDSRCDIR}/offscreen.ogg \
	${SOUNDSRCDIR}/onscreen.ogg \
	${SOUNDSRCDIR}/permission-to-dock-denied.ogg \
	${SOUNDSRCDIR}/permission-to-dock-has-expired.ogg \
	${SOUNDSRCDIR}/permission-to-dock-granted.ogg \
	${SOUNDSRCDIR}/proximity-alert.ogg \
	${SOUNDSRCDIR}/red-alert.ogg \
	${SOUNDSRCDIR}/reverse.ogg \
	${SOUNDSRCDIR}/robot-insert-component.ogg \
	${SOUNDSRCDIR}/robot-remove-component.ogg \
	${SOUNDSRCDIR}/science-data-acquired.ogg \
	${SOUNDSRCDIR}/science-probe.ogg \
	${SOUNDSRCDIR}/short-warpdrive.ogg \
	${SOUNDSRCDIR}/slider-noise.ogg \
	${SOUNDSRCDIR}/spaceship-crash.ogg \
	${SOUNDSRCDIR}/torpedo-loading.ogg \
	${SOUNDSRCDIR}/transporter-sound.ogg \
	${SOUNDSRCDIR}/tty-chatter.ogg \
	${SOUNDSRCDIR}/warning-hull-breach-imminent.ogg \
	${SOUNDSRCDIR}/warp-drive-fumble.ogg \
	${SOUNDSRCDIR}/warpdrive.ogg \
	${SOUNDSRCDIR}/welcome-to-starbase.ogg \
	${SOUNDSRCDIR}/hull-creak-0.ogg \
	${SOUNDSRCDIR}/hull-creak-1.ogg \
	${SOUNDSRCDIR}/hull-creak-2.ogg \
	${SOUNDSRCDIR}/hull-creak-3.ogg \
	${SOUNDSRCDIR}/hull-creak-4.ogg \
	${SOUNDSRCDIR}/hull-creak-5.ogg \
	${SOUNDSRCDIR}/hull-creak-6.ogg \
	${SOUNDSRCDIR}/hull-creak-7.ogg \
	${SOUNDSRCDIR}/hull-creak-8.ogg \
	${SOUNDSRCDIR}/hull-creak-9.ogg \
	${SOUNDSRCDIR}/ui1.ogg \
	${SOUNDSRCDIR}/ui2.ogg \
	${SOUNDSRCDIR}/ui3.ogg \
	${SOUNDSRCDIR}/ui4.ogg \
	${SOUNDSRCDIR}/ui5.ogg \
	${SOUNDSRCDIR}/ui6.ogg \
	${SOUNDSRCDIR}/ui7.ogg \
	${SOUNDSRCDIR}/ui8.ogg \
	${SOUNDSRCDIR}/ui9.ogg \
	${SOUNDSRCDIR}/ui10.ogg \
	${SOUNDSRCDIR}/ui11.ogg \
	${SOUNDSRCDIR}/ui12.ogg \
	${SOUNDSRCDIR}/ui13.ogg \
	${SOUNDSRCDIR}/ui14.ogg \
	${SOUNDSRCDIR}/ui15.ogg \
	${SOUNDSRCDIR}/ui16.ogg \
	${SOUNDSRCDIR}/ui17.ogg \
	${SOUNDSRCDIR}/ui18.ogg \
	${SOUNDSRCDIR}/ui19.ogg \
	${SOUNDSRCDIR}/ui20.ogg \
	${SOUNDSRCDIR}/ui21.ogg \
	${SOUNDSRCDIR}/ui22.ogg \
	${SOUNDSRCDIR}/ui23.ogg \
	${SOUNDSRCDIR}/ui24.ogg \
	${SOUNDSRCDIR}/ui25.ogg \
	${SOUNDSRCDIR}/ui26.ogg \
	${SOUNDSRCDIR}/ui27.ogg \
	${SOUNDSRCDIR}/ui28.ogg \
	${SOUNDSRCDIR}/ui29.ogg \
	${SOUNDSRCDIR}/spacemonster_slap.ogg \
	${SOUNDSRCDIR}/alarm_buzzer.ogg \
	${SOUNDSRCDIR}/atlas_rocket_sample.ogg \
	${SOUNDSRCDIR}/maneuvering_thruster.ogg \
	${SOUNDSRCDIR}/quindar-intro.ogg \
	${SOUNDSRCDIR}/quindar-outro.ogg \
	${SOUNDSRCDIR}/missile_launch.ogg

TEXTUREDIR=${DATADIR}/textures
TEXTURESRCDIR=${ASSETSSRCDIR}/textures
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
	${TEXTURESRCDIR}/shield-effect-0.png \
	${TEXTURESRCDIR}/shield-effect-1.png \
	${TEXTURESRCDIR}/shield-effect-2.png \
	${TEXTURESRCDIR}/shield-effect-3.png \
	${TEXTURESRCDIR}/shield-effect-4.png \
	${TEXTURESRCDIR}/shield-effect-5.png \
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
	${TEXTURESRCDIR}/warp-tunnel.png \
	${TEXTURESRCDIR}/wormhole.png \
	${TEXTURESRCDIR}/green-burst.png \
	${TEXTURESRCDIR}/blue-tractor-texture.png \
	${TEXTURESRCDIR}/missile_texture.png \
	${TEXTURESRCDIR}/black_hole.png \
	${TEXTURESRCDIR}/docking_port_emit.png \
	${TEXTURESRCDIR}/docking_port_texture.png \
	${TEXTURESRCDIR}/green-spark.png \
	${TEXTURESRCDIR}/laserflash.png \
	${TEXTURESRCDIR}/spacemonster_emit.png \
	${TEXTURESRCDIR}/spacemonster_tentacle_emit.png \
	${TEXTURESRCDIR}/spacemonster_tentacle_texture.png \
	${TEXTURESRCDIR}/spacemonster_texture.png \
	${TEXTURESRCDIR}/spaceplate.png \
	${TEXTURESRCDIR}/spaceplate_small.png \
	${TEXTURESRCDIR}/spaceplate_small_emit.png \
	${TEXTURESRCDIR}/spaceplateemit.png \
	${TEXTURESRCDIR}/thrust_flare.png \
	${TEXTURESRCDIR}/thrustblue.png \
	${TEXTURESRCDIR}/thrustgreen.png \
	${TEXTURESRCDIR}/thrustred.png \
	${TEXTURESRCDIR}/thrustviolet.png \
	${TEXTURESRCDIR}/thrustyellow.png \
	${TEXTURESRCDIR}/warpgate_emit.png \
	${TEXTURESRCDIR}/warpgate_texture.png \
	${TEXTURESRCDIR}/anamorphic_flare.png \
	${TEXTURESRCDIR}/lens_flare_ghost.png \
	${TEXTURESRCDIR}/lens_flare_halo.png

LUASCRIPTDIR=${DATADIR}/luascripts
LUASRCDIR=${ASSETSSRCDIR}/luascripts
LUASCRIPTS=${LUASRCDIR}/MAINMENU.LUA
LUAMISSIONS=${LUASRCDIR}/MISSIONS/BIGSHIP.LUA \
	${LUASRCDIR}/MISSIONS/CUBOMUERTE.LUA \
	${LUASRCDIR}/MISSIONS/DEMOLISHER.LUA \
	${LUASRCDIR}/MISSIONS/KALI.LUA \
	${LUASRCDIR}/MISSIONS/ROYAL-WEDDING.LUA \
	${LUASRCDIR}/MISSIONS/SAVING-PLANET-ERPH.LUA \
	${LUASRCDIR}/MISSIONS/SPACEPOX.LUA \
	${LUASRCDIR}/MISSIONS/STURNVULF.LUA \
	${LUASRCDIR}/MISSIONS/TRAINING-MISSION-1.LUA \
	${LUASRCDIR}/MISSIONS/TRAINING-MISSION-2.LUA \
	${LUASRCDIR}/MISSIONS/missions_menu.txt
LUASRCLIBS=${LUASRCDIR}/MISSIONS/lib/interactive_fiction.lua
LUATESTS=${LUASRCDIR}/TEST/CARGO.LUA \
	${LUASRCDIR}/TEST/COLLISION.LUA \
	${LUASRCDIR}/TEST/REVIEW_MODELS.LUA \
	${LUASRCDIR}/TEST/SILLY-SPACE-BATTLE.LUA \
	${LUASRCDIR}/TEST/SOUND.LUA \
	${LUASRCDIR}/TEST/TESTCUSTOMBUTTONS.LUA \
	${LUASRCDIR}/TEST/TESTDERELICT.LUA \
	${LUASRCDIR}/TEST/TESTDOCK.LUA \
	${LUASRCDIR}/TEST/TESTEXPL.LUA \
	${LUASRCDIR}/TEST/TESTSPHEROID.LUA \
	${LUASRCDIR}/TEST/TESTWARPEVENT.LUA \
	${LUASRCDIR}/TEST/TESTINTFIC.LUA \
	${LUASRCDIR}/TEST/TESTRUNAWAY.LUA \
	${LUASRCDIR}/TEST/TESTTORP.LUA \
	${LUASRCDIR}/TEST/TESTDAMAGE.LUA \
	${LUASRCDIR}/TEST/TESTCIPHER.LUA
LUAUTILS=${LUASRCDIR}/UTIL/CLEAR_ALL.LUA \
	${LUASRCDIR}/UTIL/DAMAGE.LUA \
	${LUASRCDIR}/UTIL/DESTROY_SHIP.LUA \
	${LUASRCDIR}/UTIL/DISABLE_ANTENNA.LUA \
	${LUASRCDIR}/UTIL/ENABLE_ANTENNA.LUA \
	${LUASRCDIR}/UTIL/PAY_PLAYER.LUA \
	${LUASRCDIR}/UTIL/PLAYER_SPEED_BOOST.LUA \
	${LUASRCDIR}/UTIL/PLAYER_SPEED_DEFAULT.LUA \
	${LUASRCDIR}/UTIL/REDALERT.LUA \
	${LUASRCDIR}/UTIL/REGENERATE.LUA \
	${LUASRCDIR}/UTIL/RESETBRIDGE.LUA \
	${LUASRCDIR}/UTIL/RESTORE_SKYBOX.LUA \
	${LUASRCDIR}/UTIL/SAY.LUA \
	${LUASRCDIR}/UTIL/SET_PLANET_DESCRIPTION.LUA \
	${LUASRCDIR}/UTIL/SET_PLANET_ECONOMY.LUA \
	${LUASRCDIR}/UTIL/SET_PLANET_GOVT.LUA \
	${LUASRCDIR}/UTIL/SET_PLANET_SECURITY.LUA \
	${LUASRCDIR}/UTIL/SET_PLANET_TECH.LUA \
	${LUASRCDIR}/UTIL/LS.LUA \
	${LUASRCDIR}/UTIL/SETFACTION.LUA \
	${LUASRCDIR}/UTIL/SET_SB_FACTION_MASK.LUA \
	${LUASRCDIR}/UTIL/utility_menu.txt

MATERIALDIR=${DATADIR}/materials
MATERIALSRCDIR=${ASSETSSRCDIR}/materials
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

SHADERDIR=${DESTDIR}/${PREFIX}/share/snis/shader
SHADERSRCDIR=${ASSETSSRCDIR}/shader
SHADERS=${SHADERSRCDIR}/atmosphere.frag \
	${SHADERSRCDIR}/atmosphere.vert \
	${SHADERSRCDIR}/color_by_w.frag \
	${SHADERSRCDIR}/color_by_w.vert \
	${SHADERSRCDIR}/fs-effect-copy.shader \
	${SHADERSRCDIR}/line-single-color.frag \
	${SHADERSRCDIR}/line-single-color.vert \
	${SHADERSRCDIR}/per_vertex_color.frag \
	${SHADERSRCDIR}/per_vertex_color.vert \
	${SHADERSRCDIR}/point_cloud.frag \
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
	${SHADERSRCDIR}/textured-and-lit-per-pixel.shader \
	${SHADERSRCDIR}/textured-and-lit-per-vertex.shader \
	${SHADERSRCDIR}/textured-cubemap-shield-per-pixel.shader \
	${SHADERSRCDIR}/textured-cubemap-and-lit-with-annulus-shadow-per-pixel.shader \
	${SHADERSRCDIR}/textured-particle.frag \
	${SHADERSRCDIR}/textured-particle.vert \
	${SHADERSRCDIR}/textured.shader \
	${SHADERSRCDIR}/textured-with-sphere-shadow-per-pixel.shader \
	${SHADERSRCDIR}/wireframe_filled.frag \
	${SHADERSRCDIR}/wireframe_filled.vert \
	${SHADERSRCDIR}/wireframe_transparent.frag \
	${SHADERSRCDIR}/wireframe-transparent-sphere-clip.frag \
	${SHADERSRCDIR}/wireframe-transparent-sphere-clip.vert \
	${SHADERSRCDIR}/wireframe_transparent.vert \
	${SHADERSRCDIR}/alpha_by_normal.shader

SCAD_PARAMS_FILES=${MODELSRCDIR}/disruptor.scad_params.h \
	${MODELSRCDIR}/enforcer.scad_params.h \
	${MODELSRCDIR}/starbase5.scad_params.h \
	${MODELSRCDIR}/research-vessel.scad_params.h \
	${MODELSRCDIR}/conqueror.scad_params.h \
	${MODELSRCDIR}/cruiser.scad_params.h \
	${MODELSRCDIR}/asteroid-miner.scad_params.h \
	${MODELSRCDIR}/battlestar.scad_params.h \
	${MODELSRCDIR}/wombat.scad_params.h \
	${MODELSRCDIR}/destroyer.scad_params.h \
	${MODELSRCDIR}/dragonhawk.scad_params.h \
	${MODELSRCDIR}/tanker.scad_params.h \
	${MODELSRCDIR}/swordfish.scad_params.h \
	${MODELSRCDIR}/vanquisher.scad_params.h \
	${MODELSRCDIR}/freighter.scad_params.h \
	${MODELSRCDIR}/scrambler.scad_params.h \
	${MODELSRCDIR}/spaceship2.scad_params.h \
	${MODELSRCDIR}/spaceship3.scad_params.h \
	${MODELSRCDIR}/skorpio.scad_params.h \
	${MODELSRCDIR}/spaceship.scad_params.h \
	${MODELSRCDIR}/transport.scad_params.h \
	${MODELSRCDIR}/escapepod.scad_params.h \
	${MODELSRCDIR}/mantis.scad_params.h \
	${MODELSRCDIR}/missile.scad_params.h

DOCKING_PORT_FILES=${MODELSRCDIR}/starbase2.docking_ports.h \
	${MODELSRCDIR}/starbase3.docking_ports.h \
	${MODELSRCDIR}/starbase4.docking_ports.h \
	${MODELSRCDIR}/starbase5.docking_ports.h \
	${MODELSRCDIR}/starbase6.docking_ports.h \
	${MODELSRCDIR}/starbase.docking_ports.h

MANSRCDIR=./man
MANPAGES=${MANSRCDIR}/snis_client.6.gz ${MANSRCDIR}/snis_server.6.gz \
	${MANSRCDIR}/earthlike.1.gz \
	${MANSRCDIR}/snis_text_to_speech.sh.6 ${MANSRCDIR}/snis_test_audio.1.gz ssgl/ssgl_server.6 ${MANSRCDIR}/snis_multiverse.6
MANDIR=${DESTDIR}/${PREFIX}/share/man/man6

DESKTOPDIR=${DESTDIR}/${PREFIX}/share/applications
DESKTOPSRCDIR=.
DESKTOPFILES=${DESKTOPSRCDIR}/snis.desktop
UPDATE_DESKTOP=update-desktop-database ${DESKTOPDIR} || :

ifeq (${USE_CUSTOM_STRLCPY}, 1)
LBSD=
else
LBSD=-lbsd
endif

# -rdynamic is used by gcc for runtime stack traces (see stacktrace.c)
# but clang complains about it.
USING_CLANG=$(shell $(CC) --version | grep clang)
ifeq (${USING_CLANG},)
# not using clang
RDYNAMIC=-rdynamic
$(echo ${USING_CLANG})
else
# using clang
RDYNAMIC=
$(echo ${USING_CLANG})
endif

SNDLIBS:=$(shell $(PKG_CONFIG) --libs portaudio-2.0 vorbisfile)
SNDFLAGS:=-DWITHAUDIOSUPPORT $(shell $(PKG_CONFIG) --cflags portaudio-2.0) -DDATADIR=\"${DATADIR}\"
_OGGOBJ=ogg_to_pcm.o
_SNDOBJS=wwviaudio.o

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
ifeq ($(OSX), 1)
    PROFILEFLAG=-lprofiler
else
    PROFILEFLAG=-pg
endif

OPTIMIZEFLAG=-O3
DEBUGFLAG=
else
PROFILEFLAG=
endif

ifeq (${OSX},0)
# Arch pkg-config seems to be broken for lua5.2, so we have
# this "... || echo" hack thing.
#
LUALIBS:=$(shell $(PKG_CONFIG) --libs lua5.2 --silence-errors || $(PKG_CONFIG) --libs lua52 --silence-errors || $(PKG_CONFIG) --libs lua --silence-errors || echo '-llua5.2')
LUACFLAGS:=$(shell $(PKG_CONFIG) --cflags lua5.2 --silence-errors || $(PKG_CONFIG) --cflags lua52 --silence-errors || $(PKG_CONFIG) --cflags lua --silence-errors || echo '')
else
# OSX needs to do it this way (what is the point of pkgconfig if they all do it differently?)
LUALIBS:=$(shell $(PKG_CONFIG) --libs lua)
LUACFLAGS:=$(shell $(PKG_CONFIG) --cflags lua)
endif

PNGLIBS:=$(shell $(PKG_CONFIG) --libs libpng)
PNGCFLAGS:=$(shell $(PKG_CONFIG) --cflags libpng)

SDLLIBS:=$(shell $(PKG_CONFIG) sdl2 --libs)
SDLCFLAGS:=$(shell $(PKG_CONFIG) sdl2 --cflags)

GLEWLIBS:=$(shell $(PKG_CONFIG) --libs-only-l glew)
GLEWCFLAGS:=$(shell $(PKG_CONFIG) --cflags glew)

ifeq ($(OSX), 0)
	CRYPTLIBS:=-lcrypt
else
	CRYPTLIBS:=""
endif

_COMMONOBJS=mathutils.o snis_alloc.o snis_socket_io.o snis_marshal.o \
		bline.o shield_strength.o stacktrace.o snis_ship_type.o \
		snis_faction.o mtwist.o names.o infinite-taunt.o snis_damcon_systems.o \
		string-utils.o c-is-the-locale.o starbase_metadata.o arbitrary_spin.o \
		planetary_atmosphere.o planetary_ring_data.o mesh.o pthread_util.o \
		snis_opcode_def.o rts_unit_data.o commodities.o snis_tweak.o rootcheck.o \
		corporations.o replacement_assets.o snis_asset_dir.o snis_bin_dir.o scipher.o \
		planetary_properties.o open-simplex-noise.o
COMMONOBJS=$(patsubst %,$(OD)/%,${_COMMONOBJS}) mikktspace/mikktspace.o

_SERVEROBJS=snis_server.o starbase-comms.o \
		power-model.o quat.o vec4.o matrix.o snis_event_callback.o space-part.o fleet.o \
		docking_port.o elastic_collision.o snis_nl.o spelled_numbers.o \
		snis_server_tracker.o snis_bridge_update_packet.o solarsystem_config.o a_star.o \
		key_value_parser.o nonuniform_random_sampler.o oriented_bounding_box.o shape_collision.o \
		graph_dev_mesh_stub.o turret_aimer.o snis_hash.o snis_server_debug.o \
		ship_registration.o talking_stick.o transport_contract.o
SERVEROBJS=${COMMONOBJS} $(patsubst %,$(OD)/%,${_SERVEROBJS})

_MULTIVERSEOBJS=snis_multiverse.o snis_marshal.o snis_socket_io.o mathutils.o mtwist.o stacktrace.o \
		snis_hash.o quat.o string-utils.o key_value_parser.o snis_bridge_update_packet.o \
		pthread_util.o rootcheck.o starmap_adjacency.o replacement_assets.o snis_asset_dir.o \
		snis_bin_dir.o
MULTIVERSEOBJS=$(patsubst %,$(OD)/%,${_MULTIVERSEOBJS})

OGGOBJ=$(patsubst %,$(OD)/%,${_OGGOBJ})
SNDOBJS=$(patsubst %,$(OD)/%,${_SNDOBJS})

_COMMONCLIENTOBJS= snis_ui_element.o snis_font.o snis_text_input.o \
	snis_typeface.o snis_gauge.o snis_button.o snis_label.o snis_sliders.o snis_text_window.o \
	snis_strip_chart.o material.o stl_parser.o entity.o matrix.o my_point.o liang-barsky.o joystick.o \
	quat.o vec4.o thrust_attachment.o docking_port.o ui_colors.o snis_keyboard.o solarsystem_config.o \
	pronunciation.o snis_preferences.o snis_pull_down_menu.o snis_client_debug.o starmap_adjacency.o \
	shape_collision.o oriented_bounding_box.o xdg_base_dir_spec.o snis_voice_chat.o
COMMONCLIENTOBJS=${COMMONOBJS} ${OGGOBJ} ${SNDOBJS} $(patsubst %,$(OD)/%,${_COMMONCLIENTOBJS}) 

_CLIENTOBJS= shader.o graph_dev_opengl.o opengl_cap.o snis_graph.o snis_client.o joystick_config.o snis_xwindows_hacks.o
CLIENTOBJS=${COMMONCLIENTOBJS} $(patsubst %,$(OD)/%,${_CLIENTOBJS})

_LIMCLIENTOBJS=graph_dev_gdk.o snis_limited_graph.o snis_limited_client.o joystick_config.o
LIMCLIENTOBJS=${COMMONCLIENTOBJS} $(patsubst %,$(OD)/%,${_LIMCLIENTOBJS})

_SDLCLIENTOBJS=shader.o graph_dev_opengl.o opengl_cap.o snis_graph.o mesh_viewer.o \
				png_utils.o turret_aimer.o quat.o mathutils.o mesh.o \
				mtwist.o material.o entity.o snis_alloc.o matrix.o stacktrace.o stl_parser.o \
				snis_typeface.o snis_font.o string-utils.o ui_colors.o liang-barsky.o \
				bline.o vec4.o open-simplex-noise.o replacement_assets.o
SDLCLIENTOBJS=$(patsubst %,$(OD)/%,${_SDLCLIENTOBJS}) mikktspace/mikktspace.o

_NEBULANOISEOBJS=nebula_noise.o open-simplex-noise.o png_utils.o
NEBULANOISEOBJS=$(patsubst %,$(OD)/%,${_NEBULANOISEOBJS})
NEBULANOISELIBS=-lm ${PNGLIBS}

_GENERATE_SKYBOX_OBJS=generate_skybox.o open-simplex-noise.o png_utils.o mathutils.o quat.o mtwist.o
GENERATE_SKYBOX_OBJS=$(patsubst %,$(OD)/%,${_GENERATE_SKYBOX_OBJS})
GENERATE_SKYBOX_LIBS=-lm ${PNGLIBS}
X11LIBS=$(shell $(PKG_CONFIG) --libs x11)
X11CFLAGS=$(shell $(PKG_CONFIG) --cflags x11)

SSGL=ssgl/libssglclient.a
LIBS=-Lssgl -lssglclient -ldl -lm ${LBSD} ${PNGLIBS} ${GLEWLIBS}
SERVERLIBS=-Lssgl -lssglclient ${LRTLIB} -ldl -lm ${LBSD} ${LUALIBS} ${CRYPTLIBS}
MULTIVERSELIBS=-Lssgl -lssglclient ${LRTLIB} -ldl -lm ${LBSD} ${CRYPTLIBS}
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


BINPROGS=bin/ssgl_server bin/snis_server bin/snis_client bin/snis_text_to_speech.sh \
		bin/snis_multiverse bin/lsssgl bin/snis_arduino
UTILPROGS=util/mask_clouds util/cloud-mask-normalmap bin/mesh_viewer util/sample_image_colors \
		util/generate_solarsystem_positions bin/nebula_noise bin/generate_skybox bin/earthlike

# model directory
MD=${ASSETSSRCDIR}/models

MODELS=${MD}/freighter.stl \
	${MD}/laser.stl \
	${MD}/spaceship.stl \
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
	${MD}/starbase3.stl \
	${MD}/starbase4.stl \
	${MD}/starbase5.stl \
	${MD}/starbase6.stl \
	${MD}/spacemonster.stl \
	${MD}/asteroid-miner.stl \
	${MD}/spaceship2.stl \
	${MD}/spaceship3.stl \
	${MD}/dragonhawk.stl \
	${MD}/skorpio.stl \
	${MD}/long-triangular-prism.stl \
	${MD}/ship-icon.stl \
	${MD}/heading_indicator.stl \
	${MD}/heading_indicator_tail.stl \
	${MD}/axis.stl \
	${MD}/scrambler.stl \
	${MD}/swordfish.stl \
	${MD}/wombat.stl \
	${MD}/spaceship_turret.stl \
	${MD}/spaceship_turret_base.stl \
	${MD}/vanquisher.stl \
	${MD}/docking_port.stl \
	${MD}/docking_port2.stl \
	${MD}/warpgate.stl \
	${MD}/escapepod.stl \
	${MD}/mantis.stl \
	${MD}/laser_turret.stl \
	${MD}/laser_turret_base.stl \
	${MD}/uv_sphere.stl \
	${MD}/warp-core.stl \
	${MD}/space_monster_torso.stl \
	${MD}/space_monster_tentacle_segment.stl \
	${MD}/cylinder.stl \
	${MD}/missile.stl

MYCFLAGS=-DPREFIX=${PREFIX} ${DEBUGFLAG} ${PROFILEFLAG} ${OPTIMIZEFLAG}\
	--pedantic -Wall ${STOP_ON_WARN} -pthread -std=gnu99 ${RDYNAMIC} \
	-Wno-extended-offsetof -Wno-gnu-folding-constant $(CFLAGS) -Wvla \
	-DUSE_SNIS_XWINDOWS_HACKS=${USE_SNIS_XWINDOWS_HACKS} -fno-common \
	-DUSE_CUSTOM_STRLCPY=${USE_CUSTOM_STRLCPY}
VORBISFLAGS:=$(subst -I,-isystem ,$(shell $(PKG_CONFIG) --cflags vorbisfile))

ifeq (${WITHVOICECHAT},yes)
LIBOPUS=-L. -lopus
OPUSARCHIVE=libopus.a
VCHAT=-DWITHVOICECHAT=1
OPUSINCLUDE=-I./opus-1.3.1/include
else
LIBOPUS=
OPUSARCHIVE=
VCHAT=
OPUSINCLUDE=
endif

ifeq (${V},1)
Q=
ECHO=echo
else
Q=@
ECHO=echo
endif

COMPILE=$(ECHO) '  COMPILE' $< && $(CC) ${MYCFLAGS} ${LUACFLAGS} -c -o $@ $<
VORBISCOMPILE=$(ECHO) '  COMPILE' $< && $(CC) ${MYCFLAGS} ${VORBISFLAGS} ${SNDFLAGS} -c -o $@ $<
SDLCOMPILE=$(ECHO) '  COMPILE' $< && $(CC) ${MYCFLAGS} ${SDLCFLAGS} ${X11CFLAGS} -c -o $@ $<
SNISSERVERDBGCOMPILE=$(ECHO) '  COMPILE' $< && $(CC) -DSNIS_SERVER_DATA ${MYCFLAGS} ${LUACFLAGS} -c -o $(OD)/snis_server_debug.o $<
SNISCLIENTDBGCOMPILE=$(ECHO) '  COMPILE' $< && $(CC) -DSNIS_CLIENT_DATA ${MYCFLAGS} ${LUACFLAGS} -c -o $(OD)/snis_client_debug.o $<

CLIENTLINK=$(ECHO) '  LINK' $@ && $(CC) ${MYCFLAGS} ${SNDFLAGS} -o $@ ${X11LIBS} ${SDLCFLAGS} ${CLIENTOBJS} ${SDLLIBS} ${LIBS} ${SNDLIBS} $(LDFLAGS) ${LIBOPUS} ${X11LIBS}
LIMCLIENTLINK=$(ECHO) '  LINK' $@ && $(CC) ${MYCFLAGS} ${SNDFLAGS} -o $@ ${LIMCLIENTOBJS} ${SDLLIBS} ${LIBS} ${SNDLIBS} $(LDFLAGS) ${LIBOPUS}
SDLCLIENTLINK=$(ECHO) '  LINK' $@ && $(CC) ${MYCFLAGS} ${SNDFLAGS} -o $@ ${SDLCFLAGS} ${SDLCLIENTOBJS} ${SDLLIBS} ${LIBS} ${SNDLIBS} $(LDFLAGS) ${X11LIBS}
SERVERLINK=$(ECHO) '  LINK' $@ && $(CC) ${MYCFLAGS} -o $@ ${SERVEROBJS} ${SERVERLIBS} $(LDFLAGS)
MULTIVERSELINK=$(ECHO) '  LINK' $@ && $(CC) ${MYCFLAGS} -o $@ ${MULTIVERSEOBJS} ${MULTIVERSELIBS} $(LDFLAGS)
NEBULANOISELINK=$(ECHO) '  LINK' $@ && $(CC) ${MYCFLAGS} -o $@ ${NEBULANOISEOBJS} ${NEBULANOISELIBS} $(LDFLAGS)
GENERATE_SKYBOX_LINK=$(ECHO) '  LINK' $@ && $(CC) ${MYCFLAGS} -o $@ ${GENERATE_SKYBOX_OBJS} ${GENERATE_SKYBOX_LIBS} $(LDFLAGS)
OPENSCAD=$(ECHO) '  OPENSCAD' $< && openscad -o $@ $<
EXTRACTSCADPARAMS=$(ECHO) '  EXTRACT THRUST PARAMS' $@ && $(AWK) -f extract_scad_params.awk $< > $@
EXTRACTDOCKINGPORTS=$(ECHO) '  EXTRACT DOCKING PORTS' $@ && $(AWK) -f extract_docking_ports.awk $< > $@

_ELOBJS=mtwist.o mathutils.o quat.o open-simplex-noise.o png_utils.o crater.o pthread_util.o
ELOBJS=$(patsubst %,$(OD)/%, ${_ELOBJS})
ELLIBS=-lm ${LRTLIB} -lpng
ELLINK=$(ECHO) '  LINK' $@ && $(CC) ${MYCFLAGS} -o $@ ${OD}/earthlike.o ${ELOBJS} ${ELLIBS} $(LDFLAGS)
MCLIBS=-lm ${LRTLIB} -lpng
_MCOBJS=png_utils.o
MCOBJS=$(patsubst %,$(OD)/%, ${_MCOBJS})
MCLINK=$(ECHO) '  LINK' $@ && $(CC) ${MYCFLAGS} -o $@ util/mask_clouds.o ${MCOBJS} ${MCLIBS} $(LDFLAGS)

CMNMLIBS=-lm ${LRTLIB} -lpng
_CMNMOBJS=png_utils.o
CMNMOBJS=$(patsubst %,$(OD)/%, ${_CMNMOBJS})
CMNMLINK=$(ECHO) '  LINK' $@ && $(CC) ${MYCFLAGS} -o $@ util/cloud-mask-normalmap.o ${CMNMOBJS} ${CMNMLIBS} $(LDFLAGS)

all:	bin/.t ${COMMONOBJS} ${SERVEROBJS} ${MULTIVERSEOBJS} ${CLIENTOBJS} ${BINPROGS} ${SCAD_PARAMS_FILES} ${DOCKING_PORT_FILES}

models:	${MODELS}

# we need to depend on ${OD}/.t not on ${OD} because OD is a directory and the
# timestamp changes whenever the directory contents change, and we need to depend
# only on the *existence* of the directory, not on the contents. Same for ${BIN}.
ODT=${OD}/.t

${ODT}:
	@mkdir -p ${OD}
	@if [ ! -f ${ODT} ] ; then \
		touch ${ODT} ; \
	fi

BIN=bin/.t

${BIN}:
	@mkdir -p bin
	@if [ ! -f ${BIN} ] ; then \
		touch ${BIN}  ; \
	fi

# Rule to prevent common error of trying to "make foo" instead of "make bin/foo"
BINARY_NAMES=snis_client snis_server snis_limited_client snis_multiverse nebula_noise \
	generate_skybox ssgl_server lsssgl snis_text_to_speech.sh mesh_viewer earthlike \
	infinite-taunt names stl_parser test_key_value_parser test-matrix test-space-partition \
	test-marshal test-quat test-fleet test-mtwist device-io-sample-1 test-nonuniform-random-sampler \
	test-commodities test-obj-parser test_solarsystem_config test_crater print_ship_attributes \
	test_snis_dmx snis_test_audio check-endianness
${BINARY_NAMES}:
	@echo "You probably meant: make bin/$@" 1>&2
	@/bin/false

update-assets:
	@util/snis_update_assets.sh

check-assets:
	@util/snis_update_assets.sh --dry-run

install-assets:
	@util/snis_update_assets.sh --localcopy --destdir ${DESTDIR}/${PREFIX}

build:	all

utils:	${UTILPROGS}

$(OD)/rootcheck.o:	rootcheck.c rootcheck.h Makefile ${ODT}
	$(Q)$(COMPILE)

$(OD)/graph_dev_opengl.o : graph_dev_opengl.c Makefile ${ODT}
	$(Q)$(SDLCOMPILE)

$(OD)/opengl_cap.o : opengl_cap.c Makefile ${ODT}
	$(Q)$(SDLCOMPILE)

$(OD)/graph_dev_gdk.o : graph_dev_gdk.c Makefile ${ODT}
	$(Q)$(SDLCOMPILE)

$(OD)/graph_dev_mesh_stub.o:	graph_dev_mesh_stub.c graph_dev_mesh_stub.h ${ODT}
	$(Q)$(COMPILE)

$(OD)/material.o : material.c Makefile ${ODT}
	$(Q)$(SDLCOMPILE)

$(OD)/shader.o : shader.c Makefile ${ODT}
	$(Q)$(COMPILE)

%.stl:	%.scad
	$(Q)$(OPENSCAD)

%.scad_params.h: %.scad
	$(Q)$(EXTRACTSCADPARAMS)

%.docking_ports.h: %.scad
	$(Q)$(EXTRACTDOCKINGPORTS)

$(OD)/thrust_attachment.o:	thrust_attachment.c thrust_attachment.h Makefile ${ODT}
	$(Q)$(COMPILE)

$(OD)/docking_port.o:	docking_port.c docking_port.h Makefile ${ODT}
	$(Q)$(COMPILE)

$(OD)/ui_colors.o:	ui_colors.c ui_colors.h snis_graph.h Makefile ${ODT}
	$(Q)$(COMPILE)

$(OD)/snis_keyboard.o:	snis_keyboard.c snis_keyboard.h ${OD}/string-utils.o Makefile ${ODT}
	$(Q)$(SDLCOMPILE)

$(OD)/snis_preferences.o:	snis_preferences.c snis_preferences.h string-utils.h snis_packet.h ${ODT}
	$(Q)$(COMPILE)

$(OD)/solarsystem_config.o:	solarsystem_config.c solarsystem_config.h string-utils.h Makefile ${ODT}
	$(Q)$(COMPILE)

solarsystem_config_test: solarsystem_config.c ${OD}/string-utils.o ${ODT}
	$(CC) ${MYCFLAGS} -DSOLARSYSTEM_CONFIG_TEST=1 -o $@ solarsystem_config.c ${OD}/string-utils.o

$(OD)/my_point.o:   my_point.c Makefile ${ODT}
	$(Q)$(COMPILE)

$(OD)/mesh.o:   mesh.c mikktspace/mikktspace.h open-simplex-noise.h Makefile ${ODT}
	$(Q)$(COMPILE)

$(OD)/pthread_util.o:	pthread_util.c pthread_util.h ${ODT}
	$(Q)$(COMPILE)

$(OD)/power-model.o:   power-model.c Makefile ${ODT}
	$(Q)$(COMPILE)

$(OD)/stacktrace.o:   stacktrace.c Makefile ${ODT}
	$(Q)$(COMPILE)

$(OD)/snis_ship_type.o:   snis_ship_type.c snis_ship_type.h corporations.h Makefile ${ODT}
	$(Q)$(COMPILE)

bin/test_snis_ship_type: snis_ship_type.c snis_ship_type.h ${OD}/string-utils.o ${OD}/corporations.o ${OD}/rts_unit_data.o ${BIN}
	$(CC) ${MYCFLAGS} -DTEST_SNIS_SHIP_TYPE -o bin/test_snis_ship_type snis_ship_type.c ${OD}/string-utils.o ${OD}/corporations.o ${OD}/rts_unit_data.o ${LBSD}

$(OD)/snis_faction.o:   snis_faction.c string-utils.h Makefile ${ODT}
	$(Q)$(COMPILE)

$(OD)/liang-barsky.o:   liang-barsky.c Makefile ${ODT}
	$(Q)$(COMPILE)

$(OD)/joystick.o:   joystick.c Makefile ${ODT}
	$(Q)$(COMPILE)

joystick_test:	joystick.c joystick.h Makefile
	$(CC) -g -DJOYSTICK_TEST -o joystick_test joystick.c

$(OD)/joystick_config.o:	joystick_config.c joystick_config.h string-utils.h ${ODT}
	$(Q)$(COMPILE)

$(OD)/ogg_to_pcm.o:   ogg_to_pcm.c Makefile ${ODT}
	$(Q)$(VORBISCOMPILE)

$(OD)/bline.o:	bline.c Makefile ${ODT}
	$(Q)$(COMPILE)

$(OD)/wwviaudio.o:    wwviaudio.c Makefile ${ODT}
	$(Q)$(VORBISCOMPILE)

wwviaudio_basic_test:	wwviaudio.c wwviaudio.h ${OGGOBJ}
	$(CC) ${MYCFLAGS} -DWWVIAUDIO_BASIC_TEST -DDATADIR=\".\" -o wwviaudio_basic_test wwviaudio.c ${OGGOBJ} ${SNDLIBS}

wwviaudio_recording_test:	wwviaudio.c wwviaudio.h ${OGGOBJ}
	$(CC) ${MYCFLAGS} -DWWVIAUDIO_RECORDING_TEST -DDATADIR=\".\" -o wwviaudio_recording_test wwviaudio.c ${OGGOBJ} ${SNDLIBS}

$(OD)/shield_strength.o:	shield_strength.c Makefile ${ODT}
	$(Q)$(COMPILE)

$(OD)/planetary_properties.o:	planetary_properties.c planetary_properties.h Makefile ${ODT}
	$(Q)$(COMPILE)

$(OD)/snis_server.o:	snis_server.c Makefile build_info.h ${DOCKING_PORT_FILES} ${ODT}
	$(Q)$(COMPILE)

$(OD)/snis_multiverse.o:	snis_multiverse.c snis_multiverse.h Makefile build_info.h ${ODT} \
			snis_entity_key_value_specification.h
	$(Q)$(COMPILE)

$(OD)/snis_server_tracker.o:	snis_server_tracker.c snis_server_tracker.h pthread_util.h ${ODT} \
			ssgl/ssgl.h Makefile
	$(Q)$(COMPILE)

$(OD)/snis_client.o:	snis_client.c Makefile build_info.h ui_colors.h ${ODT}
	$(Q)$(SDLCOMPILE) ${VCHAT}

$(OD)/mesh_viewer.o:	mesh_viewer.c Makefile build_info.h ${ODT}
	$(Q)$(SDLCOMPILE)

# simplexnoise1234.o:	simplexnoise1234.c Makefile build_info.h
#	$(Q)$(COMPILE)

$(OD)/open-simplex-noise.o:	open-simplex-noise.c Makefile build_info.h ${ODT}
	$(Q)$(COMPILE)

$(OD)/nebula_noise.o:	nebula_noise.c png_utils.h open-simplex-noise.h Makefile ${ODT}
	$(Q)$(COMPILE)

bin/nebula_noise:	$(NEBULANOISEOBJS) ${BIN}
	$(Q)$(NEBULANOISELINK)

$(OD)/generate_skybox.o:	generate_skybox.c png_utils.h open-simplex-noise.h quat.h mtwist.h mathutils.h Makefile ${ODT}
	$(Q)$(COMPILE)

bin/generate_skybox:	$(GENERATE_SKYBOX_OBJS) ${BIN}
	$(Q)$(GENERATE_SKYBOX_LINK)

$(OD)/earthlike.o:	earthlike.c ${ODT}
	$(Q)$(COMPILE)

/util/mask_clouds.o:	util/mask_clouds.c
	$(Q)$(COMPILE)

/util/cloud-mask-normalmap.o:	util/cloud-mask-normalmap.c
	$(Q)$(COMPILE)

$(OD)/util/sample_image_colors.o:	util/sample_image_colors.c ${OD}/png_utils.o ${ODT}
	$(Q)$(COMPILE)

util/sample_image_colors:	util/sample_image_colors.o ${OD}/png_utils.o
	$(CC) ${MYCFLAGS} -o $@ util/sample_image_colors.o ${OD}/png_utils.o ${PNGLIBS}

$(OD)/util/generate_solarsystem_positions.o:	util/generate_solarsystem_positions.c ${OD}/string-utils.o ${ODT}
	$(Q)$(COMPILE)

util/generate_solarsystem_positions:	util/generate_solarsystem_positions.o ${OD}/string-utils.o
	$(CC) ${MYCFLAGS} -o $@ util/generate_solarsystem_positions.o ${OD}/string-utils.o -lm ${LBSD}

$(OD)/snis_socket_io.o:	snis_socket_io.c Makefile ${ODT}
	$(Q)$(COMPILE)

$(OD)/snis_marshal.o:	snis_marshal.c Makefile ${ODT}
	$(Q)$(COMPILE)

$(OD)/snis_opcode_def.o:	snis_opcode_def.c snis_opcode_def.h snis_packet.h Makefile ${ODT}
	$(Q)$(COMPILE)

$(OD)/rts_unit_data.o:	rts_unit_data.c rts_unit_data.h Makefile ${ODT}
	$(Q)$(COMPILE)

$(OD)/snis_bridge_update_packet.o:	snis_bridge_update_packet.c snis_bridge_update_packet.h Makefile ${ODT}
	$(Q)$(COMPILE)

$(OD)/snis_font.o:	snis_font.c Makefile ${ODT}
	$(Q)$(COMPILE)

$(OD)/mathutils.o:	mathutils.c Makefile ${ODT}
	$(Q)$(COMPILE)

$(OD)/crater.o:	crater.c crater.h Makefile ${ODT}
	$(Q)$(COMPILE)

$(OD)/test_crater.o:	test_crater.c ${ODT}
	$(Q)$(COMPILE)

$(OD)/snis_alloc.o:	snis_alloc.c Makefile ${ODT}
	$(Q)$(COMPILE)

$(OD)/snis_damcon_systems.o:	snis_damcon_systems.c Makefile ${ODT}
	$(Q)$(COMPILE)

bin/snis_server:	${SERVEROBJS} ${SSGL} Makefile ${BIN}
	$(Q)$(SERVERLINK)

bin/snis_client:	${CLIENTOBJS} ${SSGL} Makefile ${BIN} ${OPUSARCHIVE}
	$(Q)$(CLIENTLINK)

bin/snis_multiverse:	${MULTIVERSEOBJS} ${SSGL} Makefile ${BIN}
	$(Q)$(MULTIVERSELINK)

bin/snis_limited_client:	${LIMCLIENTOBJS} ${SSGL} Makefile ${BIN}
	$(Q)$(LIMCLIENTLINK)

ssgl/lsssgl: ${SSGL}

ssgl/ssgl_server: ${SSGL}

bin/ssgl_server: ${SSGL} ${BIN}
	@cp ssgl/ssgl_server bin/ssgl_server 

bin/lsssgl:	${SSGL} ${BIN}
	@cp ssgl/lsssgl bin/lsssgl 

bin/snis_text_to_speech.sh:	snis_text_to_speech.sh ${BIN}
	@cp snis_text_to_speech.sh bin/snis_text_to_speech.sh
	@chmod +x bin/snis_text_to_speech.sh

bin/mesh_viewer:	${SDLCLIENTOBJS} ${SSGL} Makefile ${BIN}
	$(Q)$(SDLCLIENTLINK)

bin/earthlike:	${OD}/earthlike.o ${ELOBJS} Makefile ${BIN}
	$(Q)$(ELLINK)

util/mask_clouds:	util/mask_clouds.o ${MCOBJS} Makefile
	$(Q)$(MCLINK)

util/cloud-mask-normalmap:	util/cloud-mask-normalmap.o ${CMNMOBJS} Makefile
	$(Q)$(CMNMLINK)

$(OD)/starbase-comms.o:	starbase-comms.c Makefile ${ODT}
	$(Q)$(COMPILE)

$(OD)/starbase_metadata.o:	starbase_metadata.c starbase_metadata.h Makefile ${ODT}
	$(Q)$(COMPILE)

${OD}/infinite-taunt.o:	infinite-taunt.c Makefile ${ODT}
	$(Q)$(COMPILE)

bin/infinite-taunt:	${OD}/infinite-taunt.o ${OD}/names.o ${OD}/mtwist.o Makefile ${BIN}
	$(CC) -DTEST_TAUNT -o bin/infinite-taunt ${MYCFLAGS} ${OD}/mtwist.o infinite-taunt.c ${OD}/names.o ${LBSD}

bin/names:	names.c names.h ${OD}/mtwist.o ${BIN}
	$(CC) -DTEST_NAMES -o bin/names ${MYCFLAGS} ${OD}/mtwist.o names.c

$(OD)/snis_graph.o:	snis_graph.c Makefile ${ODT}
	$(Q)$(SDLCOMPILE)

$(OD)/ship_registration.o:	ship_registration.c ship_registration.h Makefile ${ODT}
	$(Q)$(COMPILE)

$(OD)/corporations.o:	corporations.c corporations.h Makefile ${ODT}
	$(Q)$(COMPILE)

$(OD)/snis_typeface.o:	snis_typeface.c Makefile ${ODT}
	$(Q)$(SDLCOMPILE)

$(OD)/snis_gauge.o:	snis_gauge.c Makefile ${ODT}
	$(Q)$(SDLCOMPILE)

$(OD)/snis_button.o:	snis_button.c Makefile ${ODT}
	$(Q)$(SDLCOMPILE)

$(OD)/snis_pull_down_menu.o:	snis_pull_down_menu.c snis_pull_down_menu.h Makefile ${ODT}
	$(Q)$(SDLCOMPILE)

$(OD)/snis_strip_chart.o:	snis_strip_chart.c Makefile ${ODT}
	$(Q)$(SDLCOMPILE)

$(OD)/snis_label.o:	snis_label.c Makefile ${ODT}
	$(Q)$(SDLCOMPILE)

$(OD)/snis_sliders.o:	snis_sliders.c snis_sliders.h ui_colors.h Makefile ${ODT}
	$(Q)$(SDLCOMPILE)

$(OD)/snis_text_window.o:	snis_text_window.c Makefile ${ODT}
	$(Q)$(SDLCOMPILE)

$(OD)/snis_ui_element.o:	snis_ui_element.c Makefile ${ODT}
	$(Q)$(SDLCOMPILE)

$(OD)/snis_text_input.o:	snis_text_input.c Makefile ${ODT}
	$(Q)$(SDLCOMPILE)

$(OD)/matrix.o:	matrix.c Makefile ${ODT}
	$(Q)$(COMPILE)

$(OD)/starmap_adjacency.o:	starmap_adjacency.c starmap_adjacency.h $(OD)/quat.o $(OD)/vec4.o ${ODT}
	$(Q)$(COMPILE)

$(OD)/snis_voice_chat.o:	snis_voice_chat.c snis_voice_chat.h pthread_util.h wwviaudio.h ${OPUSARCHIVE}
	$(Q)$(COMPILE) ${VCHAT} ${OPUSINCLUDE}

$(OD)/replacement_assets.o:	replacement_assets.c replacement_assets.h ${ODT}
	$(Q)$(COMPILE)

$(OD)/snis_asset_dir.o:	snis_asset_dir.c snis_asset_dir.h ${ODT}
	$(Q)$(COMPILE)

$(OD)/snis_bin_dir.o:	snis_bin_dir.c snis_bin_dir.h ${ODT}
	$(Q)$(COMPILE)

$(OD)/stl_parser.o:	stl_parser.c Makefile ${ODT}
	$(Q)$(COMPILE)

bin/stl_parser:	stl_parser.c $(OD)/matrix.o $(OD)/mesh.o $(OD)/mathutils.o $(OD)/quat.o $(OD)/mtwist.o \
		$(OD)/open-simplex-noise.o mikktspace/mikktspace.o \
		$(OD)/string-utils.o Makefile ${BIN}
	$(CC) -DTEST_STL_PARSER ${MYCFLAGS} -o bin/stl_parser stl_parser.c ${OD}/matrix.o ${OD}/mesh.o ${OD}/mathutils.o \
		${OD}/quat.o ${OD}/mtwist.o mikktspace/mikktspace.o ${OD}/string-utils.o ${OD}/open-simplex-noise.o -lm $(LDFLAGS)

$(OD)/entity.o:	entity.c Makefile ${ODT}
	$(Q)$(SDLCOMPILE)

$(OD)/names.o:	names.c Makefile ${ODT}
	$(Q)$(COMPILE)

$(OD)/space-part.o:	space-part.c Makefile ${ODT}
	$(Q)$(COMPILE)

$(OD)/quat.o:	quat.c Makefile ${ODT}
	$(Q)$(COMPILE)

$(OD)/oriented_bounding_box.o:	oriented_bounding_box.c oriented_bounding_box.h ${ODT}
	$(Q)$(COMPILE)

$(OD)/shape_collision.o:	shape_collision.c shape_collision.h ${ODT}
	$(Q)$(COMPILE)

$(OD)/turret_aimer.o:	turret_aimer.c turret_aimer.h quat.h mathutils.h ${ODT}
	$(Q)$(COMPILE)

$(OD)/vec4.o:	vec4.c Makefile ${ODT}
	$(Q)$(COMPILE)

$(OD)/arbitrary_spin.o:	arbitrary_spin.c arbitrary_spin.h Makefile ${ODT}
	$(Q)$(COMPILE)

$(OD)/a_star.o:	a_star.c a_star.h Makefile ${ODT}
	$(Q)$(COMPILE)

$(OD)/a_star_test.o:	a_star_test.c a_star.h Makefile ${ODT}
	$(Q)$(COMPILE)

a_star_test:	a_star_test.o a_star.o Makefile
	$(CC) -g -o a_star_test a_star_test.c a_star.o -lm

$(OD)/mtwist.o:	mtwist.c Makefile ${ODT}
	$(Q)$(COMPILE)

$(OD)/elastic_collision.o:	elastic_collision.c elastic_collision.h Makefile ${ODT}
	$(Q)$(COMPILE)

$(OD)/talking_stick.o:	talking_stick.c talking_stick.h Makefile ${ODT}
	$(Q)$(COMPILE)

$(OD)/transport_contract.o:	transport_contract.c transport_contract.h Makefile ${ODT}
	$(Q)$(COMPILE)

bin/test_transport_contract:	transport_contract.c transport_contract.h ${OD}/commodities.o \
				${OD}/names.o ${OD}/mtwist.o ${OD}/string-utils.o ${OD}/infinite-taunt.o \
				${ODT} ${BIN}
	$(CC) -g -DTEST_TRANSPORT_CONTRACT=1 -o bin/test_transport_contract transport_contract.c \
			${OD}/commodities.o ${OD}/names.o ${OD}/mtwist.o ${OD}/string-utils.o ${OD}/infinite-taunt.o ${LBSD}

$(OD)/fleet.o:	fleet.c Makefile ${ODT}
	$(Q)$(COMPILE)

$(OD)/png_utils.o:	png_utils.c png_utils.h Makefile ${ODT}
	$(Q)$(COMPILE)

$(OD)/c-is-the-locale.o:	c-is-the-locale.c ${ODT}
	$(Q)$(COMPILE)

$(OD)/commodities.o:	commodities.c Makefile ${ODT}
	$(Q)$(COMPILE)

$(OD)/string-utils.o:	string-utils.c Makefile ${ODT}
	$(Q)$(COMPILE)

$(OD)/xdg_base_dir_spec.o:	xdg_base_dir_spec.c xdg_base_dir_spec.h Makefile ${ODT}
	$(Q)$(COMPILE)

$(OD)/snis_tweak.o: snis_tweak.c snis_tweak.h Makefile ${ODT}
	$(Q)$(COMPILE)

$(OD)/snis_client_debug.o: snis_debug.c snis_debug.h Makefile ${ODT}
	$(Q)$(SNISCLIENTDBGCOMPILE)

$(OD)/snis_xwindows_hacks.o:	snis_xwindows_hacks.c snis_xwindows_hacks.h Makefile ${ODT}
	$(Q)$(COMPILE)

$(OD)/snis_server_debug.o: snis_debug.c snis_debug.h Makefile ${ODT}
	$(Q)$(SNISSERVERDBGCOMPILE)

$(OD)/pronunciation.o:	pronunciation.c Makefile ${ODT}
	$(Q)$(COMPILE)

test_xdg_base_dir:	xdg_base_dir_spec.c xdg_base_dir_spec.h Makefile
	$(CC) -DTEST_XDG_BASE_DIR -o test_xdg_base_dir xdg_base_dir_spec.c

test_pronunciation:	pronunciation.c Makefile
	$(CC) -DTEST_PRONUNCIATION_FIXUP -o test_pronunciation pronunciation.c

$(OD)/planetary_atmosphere.o:	planetary_atmosphere.c Makefile ${ODT}
	$(Q)$(COMPILE)

$(OD)/planetary_ring_data.o:	planetary_ring_data.c planetary_ring_data.h mtwist.h Makefile ${ODT}
	$(Q)$(COMPILE)

test_planetary_atmosphere:	planetary_atmosphere.c mtwist.o Makefile
	$(CC) -g -DTEST_PLANETARY_ATMOSPHERE_PROFILE -o test_planetary_atmosphere planetary_atmosphere.c mtwist.o

$(OD)/key_value_parser.o:	key_value_parser.c key_value_parser.h Makefile ${ODT}
	$(Q)$(COMPILE)

bin/test_key_value_parser:	key_value_parser.c key_value_parser.h Makefile ${BIN}
	$(CC) ${MYCFLAGS} -DTEST_KEY_VALUE_PARSER -o bin/test_key_value_parser key_value_parser.c
	bin/test_key_value_parser

bin/test-matrix:	matrix.c Makefile ${BIN}
	$(CC) ${MYCFLAGS} -DTEST_MATRIX -o bin/test-matrix matrix.c -lm

bin/test-space-partition:	space-part.c Makefile ${BIN}
	$(CC) ${MYCFLAGS} -g -DTEST_SPACE_PARTITION -o bin/test-space-partition space-part.c -lm

$(OD)/snis_event_callback.o:	snis_event_callback.c Makefile ${ODT}
	$(Q)$(COMPILE)

$(OD)/snis_hash.o:	snis_hash.c snis_hash.h Makefile ${ODT}
	$(Q)$(COMPILE)

test_snis_crypt:	snis_hash.c snis_hash.h
	$(CC) -DTEST_SNIS_CRYPT -o test_snis_crypt snis_hash.c ${CRYPTLIBS}

test_marshal:	snis_marshal.c snis_marshal.h stacktrace.o
	$(CC) -DTEST_MARSHAL -o test_marshal stacktrace.o snis_marshal.c

$(OD)/snis_nl.o:	snis_nl.c snis_nl.h Makefile ${ODT}
	$(Q)$(COMPILE)

snis_nl:	snis_nl.o ${OD}/string-utils.o ${OD}/spelled_numbers.o
	$(CC) -g -DTEST_NL -o snis_nl ${OD}/string-utils.o ${OD}/spelled_numbers.o snis_nl.c

$(OD)/spelled_numbers.o:	spelled_numbers.c spelled_numbers.h Makefile ${ODT}
	$(Q)$(COMPILE)

$(OD)/scipher.o:	scipher.c scipher.h mathutils.h
	$(Q)$(COMPILE)

test_scipher:	scipher.c scipher.h mathutils.o mtwist.o
	$(CC) -g -fsanitize=address -DTEST_SCIPHER -o test_scipher scipher.c mathutils.o mtwist.o -lm

spelled_numbers:	spelled_numbers.c
	$(CC) -g -DSPELLED_NUMBERS_TEST_CASE -o spelled_numbers spelled_numbers.c -lm

${SSGL}:
	(cd ssgl && ${MAKE} USE_CUSTOM_STRLCPY=${USE_CUSTOM_STRLCPY} )

mikktspace/mikktspace.o:
	(cd mikktspace && ${MAKE} )

mostly-clean:
	rm -f ${SERVEROBJS} ${CLIENTOBJS} ${LIMCLIENTOBJS} ${MULTIVERSEOBJS} ${SDLCLIENTOBJS} ${SSGL} \
	${BINPROGS} ${UTILPROGS} ${ELOBJS} stl_parser snis_limited_client.c \
	test-space-partition snis_test_audio.o snis_test_audio joystick_test local_termios2.h \
	bin/nebula_noise bin/generate_skybox bin/names bin/infinite-taunt bin/stl_parser \
	bin/test-obj-parser bin/test-commodities bin/test_nonuniform_random_sampler bin/test-marshal \
	bin/test-quat bin/test-fleet bin/test-mtwist bin/snis-device-io-sample-1 bin/check-endianness \
	${OD}/*.o ${ODT} bin/test-matrix  bin/test_solarsystem_config  bin/test-space-partition \
	bin/device-io-sample-1 bin/print_ship_attributes bin/snis_test_audio bin/test_crater \
	bin/test_key_value_parser bin/test_snis_dmx test_scipher bin/test_snis_ship_type wwviaudio_basic_test \
	${MANSRCDIR}/earthlike.1.gz  ${MANSRCDIR}/snis_client.6.gz  ${MANSRCDIR}/snis_server.6.gz  \
	${MANSRCDIR}/snis_test_audio.1.gz bin/test_transport_contract
	rm -f ${BIN}
	rm -fr opus-1.3.1
	rm -f libopus.a
	if [ -x bin ]; then rmdir bin; fi
	rm -fr ${OD}
	( cd ssgl && ${MAKE} clean )
	( cd mikktspace && ${MAKE} clean )

bin/test-marshal:	snis_marshal.c ${OD}/stacktrace.o Makefile ${BIN}
	$(CC) -DTEST_MARSHAL -o bin/test-marshal snis_marshal.c ${OD}/stacktrace.o

bin/test-quat:	test-quat.c ${OD}/quat.o ${OD}/matrix.o ${OD}/mathutils.o ${OD}/mtwist.o Makefile ${BIN}
	$(CC) -Wall -Wextra --pedantic -o test-quat test-quat.c ${OD}/quat.o ${OD}/matrix.o ${OD}/mathutils.o ${OD}/mtwist.o -lm

bin/test-fleet: ${OD}/quat.o ${OD}/fleet.o ${OD}/mathutils.o ${OD}/mtwist.o Makefile ${BIN}
	$(CC) -DTESTFLEET=1 -c -o ${OD}/test-fleet.o fleet.c
	$(CC) -DTESTFLEET=1 -o bin/test-fleet ${OD}/test-fleet.o ${OD}/mathutils.o ${OD}/quat.o ${OD}/mtwist.o -lm

bin/test-mtwist: ${OD}/mtwist.o test-mtwist.c Makefile ${BIN}
	$(CC) -o bin/test-mtwist ${OD}/mtwist.o test-mtwist.c

$(OD)/snis-device-io.o:	snis-device-io.h snis-device-io.c Makefile ${ODT}
	$(CC) -Wall -Wextra --pedantic -pthread -c -o ${OD}/snis-device-io.o snis-device-io.c

bin/device-io-sample-1:	device-io-sample-1.c ${OD}/snis-device-io.o ${BIN}
	$(CC) -Wall -Wextra --pedantic -pthread -o bin/device-io-sample-1 ${OD}/snis-device-io.o \
			device-io-sample-1.c ${LBSD}

bin/snis_arduino: snis_arduino.c ${OD}/snis-device-io.o ${BIN}
	$(CC) -Wall -Wextra --pedantic -pthread -o bin/snis_arduino ${OD}/snis-device-io.o \
			snis_arduino.c ${LBSD}

$(OD)/nonuniform_random_sampler.o:	nonuniform_random_sampler.c nonuniform_random_sampler.h ${ODT}
	$(Q)$(COMPILE)

bin/test_nonuniform_random_sampler:	nonuniform_random_sampler.c ${OD}/mathutils.o ${OD}/mtwist.o ${BIN}
	$(CC) -D TEST_NONUNIFORM_SAMPLER -o bin/test_nonuniform_random_sampler ${OD}/mtwist.o ${OD}/mathutils.o -lm nonuniform_random_sampler.c

bin/test-commodities:	${OD}/commodities.o Makefile ${OD}/string-utils.o ${BIN}
	$(CC) -DTESTCOMMODITIES=1 -O3 -c commodities.c -o ${OD}/test-commodities.o
	$(CC) -DTESTCOMMODITIES=1 -o bin/test-commodities ${OD}/string-utils.o ${OD}/test-commodities.o ${LBSD}

bin/test-obj-parser:	test-obj-parser.c mikktspace/mikktspace.o ${OD}/string-utils.o ${OD}/stl_parser.o ${OD}/mesh.o \
		${OD}/mtwist.o ${OD}/mathutils.o ${OD}/matrix.o ${OD}/quat.o ${OD}/open-simplex-noise.o ${OD}/stacktrace.o Makefile ${BIN}
	$(CC) -o bin/test-obj-parser mikktspace/mikktspace.o ${OD}/string-utils.o ${OD}/stl_parser.o ${OD}/mtwist.o \
		${OD}/mathutils.o ${OD}/matrix.o ${OD}/mesh.o ${OD}/quat.o ${OD}/open-simplex-noise.o ${OD}/stacktrace.o \
		-lm test-obj-parser.c ${LBSD}

test:	bin/test-matrix bin/test-space-partition bin/test-marshal bin/test-quat bin/test-fleet bin/test-mtwist bin/test-commodities bin/test_solarsystem_config
	/bin/true	# Prevent make from running "$(CC) test.o".

bin/test_solarsystem_config:	test_solarsystem_config.c ${OD}/solarsystem_config.o ${OD}/string-utils.o ${BIN}
	$(CC) -o $@ $< ${OD}/solarsystem_config.o ${OD}/string-utils.o

bin/test_crater:	$(OD)/test_crater.o $(OD)/crater.o $(OD)/mathutils.o $(OD)/mtwist.o ${OD}/png_utils.o ${BIN}
	$(CC) -o $@ ${PNGCFLAGS} $(OD)/test_crater.o $(OD)/crater.o $(OD)/mtwist.o ${OD}/png_utils.o ${PNGLIBS} $(OD)/mathutils.o -lm

${MANSRCDIR}/snis_client.6.gz:	${MANSRCDIR}/snis_client.6
	gzip -9 - < ${MANSRCDIR}/snis_client.6 > ${MANSRCDIR}/snis_client.6.gz

${MANSRCDIR}/snis_server.6.gz:	${MANSRCDIR}/snis_server.6
	gzip -9 - < ${MANSRCDIR}/snis_server.6 > ${MANSRCDIR}/snis_server.6.gz

${MANSRCDIR}/earthlike.1.gz:	${MANSRCDIR}/earthlike.1
	gzip -9 - < ${MANSRCDIR}/earthlike.1 > ${MANSRCDIR}/earthlike.1.gz

${MANSRCDIR}/snis_test_audio.1.gz:	${MANSRCDIR}/snis_test_audio.1
	gzip -9 - < ${MANSRCDIR}/snis_test_audio.1 > ${MANSRCDIR}/snis_test_audio.1.gz

bin/print_ship_attributes:	snis_entity_key_value_specification.h $(OD)/key_value_parser.o ${BIN}
	$(CC) -o bin/print_ship_attributes print_ship_attributes.c $(OD)/key_value_parser.o

local_termios2.h:	termios2.h
	$(Q)./check_for_termios2.sh

$(OD)/snis_dmx.o:	snis_dmx.c snis_dmx.h Makefile local_termios2.h ${ODT}
	$(Q)$(COMPILE)

bin/test_snis_dmx:	test_snis_dmx.c ${OD}/snis_dmx.o ${BIN}
	$(Q)$(CC) -pthread -o bin/test_snis_dmx test_snis_dmx.c ${OD}/snis_dmx.o

$(OD)/snis_test_audio.o:	snis_test_audio.c Makefile ${SNDOBJS} ${OGGOBJ} ${ODT}
	$(Q)$(VORBISCOMPILE)

bin/snis_test_audio:	${OD}/snis_test_audio.o ${SNDLIBS} Makefile ${BIN}
	$(CC) -o bin/snis_test_audio ${OD}/snis_test_audio.o ${SNDOBJS} ${OGGOBJ} ${SNDLIBS}

install:	${BINPROGS} ${MODELS} ${AUDIOFILES} ${TEXTURES} \
		${MATERIALS} ${CONFIGFILES} ${SHADERS} ${LUASCRIPTS} ${LUAUTILS} \
		${LUAMISSIONS} ${LUASRCLIBS} ${LUATESTS} ${MANPAGES} ${SSGL} \
		${SOLARSYSTEMFILES}
	@# First check that PREFIX is sane, and esp. that it's not pointed at source
	@mkdir -p ${DESTDIR}/${PREFIX}
	@touch ${DESTDIR}/${PREFIX}/.canary-in-the-coal-mine.canary
	@if [ -f .canary-in-the-coal-mine.canary ] ; then \
		echo 1>&2 ; \
		echo "DESTDIR/PREFIX is ${DESTDIR}/${PREFIX} -- cannot install here" 1>&2 ; \
		echo "Try: make PREFIX=/usr/local ; make PREFIX=/usr/local install" 1>&2  ; \
		echo 1>&2 ; \
		rm -f ${DESTDIR}/${PREFIX}/.canary-in-the-coal-mine.canary ; \
		exit 1 ; \
	fi
	@ rm -f ${DESTDIR}/${PREFIX}/.canary-in-the-coal-mine.canary
	mkdir -p ${DESTDIR}/${PREFIX}/bin
	for x in ${BINPROGS} ; do \
		${INSTALL} -m 755 $$x \
				${DESTDIR}/${PREFIX}/bin; \
	done
	${AWK} '/^PREFIX=.*/ { printf("PREFIX='${PREFIX}'\n"); next; } \
		{ print; } ' < snis_launcher > /tmp/snis_launcher
	${INSTALL} -m 755 /tmp/snis_launcher ${DESTDIR}/${PREFIX}/bin
	rm -f /tmp/snis_launcher
	for d in ${MATERIALDIR} ${LUASCRIPTDIR}/UTIL ${LUASCRIPTDIR}/TEST \
		${LUASCRIPTDIR}/MISSIONS ${LUASCRIPTDIR}/MISSIONS/lib ${SHADERDIR} ${SOUNDDIR} \
		${TEXTUREDIR} ${MODELDIR}/wombat ${SHADERDIR} ; do \
		mkdir -p $$d ; \
	done
	${INSTALL} -m 644 ${CONFIGFILES} ${CONFIGFILEDIR}
	${INSTALL} -m 644 ${SOUNDFILES} ${SOUNDDIR}
	${INSTALL} -m 644 ${TEXTUREFILES} ${TEXTUREDIR}
	${INSTALL} -m 644  ${LUASCRIPTS} ${LUASCRIPTDIR}
	${INSTALL} -m 644  ${LUAUTILS} ${LUASCRIPTDIR}/UTIL
	${INSTALL} -m 644  ${LUAMISSIONS} ${LUASCRIPTDIR}/MISSIONS
	${INSTALL} -m 644  ${LUASRCLIBS} ${LUASCRIPTDIR}/MISSIONS/lib
	${INSTALL} -m 644  ${LUATESTS} ${LUASCRIPTDIR}/TEST
	${INSTALL} -m 644  ${MATERIALFILES} ${MATERIALDIR}
	${INSTALL} -m 644  ${MODELS} ${MODELDIR}
	mkdir -p ${SOLARSYSTEMDIR}/default
	${INSTALL} -m 644 ${SOLARSYSTEMSRCDIR}/default/assets.txt ${SOLARSYSTEMDIR}/default
	for d in dreadknight disruptor conqueror enforcer starbase starbase2 cargocontainer research-vessel ; do \
		mkdir -p ${MODELDIR}/$$d ; \
		${INSTALL} -m 644 ${MODELSRCDIR}/$$d/$$d.mtl ${MODELDIR}/$$d ; \
		cp ${MODELSRCDIR}/$$d/$$d.obj ${MODELDIR}/$$d ; \
		cp ${MODELSRCDIR}/$$d/$$d.png ${MODELDIR}/$$d ; \
		if [ -f ${MODELSRCDIR}/$$d/$$d"lights.png" ] ; then \
			cp ${MODELSRCDIR}/$$d/$$d"lights.png" ${MODELDIR}/$$d ; \
		fi ; \
		if [ -f ${MODELSRCDIR}/$$d/$$d"-lighting.png" ] ; then \
			cp ${MODELSRCDIR}/$$d/$$d"-lighting.png" ${MODELDIR}/$$d ; \
		fi ; \
	done
	${INSTALL} -m 644 ${MODELSRCDIR}/dreadknight/dreadknight-exhaust-plumes.h ${MODELDIR}/dreadknight
	${INSTALL} -m 644 ${MODELSRCDIR}/wombat/snis3006lights.png ${MODELDIR}/wombat
	${INSTALL} -m 644 ${MODELSRCDIR}/wombat/snis3006.mtl ${MODELDIR}/wombat
	${INSTALL} -m 644 ${MODELSRCDIR}/wombat/snis3006.obj ${MODELDIR}/wombat
	${INSTALL} -m 644 ${MODELSRCDIR}/wombat/snis3006.png ${MODELDIR}/wombat
	${INSTALL} -m 644 ${DOCKING_PORT_FILES} ${MODELDIR}
	${INSTALL} -m 644 ${SCAD_PARAMS_FILES} ${MODELDIR}
	${INSTALL} -m 644 ${SHADERS} ${SHADERDIR}
	mkdir -p ${MANDIR}
	${INSTALL} -m 644 ${MANPAGES} ${MANDIR}
	mkdir -p ${DESKTOPDIR}
	${INSTALL} -m 644 ${DESKTOPFILES} ${DESKTOPDIR}
	${UPDATE_DESKTOP}

uninstall:
	@# check that PREFIX is sane
	@touch ${DESTDIR}/${PREFIX}/.canary-in-the-coal-mine.canary
	@if [ -f .canary-in-the-coal-mine.canary ] ; then \
		echo 1>&2 ; \
		echo "DESTDIR/PREFIX is ${DESTDIR}/${PREFIX} -- cannot uninstall here" 1>&2 ; \
		echo "Try: make PREFIX=/usr/local uninstall" 1>&2  ; \
		echo 1>&2 ; \
		rm -f ${DESTDIR}/${PREFIX}/.canary-in-the-coal-mine.canary ; \
		exit 1 ; \
	fi
	@rm -f ${DESTDIR}/${PREFIX}/.canary-in-the-coal-mine.canary
	if [ ! -d "${DESTDIR}/${PREFIX}" ] ; then \
		echo "DESTDIR/PREFIX is not a directory." 1>&2 ;\
		exit 1 ;\
	fi
	for x in ${BINPROGS} ; do \
		rm -f ${DESTDIR}/${PREFIX}/$$x ; \
	done
	rm -f ${DESTDIR}/${PREFIX}/bin/snis_launcher
	rm -fr ${DESTDIR}/${PREFIX}/share/snis
	rm -f ${MANDIR}/snis_client.6.gz ${MANDIR}/snis_client.6
	rm -f ${MANDIR}/snis_server.6.gz ${MANDIR}/snis_server.6
	rm -f ${MANDIR}/earthlike.1.gz ${MANDIR}/earthlike.1
	rm -f ${MANDIR}/ssgl_server.6
	rm -f ${MANDIR}/snis_test_audio.1.gz
	rm -f ${MANDIR}/snis_multiverse.6
	rm -f ${MANDIR}/snis_text_to_speech.sh.6
	rm -f ${DESKTOPDIR}/snis.desktop
	${UPDATE_DESKTOP}

clean:	mostly-clean
	rm -f ${MODELS} test_marshal test_crater
	rm -f opus-1.3.1.tar.gz

depend:
	rm -f Makefile.depend
	$(MAKE) Makefile.depend

Makefile.depend :
	# Do in 2 steps so that on failure we don't get an empty but "up to date"
	# dependencies file.
	makedepend -w0 -f- *.c | grep -v /usr | sort > Makefile.depend.tmp
	mv Makefile.depend.tmp Makefile.depend

bin/check-endianness:	check-endianness.c Makefile ${BIN}
	@echo "  COMPILE check-endianness.c"
	$(Q)$(CC) -o bin/check-endianness check-endianness.c

build_info.h: bin/check-endianness snis.h gather_build_info Makefile
	@echo "  GATHER BUILD INFO"
	$(Q)@./gather_build_info > build_info.h

cppcheck:
	cppcheck --force .

scan-build:
	make mostly-clean
	rm -fr /tmp/snis-scan-build-output
	scan-build -o /tmp/snis-scan-build-output make CC=clang
	xdg-open /tmp/snis-scan-build-output/*/index.html

# opus stuff for voice chat
opus-1.3.1.tar.gz:
	wget https://archive.mozilla.org/pub/opus/opus-1.3.1.tar.gz

opus-1.3.1:	opus-1.3.1.tar.gz
	tar xzf opus-1.3.1.tar.gz

libopus.a:	opus-1.3.1
	(cd opus-1.3.1 && ./configure && make && cp ./.libs/libopus.a ..)

include Makefile.depend

# To compile with voice chat, WITHVOICECHAT=yes,
# for no voice chat, make WITHVOICECHAT=no
WITHVOICECHAT=yes
# If your distro does not have opus library packages, the Makefile can download the source from
# Mozilla.org and compile it.  Normally you shouldn't need to do this, but you can change this
# to yes if you need to do this.
DOWNLOAD_OPUS?=no
USE_SNIS_XWINDOWS_HACKS=1
PKG_CONFIG?=pkg-config
SDL2_CONFIG?=sdl2-config
SERVERSONLY ?= 0
USE_GLES?=0

# "make WEB=1 web" builds snis_client to run in a web browser, using emscripten.
# See doc/web-client.txt.  It uses its own object directory, so it does not
# disturb the native build.
WEB?=0
ifeq (${WEB},1)
CC=emcc
USE_GLES=1
USE_SNIS_XWINDOWS_HACKS=0
NET_TRANSPORT_OBJ=snis_net_transport_web.o
MAIN_LOOP_OBJ=snis_main_loop_web.o
ASSET_FETCH_OBJ=snis_asset_fetch_web.o
JOYSTICK_OBJ=joystick_web.o
SPEAK_OBJ=snis_speak_web.o
else
NET_TRANSPORT_OBJ=snis_net_transport.o
MAIN_LOOP_OBJ=snis_main_loop.o
ASSET_FETCH_OBJ=snis_asset_fetch.o
JOYSTICK_OBJ=joystick.o
SPEAK_OBJ=snis_speak.o
endif

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

PREFIX?=.

ifeq (${UBSAN},1)
UBSANFLAG=-fsanitize=undefined
CC=clang
else
UBSANFLAG=
endif

ifeq (${ASAN},1)
ASANFLAG=-fsanitize=address
else
ASANFLAG=
endif

# Hack for raspberry pi, had some trouble getting -fsanitize=bounds to compile
# There is probably a better way to do this.
MYARCH=$(shell arch)
ifeq (${MYARCH},aarch64)
BOUNDSFLAGS=
UBSANFLAG=
ASANFLAG=
else
BOUNDSFLAGS=-fsanitize=bounds
endif

SPACEDUST=-DSPACEDUST
MOVING_STARFIELD=-DMOVING_STARFIELD

# If ccache is installed, use it. It makes hopping around in git history much
# cheaper, because files that were merely touched do not get recompiled.
# Say "make USE_CCACHE=0" to opt out.
#
# Only do this to a plain one-word CC. A CC of several words is already a
# wrapper of some sort -- "ccache gcc", "distcc gcc", "icecc gcc" -- and
# ccache(1) warns specifically against the "ccache anotherwrapper compiler"
# form, so someone who has set that up gets left exactly as they arranged it.
# A one-word CC that is itself a ccache symlink needs no such care: ccache
# skips any candidate resolving back to itself when it looks up the compiler,
# so it does not end up calling itself.
USE_CCACHE ?= 1
ifeq (${USE_CCACHE},1)
ifeq (1,$(words ${CC}))
ifeq (,$(findstring ccache,${CC}))
CCACHE:=$(shell command -v ccache 2>/dev/null)
ifneq (,${CCACHE})
# "override", so that "make CC=clang" gets the cache too. A plain assignment is
# ignored for a variable set on the command line, which would otherwise make
# ccache appear or vanish depending on whether you said "make CC=x" or "CC=x make".
override CC:=${CCACHE} ${CC}
endif
endif
endif
endif

# object fild directory
ifeq (${WEB},1)
OD=object_files_web
else
OD=object_files
endif

DATADIR=${DESTDIR}${PREFIX}/share/snis
CONFIGFILEDIR=${DATADIR}
CONFIGSRCDIR=./share/snis
CONFIGFILES=$(addprefix ${CONFIGSRCDIR}/,commodities.txt factions.txt \
	ship_types.txt starbase_models.txt user_colors.cfg joystick_config.txt)
SOLARSYSTEMSRCDIR=${CONFIGSRCDIR}/solarsystems
SOLARSYSTEMDIR=${DATADIR}/solarsystems
SOLARSYSTEMFILES=${SOLARSYSTEMSRCDIR}/default/assets.txt

ASSETSSRCDIR=share/snis

MODELSRCDIR=${ASSETSSRCDIR}/models
MODELDIR=${DATADIR}/models
SOUNDDIR=${DATADIR}/sounds
SOUNDSRCDIR=${ASSETSSRCDIR}/sounds
SOUNDFILES=$(addprefix ${SOUNDSRCDIR}/,Attribution.txt \
	atmospheric-friction.ogg big_explosion.ogg bigshotlaser.ogg \
	changescreen.ogg comms-hail.ogg crewmember-has-joined.ogg \
	dangerous-radiation.ogg docking-sound.ogg \
	docking-system-disengaged.ogg docking-system-engaged.ogg \
	entering-high-security-area.ogg flak_gun_sound.ogg flak_hit.ogg \
	fuel-levels-critical.ogg incoming-fire-detected.ogg laser-fail.ogg \
	leaving-high-security-area.ogg mining-bot-deployed.ogg \
	mining-bot-standing-by.ogg mining-bot-stowed.ogg new-starship.ogg \
	offscreen.ogg onscreen.ogg permission-to-dock-denied.ogg \
	permission-to-dock-has-expired.ogg permission-to-dock-granted.ogg \
	proximity-alert.ogg red-alert.ogg reverse.ogg \
	robot-insert-component.ogg robot-remove-component.ogg \
	science-data-acquired.ogg science-probe.ogg short-warpdrive.ogg \
	slider-noise.ogg spaceship-crash.ogg torpedo-loading.ogg \
	transporter-sound.ogg tty-chatter.ogg warning-hull-breach-imminent.ogg \
	warp-drive-fumble.ogg warpdrive.ogg welcome-to-starbase.ogg \
	hull-creak-0.ogg hull-creak-1.ogg hull-creak-2.ogg hull-creak-3.ogg \
	hull-creak-4.ogg hull-creak-5.ogg hull-creak-6.ogg hull-creak-7.ogg \
	hull-creak-8.ogg hull-creak-9.ogg \
	ui1.ogg ui2.ogg ui3.ogg ui4.ogg ui5.ogg ui6.ogg ui7.ogg ui8.ogg \
	ui9.ogg ui10.ogg ui11.ogg ui12.ogg ui13.ogg ui14.ogg ui15.ogg ui16.ogg \
	ui17.ogg ui18.ogg ui19.ogg ui20.ogg ui21.ogg ui22.ogg ui23.ogg \
	ui24.ogg ui25.ogg ui26.ogg ui27.ogg ui28.ogg ui29.ogg \
	spacemonster_slap.ogg alarm_buzzer.ogg atlas_rocket_sample.ogg \
	maneuvering_thruster.ogg quindar-intro.ogg quindar-outro.ogg \
	missile_launch.ogg)

TEXTUREDIR=${DATADIR}/textures
TEXTURESRCDIR=${ASSETSSRCDIR}/textures
TEXTUREFILES=$(addprefix ${TEXTURESRCDIR}/,AreaTex.h \
	asteroid1-0.png asteroid1-1.png asteroid1-2.png \
	asteroid1-3.png asteroid1-4.png asteroid1-5.png \
	asteroid2-0.png asteroid2-1.png asteroid2-2.png \
	asteroid2-3.png asteroid2-4.png asteroid2-5.png \
	Attribution.txt green-laser-texture.png \
	image0.png image1.png image2.png image3.png image4.png image5.png \
	nebula0.png nebula1.png nebula2.png nebula3.png nebula4.png nebula5.png \
	orange-haze0.png orange-haze1.png orange-haze2.png \
	orange-haze3.png orange-haze4.png orange-haze5.png \
	planetary-ring0.png \
	planet-texture0-0.png planet-texture0-1.png planet-texture0-2.png \
	planet-texture0-3.png planet-texture0-4.png planet-texture0-5.png \
	planet-texture0.png \
	planet-texture1-0.png planet-texture1-1.png planet-texture1-2.png \
	planet-texture1-3.png planet-texture1-4.png planet-texture1-5.png \
	planet-texture1.png \
	planet-texture2-0.png planet-texture2-1.png planet-texture2-2.png \
	planet-texture2-3.png planet-texture2-4.png planet-texture2-5.png \
	planet-texture2.png \
	planet-texture3-0.png planet-texture3-1.png planet-texture3-2.png \
	planet-texture3-3.png planet-texture3-4.png planet-texture3-5.png \
	planet-texture3.png \
	planet-texture4-0.png planet-texture4-1.png planet-texture4-2.png \
	planet-texture4-3.png planet-texture4-4.png planet-texture4-5.png \
	red-laser-texture.png red-torpedo-texture.png SearchTex.h \
	shield-effect-0.png shield-effect-1.png shield-effect-2.png \
	shield-effect-3.png shield-effect-4.png shield-effect-5.png \
	space-blue-plasma.png spark-texture.png sun.png \
	test0.png test1.png test2.png test3.png test4.png test5.png \
	thrust.png warp-effect.png warp-tunnel.png wormhole.png \
	green-burst.png blue-tractor-texture.png missile_texture.png \
	black_hole.png docking_port_emit.png docking_port_texture.png \
	green-spark.png laserflash.png spacemonster_emit.png \
	spacemonster_tentacle_emit.png spacemonster_tentacle_texture.png \
	spacemonster_texture.png spaceplate.png spaceplate_small.png \
	spaceplate_small_emit.png spaceplateemit.png thrust_flare.png \
	thrustblue.png thrustgreen.png thrustred.png thrustviolet.png \
	thrustyellow.png warpgate_emit.png warpgate_texture.png \
	anamorphic_flare.png lens_flare_ghost.png lens_flare_halo.png)

LUASCRIPTDIR=${DATADIR}/luascripts
LUASRCDIR=${ASSETSSRCDIR}/luascripts
LUASCRIPTS=${LUASRCDIR}/MAINMENU.LUA
LUAMISSIONS=$(addprefix ${LUASRCDIR}/MISSIONS/,BIGSHIP.LUA CUBOMUERTE.LUA \
	DEMOLISHER.LUA KALI.LUA ROYAL-WEDDING.LUA SAVING-PLANET-ERPH.LUA \
	SPACEPOX.LUA STURNVULF.LUA TRAINING-MISSION-1.LUA \
	TRAINING-MISSION-2.LUA missions_menu.txt)
LUASRCLIBS=${LUASRCDIR}/MISSIONS/lib/interactive_fiction.lua
LUATESTS=$(addprefix ${LUASRCDIR}/TEST/,CARGO.LUA COLLISION.LUA \
	REVIEW_MODELS.LUA SILLY-SPACE-BATTLE.LUA SOUND.LUA \
	TESTCUSTOMBUTTONS.LUA TESTDERELICT.LUA TESTDOCK.LUA TESTEXPL.LUA \
	TESTSPHEROID.LUA TESTWARPEVENT.LUA TESTINTFIC.LUA TESTRUNAWAY.LUA \
	TESTTORP.LUA TESTDAMAGE.LUA TESTCIPHER.LUA)
LUAUTILS=$(addprefix ${LUASRCDIR}/UTIL/,CLEAR_ALL.LUA DAMAGE.LUA \
	DESTROY_SHIP.LUA DISABLE_ANTENNA.LUA ENABLE_ANTENNA.LUA \
	PAY_PLAYER.LUA PLAYER_SPEED_BOOST.LUA PLAYER_SPEED_DEFAULT.LUA \
	REDALERT.LUA REGENERATE.LUA RESETBRIDGE.LUA RESTORE_SKYBOX.LUA \
	SAY.LUA SET_PLANET_DESCRIPTION.LUA SET_PLANET_ECONOMY.LUA \
	SET_PLANET_GOVT.LUA SET_PLANET_SECURITY.LUA SET_PLANET_TECH.LUA \
	LS.LUA SETFACTION.LUA SET_SB_FACTION_MASK.LUA utility_menu.txt)

MATERIALDIR=${DATADIR}/materials
MATERIALSRCDIR=${ASSETSSRCDIR}/materials
MATERIALFILES=$(addprefix ${MATERIALSRCDIR}/,nebula0.mat nebula10.mat \
	nebula11.mat nebula12.mat nebula13.mat nebula14.mat nebula15.mat \
	nebula16.mat nebula17.mat nebula18.mat nebula19.mat nebula1.mat \
	nebula2.mat nebula3.mat nebula4.mat nebula5.mat nebula6.mat \
	nebula7.mat nebula8.mat nebula9.mat)

SHADERDIR=${DESTDIR}${PREFIX}/share/snis/shader
SHADERSRCDIR=${ASSETSSRCDIR}/shader
SHADERS=$(addprefix ${SHADERSRCDIR}/,atmosphere.frag atmosphere.vert \
	color_by_w.frag color_by_w.vert csm_depth.frag csm_depth.vert \
	fs-effect-copy.shader line-single-color.frag line-single-color.vert \
	per_vertex_color.frag per_vertex_color.vert \
	point_cloud.frag point_cloud.vert \
	single_color.frag single-color-lit-per-pixel.frag \
	single-color-lit-per-pixel.vert single-color-lit-per-vertex.frag \
	single-color-lit-per-vertex.vert single_color.vert \
	skybox.frag skybox.vert \
	smaa-blend.shader smaa-edge.shader smaa-high.shader SMAA.hlsl \
	smaa-neighborhood.shader textured-and-lit-per-pixel.shader \
	textured-and-lit-per-vertex.shader \
	textured-cubemap-shield-per-pixel.shader \
	textured-cubemap-and-lit-with-annulus-shadow-per-pixel.shader \
	textured-particle.frag textured-particle.vert textured.shader \
	textured-with-sphere-shadow-per-pixel.shader \
	planetary-ring-dust.shader \
	wireframe_filled.frag wireframe_filled.vert \
	wireframe_transparent.frag wireframe-transparent-sphere-clip.frag \
	wireframe-transparent-sphere-clip.vert wireframe_transparent.vert \
	alpha_by_normal.shader)

_SCAD_PARAMS_MODELS=disruptor enforcer starbase5 research-vessel conqueror \
	cruiser asteroid-miner battlestar wombat destroyer dragonhawk tanker \
	swordfish vanquisher freighter scrambler spaceship2 spaceship3 \
	skorpio spaceship transport escapepod mantis missile
SCAD_PARAMS_FILES=$(patsubst %,${MODELSRCDIR}/%.scad_params.h,${_SCAD_PARAMS_MODELS})

_DOCKING_PORT_MODELS=starbase2 starbase3 starbase4 starbase5 starbase6 starbase
DOCKING_PORT_FILES=$(patsubst %,${MODELSRCDIR}/%.docking_ports.h,${_DOCKING_PORT_MODELS})

MANSRCDIR=./man
MAN6PAGES=$(addprefix ${MANSRCDIR}/,snis_client.6.gz snis_server.6.gz \
	snis_text_to_speech.sh.6 snis_multiverse.6 snis_update_assets.6 \
	snis-console.6 snis_web_gateway.6) ssgl/ssgl_server.6
MAN1PAGES=$(addprefix ${MANSRCDIR}/,earthlike.1.gz snis_test_audio.1.gz lsssgl.1)
MAN6DIR=${DESTDIR}${PREFIX}/share/man/man6
MAN1DIR=${DESTDIR}${PREFIX}/share/man/man1

METAINFOSRCDIR=./share/metainfo
METAINFODIR=${DESTDIR}${PREFIX}/share/metainfo
METAINFOFILE=${METAINFOSRCDIR}/io.github.smcameron.space-nerds-in-space.metainfo.xml
METAINFOFILE_TEMPLATE=${METAINFOFILE}.template

DESKTOPDIR=${DESTDIR}${PREFIX}/share/applications
DESKTOPSRCDIR=./share/applications
DESKTOPFILES=${DESKTOPSRCDIR}/io.github.smcameron.space-nerds-in-space.desktop \
	${DESKTOPSRCDIR}/io.github.smcameron.space-nerds-in-space.svg
UPDATE_DESKTOP=update-desktop-database ${DESKTOPDIR} || :

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

ifeq (${SERVERSONLY},0)
SNDLIBS:=$(shell $(PKG_CONFIG) --libs portaudio-2.0 vorbisfile)
SNDFLAGS:=-DWITHAUDIOSUPPORT $(shell $(PKG_CONFIG) --cflags portaudio-2.0) -DDATADIR=\"${DATADIR}\"
_OGGOBJ=ogg_to_pcm.o
_SNDOBJS=wwviaudio.o
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

ifeq (${SERVERSONLY},0)
PNGLIBS:=$(shell $(PKG_CONFIG) --libs libpng)
PNGCFLAGS:=$(shell $(PKG_CONFIG) --cflags libpng)

SDLLIBS:=$(shell $(SDL2_CONFIG) --libs)
SDLCFLAGS:=$(shell $(SDL2_CONFIG) --cflags)
NCURSESCFLAGS:=$(shell $(PKG_CONFIG) --cflags ncurses)
NCURSESLIBS:=$(shell $(PKG_CONFIG) --libs ncurses)

GLADLIBS=
COMMON_GRAPH_OBJS=graph_dev_texture_cache.o graph_dev_mesh_cache.o graph_dev_context.o graph_dev_shader_setup.o star_light.o
ifeq (${USE_GLES},0)
GLADCFLAGS=-Iextern/glad/include
GRAPH_OBJS=glad-gl.o graph_dev_opengl.o ${COMMON_GRAPH_OBJS} opengl_cap.o
else
GLADCFLAGS=-Iextern/glad-es/include -DUSE_GLES=1
GRAPH_OBJS=glad-gles2.o graph_dev_gles.o ${COMMON_GRAPH_OBJS} gles_cap.o
endif
endif

ifeq ($(OSX), 0)
	CRYPTLIBS:=-lcrypt
else
	CRYPTLIBS:=""
endif

_COMMONOBJS=mathutils.o snis_alloc.o snis_socket_io.o snis_marshal.o \
		bline.o shield_strength.o stacktrace.o snis_ship_type.o \
		snis_faction.o mtwist.o names.o infinite-taunt.o snis_damcon_systems.o \
		string-utils.o c-is-the-locale.o starbase_metadata.o arbitrary_spin.o \
		planetary_atmosphere.o planetary_ring_data.o png_utils.o mesh.o pthread_util.o \
		snis_opcode_def.o rts_unit_data.o commodities.o snis_tweak.o rootcheck.o \
		corporations.o replacement_assets.o snis_asset_dir.o snis_bin_dir.o scipher.o \
		planetary_properties.o open-simplex-noise.o snis_licenses.o net_utils.o \
		${NET_TRANSPORT_OBJ}
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
		snis_bin_dir.o snis_licenses.o net_utils.o commodities.o ${NET_TRANSPORT_OBJ}
MULTIVERSEOBJS=$(patsubst %,$(OD)/%,${_MULTIVERSEOBJS})

OGGOBJ=$(patsubst %,$(OD)/%,${_OGGOBJ})
SNDOBJS=$(patsubst %,$(OD)/%,${_SNDOBJS})

_COMMONCLIENTOBJS= snis_ui_element.o snis_font.o snis_text_input.o \
	snis_typeface.o snis_gauge.o snis_button.o snis_label.o snis_sliders.o snis_text_window.o \
	snis_strip_chart.o material.o stl_parser.o entity.o matrix.o my_point.o liang-barsky.o ${JOYSTICK_OBJ} \
	quat.o vec4.o thrust_attachment.o docking_port.o ui_colors.o snis_keyboard.o solarsystem_config.o \
	pronunciation.o snis_preferences.o snis_pull_down_menu.o snis_client_debug.o starmap_adjacency.o \
	shape_collision.o oriented_bounding_box.o xdg_base_dir_spec.o snis_voice_chat.o read_menu_file.o \
	snis_client_forker.o snis_process_options.o workqueue.o ${MAIN_LOOP_OBJ}
COMMONCLIENTOBJS=${COMMONOBJS} ${OGGOBJ} ${SNDOBJS} $(patsubst %,$(OD)/%,${_COMMONCLIENTOBJS}) 

_CLIENTOBJS= shader.o ${GRAPH_OBJS} snis_graph.o snis_client.o joystick_config.o snis_xwindows_hacks.o \
				black_hole_lens.o mesh_fracture.o particle_batch.o ship_death.o \
				ship_death_fireball.o ship_death_shrapnel.o ship_death_wreck.o snis_wled.o \
				${ASSET_FETCH_OBJ} ${SPEAK_OBJ}
CLIENTOBJS=${COMMONCLIENTOBJS} $(patsubst %,$(OD)/%,${_CLIENTOBJS})

_VIEWER_COMMON_OBJS=shader.o ${GRAPH_OBJS} snis_graph.o \
				png_utils.o turret_aimer.o quat.o mathutils.o mesh.o \
				mtwist.o material.o entity.o snis_alloc.o matrix.o stacktrace.o stl_parser.o \
				snis_typeface.o snis_font.o string-utils.o ui_colors.o liang-barsky.o \
				bline.o vec4.o open-simplex-noise.o replacement_assets.o \
				snis_xwindows_hacks.o workqueue.o pthread_util.o

_SDLCLIENTOBJS=mesh_viewer.o ${_VIEWER_COMMON_OBJS}
SDLCLIENTOBJS=$(patsubst %,$(OD)/%,${_SDLCLIENTOBJS}) mikktspace/mikktspace.o

_SHADOWLABOBJS=shadow_lab.o ${_VIEWER_COMMON_OBJS} black_hole_lens.o \
				solarsystem_config.o snis_asset_dir.o
SHADOWLABOBJS=$(patsubst %,$(OD)/%,${_SHADOWLABOBJS}) mikktspace/mikktspace.o

_STARLIGHTPREVIEWOBJS=star_light_preview.o ${_VIEWER_COMMON_OBJS}
STARLIGHTPREVIEWOBJS=$(patsubst %,$(OD)/%,${_STARLIGHTPREVIEWOBJS}) mikktspace/mikktspace.o

_NEBULANOISEOBJS=nebula_noise.o open-simplex-noise.o png_utils.o
NEBULANOISEOBJS=$(patsubst %,$(OD)/%,${_NEBULANOISEOBJS})
NEBULANOISELIBS=-lm ${PNGLIBS}

_GENERATE_SKYBOX_OBJS=generate_skybox.o open-simplex-noise.o png_utils.o mathutils.o quat.o mtwist.o
GENERATE_SKYBOX_OBJS=$(patsubst %,$(OD)/%,${_GENERATE_SKYBOX_OBJS})
GENERATE_SKYBOX_LIBS=-lm ${PNGLIBS}
X11LIBS=$(shell $(PKG_CONFIG) --libs x11)
X11CFLAGS=$(shell $(PKG_CONFIG) --cflags x11)

SSGL=ssgl/libssglclient.a
LIBS=${GLADLIBS} -Lssgl -lssglclient -ldl -lm ${PNGLIBS}
MULTIVERSELIBS=-Lssgl -lssglclient ${LRTLIB} -ldl -lm ${CRYPTLIBS}
SERVERLIBS=${MULTIVERSELIBS} ${LUALIBS} ${PNGLIBS}
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


SERVERPROGS=bin/ssgl_server bin/snis_server bin/snis_multiverse
BINPROGS=${SERVERPROGS} bin/snis_client bin/snis_text_to_speech.sh \
		bin/lsssgl bin/snis_arduino bin/snis_launcher bin/snis-console \
		bin/snis_update_assets bin/update_assets_from_launcher.sh bin/snis_web_gateway
UTILPROGS=util/mask_clouds util/cloud-mask-normalmap bin/mesh_viewer bin/shadow_lab bin/star_light_preview util/sample_image_colors \
		util/generate_solarsystem_positions bin/nebula_noise bin/generate_skybox bin/earthlike
TESTPROGS=bin/test-matrix bin/test-mathutils bin/test-space-partition \
		bin/test_marshal bin/test-quat bin/test-fleet bin/test-mtwist \
		bin/test-commodities bin/test_solarsystem_config \
		bin/test_mesh_fracture bin/test_wled \
		bin/test_key_value_parser bin/test_stringutils \
		bin/test_transport_contract bin/test_snis_ship_type \
		bin/test_nonuniform_random_sampler
FUZZPROGS=fuzz_obj_parser fuzz_snis_read_ship_types \
		fuzz_solarsystem_asset_spec_read fuzz_read_thrust_attachments \
		fuzz_process_manifest fuzz_read_joystick_config \
		fuzz_read_commodities fuzz_read_starbase_metadata \
		fuzz_read_docking_port_attachments

# model directory
MD=${ASSETSSRCDIR}/models

_MODELS=freighter laser spaceship torpedo tanker destroyer transport \
	battlestar cruiser tetrahedron flat-tetrahedron big-flat-tetrahedron \
	asteroid asteroid2 asteroid3 asteroid4 wormhole \
	starbase3 starbase4 starbase5 starbase6 spacemonster asteroid-miner \
	spaceship2 spaceship3 dragonhawk skorpio long-triangular-prism \
	ship-icon heading_indicator heading_indicator_tail axis scrambler \
	swordfish wombat spaceship_turret spaceship_turret_base vanquisher \
	docking_port docking_port2 warpgate escapepod mantis \
	laser_turret laser_turret_base uv_sphere warp-core \
	space_monster_torso space_monster_tentacle_segment cylinder \
	missile mf-cockpit
MODELS=$(patsubst %,${MD}/%.stl,${_MODELS})

# compiler specific cflags
ifeq (${USING_CLANG},)
#
# We are not using clang.
#
# clang doesn't seem to know about these
# (which is weird, because "git -S no-extended-offsetof" shows this flag
# was added in the commit enabling clang support.)
#
COMPSPECCFLAGS=-Wno-extended-offsetof -Wno-format-truncation -Wstringop-truncation -Wstringop-overflow
else
# We are using clang
#
# I do not remember why we have -Wno-gnu-folding-constant, it was added when clang support
# was added, so clang apparently needed this at one time. It's not clear that it is still
# needed.  I suspect clang used to throw many spurious warnings related to this.
#
COMPSPECCFLAGS=-Wno-gnu-folding-constant
endif

# Some of these compiler options are cargo-culted from
#   https://best.openssf.org/Compiler-Hardening-Guides/Compiler-Options-Hardening-Guide-for-C-and-C++.html
#   There's a lot of work we need to do before we can turn on -Wconversion though.
#
# -fexceptions is called out for C code using glibc pthreads.
#
MYCFLAGS=-DDESTDIR=${DESTDIR} -DPREFIX=${PREFIX} ${DEBUGFLAG} ${PROFILEFLAG} \
	${OPTIMIZEFLAG} ${ASANFLAG} ${UBSANFLAG} ${SPACEDUST} ${MOVING_STARFIELD}\
	--pedantic -Wall -Wextra -Wformat -Wformat=2 -Werror=format-security ${STOP_ON_WARN} -pthread -std=gnu99 ${RDYNAMIC} \
	$(CFLAGS) -Wvla \
	-DUSE_SNIS_XWINDOWS_HACKS=${USE_SNIS_XWINDOWS_HACKS} -fno-common \
	-U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=3 ${BOUNDSFLAGS}\
	-Warray-bounds \
	-fstack-clash-protection -fstack-protector-strong -Wimplicit-fallthrough \
	-Wl,-z,nodlopen -Wl,-z,noexecstack \
	-Wl,-z,relro -Wl,-z,now \
	-Wl,--as-needed -Wl,--no-copy-dt-needed-entries \
	${COMPSPECCFLAGS} -Wstrict-prototypes -fexceptions -Wshadow
# -I. so that headers under graph_dev/ can reach the ones at the top level.
MYCFLAGS += -I.

ifeq (${SERVERSONLY},0)
VORBISFLAGS:=$(subst -I,-isystem ,$(shell $(PKG_CONFIG) --cflags vorbisfile))
endif

ifeq (${WITHVOICECHAT},yes)
ifeq (${WEB},1)
# The web build compiles its own Opus with emscripten
LIBOPUS=libopus-web.a
OPUSCFLAGS=-I./opus-1.3.1/include
VCHAT=-DWITHVOICECHAT=1
OPUSARCHIVE=libopus-web.a
else ifeq (${DOWNLOAD_OPUS},no)
LIBOPUS:=$(shell $(PKG_CONFIG) --libs opus)
OPUSCFLAGS:=$(shell $(PKG_CONFIG) --cflags opus)
VCHAT=-DWITHVOICECHAT=1
OPUSARCHIVE=
else
# we're downloading Opus
LIBOPUS=-L. -lopus
OPUSCFLAGS=-I./opus-1.3.1/include
VCHAT=-DWITHVOICECHAT=1
OPUSARCHIVE=libopus.a
endif
else
# no voice chat
LIBOPUS=
OPUSCFLAGS=
VCHAT=
OPUSARCHIVE=
endif

ifeq (${V},1)
Q=
ECHO=echo
else
Q=@
ECHO=echo
endif

ifeq (${WEB},1)
# Emscripten provides SDL2, libpng, zlib and libvorbis itself.  Sound goes
# through SDL instead of portaudio, and there is no X11.
SDLLIBS=-sUSE_SDL=2
SDLCFLAGS=-sUSE_SDL=2
PNGLIBS=-sUSE_LIBPNG=1 -sUSE_ZLIB=1
PNGCFLAGS=-sUSE_LIBPNG=1
X11LIBS=
X11CFLAGS=
LUACFLAGS=
VORBISFLAGS=-sUSE_VORBIS=1 -sUSE_SDL=2
SNDLIBS=-sUSE_VORBIS=1
SNDFLAGS=-DWWVIAUDIO_USE_SDL -DDATADIR=\"${DATADIR}\"
COMMONOBJS=$(patsubst %,$(OD)/%,${_COMMONOBJS}) $(OD)/mikktspace.o
# The lobby connection goes over snis_net_transport (snis_web_ssgl.c), not
# ssgl_connect_to_lobby.c and ssgl_socket_io.c, and a browser never registers
# a game server with the lobby.
_SSGLWEBOBJS=ssgl_sleep.o ssgl_sanitize.o ssgl_recv_game_servers.o \
		ssgl_get_primary_host_ip_addr.o ssgl_log.o ssgl_get_gamelobby_port.o ssgl_string.o
SSGL=$(OD)/libssglclient.a
LIBS=${SSGL} -lm ${PNGLIBS}
_CLIENTOBJS+=snis_web_stubs.o snis_web_ssgl.o
# These are for ELF and x86 linkers and compilers, not for wasm.
COMMA:=,
MYCFLAGS:=$(filter-out -Wl$(COMMA)% -fstack-clash-protection -D_FORTIFY_SOURCE=3 -fexceptions,${MYCFLAGS})
# The web client's assets are downloaded once, by snis_update_assets, into
# WEB_ASSET_CACHE, which "make mostly-clean" leaves alone.  "make WEB=1
# web-update-assets" checks for newer ones.  They are bundled where
# snis_asset_dir.c looks by default, so nothing needs configuring.
WEB_ASSET_CACHE?=web-asset-cache
WEB_ASSET_STAMP=${WEB_ASSET_CACHE}/share/snis/.downloaded
WEB_SOLARSYSTEM?=default
WEB_ASSET_EXCLUDES=--exclude=/shader --exclude='*.scad' --exclude='*.scad_params.h' \
	--exclude=.downloaded \
	--include='/solarsystems/${WEB_SOLARSYSTEM}/***' --exclude='/solarsystems/*'
WEBLDFLAGS=-pthread -sPTHREAD_POOL_SIZE=24 \
	-sALLOW_MEMORY_GROWTH=1 -sINITIAL_MEMORY=512MB -sMAXIMUM_MEMORY=4GB \
	-sSTACK_SIZE=4MB -sDEFAULT_PTHREAD_STACK_SIZE=2MB \
	-sMAX_WEBGL_VERSION=2 -sGL_ENABLE_GET_PROC_ADDRESS=1 \
	-sENVIRONMENT=web,worker -sEXIT_RUNTIME=0 \
	--preload-file $(OD)/web-assets@/home/web_user/.local/share/space-nerds-in-space/share/snis
endif

COMPILE=$(ECHO) '  COMPILE' $< && $(CC) ${MYCFLAGS} ${LUACFLAGS} -c -o $@ $<
VORBISCOMPILE=$(ECHO) '  COMPILE' $< && $(CC) ${MYCFLAGS} ${VORBISFLAGS} ${SNDFLAGS} -c -o $@ $<
SDLCOMPILE=$(ECHO) '  COMPILE' $< && $(CC) ${MYCFLAGS} ${GLADCFLAGS} ${SDLCFLAGS} ${X11CFLAGS} -c -o $@ $<
SNISSERVERDBGCOMPILE=$(ECHO) '  COMPILE' $< && $(CC) -DSNIS_SERVER_DATA ${MYCFLAGS} ${LUACFLAGS} -c -o $(OD)/snis_server_debug.o $<
SNISCLIENTDBGCOMPILE=$(ECHO) '  COMPILE' $< && $(CC) -DSNIS_CLIENT_DATA ${MYCFLAGS} ${LUACFLAGS} -c -o $(OD)/snis_client_debug.o $<

CLIENTLINK=$(ECHO) '  LINK' $@ && $(CC) ${MYCFLAGS} ${SNDFLAGS} -o $@ ${X11LIBS} ${SDLCFLAGS} ${CLIENTOBJS} ${SDLLIBS} ${LIBS} ${SNDLIBS} $(LDFLAGS) ${LIBOPUS} ${X11LIBS}
VIEWERLINK=$(ECHO) '  LINK' $@ && $(CC) ${MYCFLAGS} ${SNDFLAGS} -o $@ ${SDLCFLAGS} $(filter %.o,$^) ${SDLLIBS} ${LIBS} ${SNDLIBS} $(LDFLAGS) ${X11LIBS}
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
MCLIBS=${ELLIBS}
_MCOBJS=png_utils.o
MCOBJS=$(patsubst %,$(OD)/%, ${_MCOBJS})
MCLINK=$(ECHO) '  LINK' $@ && $(CC) ${MYCFLAGS} -o $@ $(filter %.o,$^) ${MCLIBS} $(LDFLAGS)

all:	bin/.t ${COMMONOBJS} ${SERVEROBJS} ${MULTIVERSEOBJS} ${CLIENTOBJS} ${BINPROGS} ${SCAD_PARAMS_FILES} ${DOCKING_PORT_FILES} ${METAINFOFILE}

# if you only want to build the servers, say on a cloud server
# use WITHVOICECHAT=no SERVERSONLY=1 to avoid complaints from pkg-config
serversonly:	${SERVERPROGS}

models:	${MODELS}

# we need to depend on ${OD}/.t not on ${OD} because OD is a directory and the
# timestamp changes whenever the directory contents change, and we need to depend
# only on the *existence* of the directory, not on the contents. Same for ${BIN}.
ODT=${OD}/.t
BIN=bin/.t

${ODT} ${BIN}:
	@mkdir -p $(dir $@)
	@if [ ! -f $@ ] ; then \
		touch $@ ; \
	fi

# Rule to prevent common error of trying to "make foo" instead of "make bin/foo"
BINARY_NAMES=snis_client snis_server snis_limited_client snis_multiverse nebula_noise \
	generate_skybox ssgl_server lsssgl snis_text_to_speech.sh mesh_viewer shadow_lab star_light_preview earthlike \
	infinite-taunt names stl_parser test_key_value_parser test-matrix test-mathutils test-space-partition \
	test_marshal test-quat test-fleet test-mtwist device-io-sample-1 test_nonuniform_random_sampler \
	test-commodities test-obj-parser test_solarsystem_config test_crater print_ship_attributes \
	test_snis_dmx snis_test_audio check-endianness test_stringutils yoke-test-program \
	test_mesh_fracture test_wled test_snis_ship_type test_transport_contract
${BINARY_NAMES}:
	@echo "You probably meant: make bin/$@" 1>&2
	@/bin/false

# Updating assets is now done via snis_launcher
#
# update-assets:
#	@util/snis_update_assets.sh
#
# check-assets:
#	@util/snis_update_assets.sh --dry-run
#
# install-assets:
#	@util/snis_update_assets.sh --localcopy --destdir ${DESTDIR}${PREFIX}

build:	all

utils:	${UTILPROGS}

_SDLCOMPILE_OBJS=graph_dev_opengl.o opengl_cap.o graph_dev_gles.o gles_cap.o \
	material.o snis_keyboard.o joystick_web.o mesh_viewer.o shadow_lab.o \
	star_light_preview.o black_hole_lens.o snis_graph.o snis_typeface.o \
	snis_gauge.o snis_button.o snis_pull_down_menu.o snis_strip_chart.o \
	snis_label.o snis_sliders.o snis_text_window.o snis_ui_element.o \
	snis_text_input.o entity.o
SDLCOMPILE_OBJS=$(patsubst %,$(OD)/%,${_SDLCOMPILE_OBJS})

_VORBISCOMPILE_OBJS=ogg_to_pcm.o wwviaudio.o snis_test_audio.o
VORBISCOMPILE_OBJS=$(patsubst %,$(OD)/%,${_VORBISCOMPILE_OBJS})

$(OD)/%.o:	%.c Makefile ${ODT}
	$(Q)$(COMPILE)

$(OD)/graph_dev_%.o:	graph_dev/%.c Makefile ${ODT}
	$(Q)$(SDLCOMPILE)

$(SDLCOMPILE_OBJS): $(OD)/%.o:	%.c Makefile ${ODT}
	$(Q)$(SDLCOMPILE)

$(VORBISCOMPILE_OBJS): $(OD)/%.o:	%.c Makefile ${ODT}
	$(Q)$(VORBISCOMPILE)

$(OD)/rootcheck.o:	rootcheck.h

$(OD)/graph_dev_opengl.o: graph_dev.h shader.h vertex.h triangle.h \
		mtwist.h mathutils.h matrix.h quat.h mesh.h vec4.h snis_graph.h graph_dev.h \
		material.h entity.h entity_private.h snis_typeface.h opengl_cap.h png_utils.h

# The texture cache, shared by both graph_dev backends.  Built with the same flags as
# whichever one is in use, since USE_GLES decides which glad header it picks up.
$(OD)/graph_dev_texture_cache.o: graph_dev/texture_cache.h \
		graph_dev.h png_utils.h snis_graph.h workqueue.h string-utils.h

# The mesh cache, likewise.  Same reasoning, same flags.
$(OD)/graph_dev_mesh_cache.o: graph_dev/mesh_cache.h mesh.h \
		vertex.h triangle.h quat.h opengl_cap.h snis_profile.h

# The shader setup that is the same on both GL versions.
$(OD)/graph_dev_shader_setup.o: graph_dev/shader_setup.h \
		graph_dev/context.h shader.h

# The shared renderer context and the 2D drawing that needs only it.
$(OD)/graph_dev_context.o: graph_dev/context.h graph_dev.h \
		graph_dev/mesh_cache.h snis_graph.h star_light.h material.h entity.h \
		entity_private.h matrix.h

$(OD)/graph_dev_gles.o: graph_dev.h shader.h vertex.h triangle.h \
		mtwist.h mathutils.h matrix.h quat.h mesh.h vec4.h snis_graph.h graph_dev.h \
		material.h entity.h entity_private.h snis_typeface.h opengl_cap.h png_utils.h \
		graph_dev/texture_cache.h graph_dev/mesh_cache.h graph_dev/context.h

$(OD)/graph_dev_mesh_stub.o:	graph_dev_mesh_stub.h

$(OD)/material.o: mtwist.h quat.h snis_graph.h material.h star_light.h \
			graph_dev.h build_bug_on.h

$(OD)/shader.o : shader.c Makefile ${ODT}
	$(Q)$(COMPILE) ${GLADCFLAGS}

%.stl:	%.scad
	$(Q)$(OPENSCAD)

%.scad_params.h: %.scad
	$(Q)$(EXTRACTSCADPARAMS)

%.docking_ports.h: %.scad
	$(Q)$(EXTRACTDOCKINGPORTS)

$(OD)/glad-gles2.o:	extern/glad-es/src/gles2.c
	$(Q)$(COMPILE) -Iextern/glad-es/include

$(OD)/glad-gl.o:	extern/glad/src/gl.c
	$(Q)$(COMPILE) -Iextern/glad/include

$(OD)/thrust_attachment.o:	thrust_attachment.h
$(OD)/docking_port.o:	docking_port.h
$(OD)/snis_licenses.o:	snis_licenses.h
$(OD)/net_utils.o:	net_utils.h
$(OD)/ui_colors.o:	ui_colors.h snis_graph.h
$(OD)/snis_keyboard.o:	snis_keyboard.h ${OD}/string-utils.o xdg_base_dir_spec.h
$(OD)/snis_preferences.o:	snis_preferences.h string-utils.h snis_packet.h
$(OD)/solarsystem_config.o:	solarsystem_config.h string-utils.h

solarsystem_config_test: solarsystem_config.c ${OD}/string-utils.o ${ODT} ${OD}/stacktrace.o
	$(CC) ${MYCFLAGS} -DSOLARSYSTEM_CONFIG_TEST=1 -o $@ solarsystem_config.c ${OD}/string-utils.o ${OD}/stacktrace.o

$(OD)/mesh.o:	mikktspace/mikktspace.h open-simplex-noise.h
$(OD)/pthread_util.o:	pthread_util.h
$(OD)/snis_ship_type.o:	snis_ship_type.h quat.h corporations.h

bin/snis_launcher:	snis_launcher
bin/update_assets_from_launcher.sh:	util/update_assets_from_launcher.sh
bin/snis_launcher bin/update_assets_from_launcher.sh:
	./modify_snis_launcher.sh $< "${DESTDIR}" "${PREFIX}" > $@
	chmod +x $@

bin/test_snis_ship_type: snis_ship_type.c snis_ship_type.h quat.h ${OD}/string-utils.o ${OD}/corporations.o ${OD}/rts_unit_data.o ${OD}/stacktrace.o ${BIN}
	$(Q)$(ECHO) '  LINK' $@ && $(CC) ${MYCFLAGS} -DTEST_SNIS_SHIP_TYPE -o $@ snis_ship_type.c ${OD}/string-utils.o ${OD}/corporations.o ${OD}/rts_unit_data.o ${OD}/stacktrace.o -lm

$(OD)/snis_faction.o:	string-utils.h
$(OD)/joystick_web.o:	joystick.h

joystick_test:	joystick.c joystick.h Makefile
	$(CC) -g -DJOYSTICK_TEST -o joystick_test joystick.c

$(OD)/joystick_config.o:	joystick_config.h string-utils.h

wwviaudio_basic_test:	wwviaudio.c wwviaudio.h ${OGGOBJ}
	$(CC) ${MYCFLAGS} -DWWVIAUDIO_BASIC_TEST -DDATADIR=\".\" -o wwviaudio_basic_test wwviaudio.c ${OGGOBJ} ${SNDLIBS}

wwviaudio_recording_test:	wwviaudio.c wwviaudio.h ${OGGOBJ}
	$(CC) ${MYCFLAGS} -DWWVIAUDIO_RECORDING_TEST -DDATADIR=\".\" -o wwviaudio_recording_test wwviaudio.c ${OGGOBJ} ${SNDLIBS}

$(OD)/planetary_properties.o:	planetary_properties.h
$(OD)/snis_server.o:	build_info.h ${DOCKING_PORT_FILES}
$(OD)/snis_multiverse.o:	snis_multiverse.h build_info.h \
			snis_entity_key_value_specification.h
$(OD)/snis_server_tracker.o:	snis_server_tracker.h pthread_util.h ssgl/ssgl.h

$(OD)/snis_client.o:	snis_client.c Makefile build_info.h ui_colors.h snis_ui.h ${ODT}
	$(Q)$(SDLCOMPILE) ${VCHAT}

$(OD)/mesh_viewer.o:	build_info.h
$(OD)/shadow_lab.o:	build_info.h
$(OD)/star_light_preview.o:	star_light.h build_info.h
$(OD)/star_light.o:	star_light.h

# simplexnoise1234.o:	simplexnoise1234.c Makefile build_info.h
#	$(Q)$(COMPILE)

$(OD)/open-simplex-noise.o:	build_info.h
$(OD)/nebula_noise.o:	png_utils.h open-simplex-noise.h

bin/nebula_noise:	$(NEBULANOISEOBJS) ${BIN}
	$(Q)$(NEBULANOISELINK)

$(OD)/generate_skybox.o:	png_utils.h open-simplex-noise.h quat.h mtwist.h mathutils.h

bin/generate_skybox:	$(GENERATE_SKYBOX_OBJS) ${BIN}
	$(Q)$(GENERATE_SKYBOX_LINK)

/util/mask_clouds.o:	util/mask_clouds.c
	$(Q)$(COMPILE)

/util/cloud-mask-normalmap.o:	util/cloud-mask-normalmap.c
	$(Q)$(COMPILE)

$(OD)/util/sample_image_colors.o:	${OD}/png_utils.o

util/sample_image_colors:	util/sample_image_colors.o ${OD}/png_utils.o
	$(CC) ${MYCFLAGS} -o $@ util/sample_image_colors.o ${OD}/png_utils.o ${PNGLIBS}

$(OD)/util/generate_solarsystem_positions.o:	${OD}/string-utils.o

util/generate_solarsystem_positions:	util/generate_solarsystem_positions.o ${OD}/string-utils.o ${OD}/stacktrace.o
	$(CC) ${MYCFLAGS} -o $@ util/generate_solarsystem_positions.o ${OD}/string-utils.o ${OD}/stacktrace.o -lm

$(OD)/snis_net_transport.o:	snis_net_transport.h
$(OD)/snis_net_transport_web.o:	snis_net_transport.h
$(OD)/snis_main_loop.o:	snis_main_loop.h
$(OD)/snis_main_loop_web.o:	snis_main_loop.h
$(OD)/snis_asset_fetch.o:	snis_asset_fetch.h
$(OD)/snis_asset_fetch_web.o:	snis_asset_fetch.h
$(OD)/snis_speak.o:	snis_speak.h snis_bin_dir.h
$(OD)/snis_speak_web.o:	snis_speak.h
$(OD)/snis_web_ssgl.o:	snis_net_transport.h

$(OD)/mikktspace.o:	mikktspace/mikktspace.c Makefile ${ODT}
	$(Q)$(COMPILE)

$(OD)/ssgl_%.o:	ssgl/ssgl_%.c Makefile ${ODT}
	$(Q)$(COMPILE)

$(OD)/libssglclient.a:	$(patsubst %,$(OD)/%,${_SSGLWEBOBJS})
	$(Q)$(ECHO) '  ARCHIVE' $@ && emar -cr $@ $^

.PHONY:	web web-assets web-update-assets

ifeq (${WEB},0)
web web-assets web-update-assets:
	@echo "You probably meant: make WEB=1 $@" 1>&2
	@/bin/false
else
# The browser client: bin/web/snis_client.{html,js,wasm,data}
web:	bin/web/snis_client.js bin/web/snis_client.html

# snis_update_assets runs natively, so build it without WEB=1.
${WEB_ASSET_STAMP}:
	$(Q)$(MAKE) WEB=0 bin/snis_update_assets
	$(Q)$(ECHO) '  DOWNLOAD' ${WEB_ASSET_CACHE} && mkdir -p ${WEB_ASSET_CACHE} && \
		bin/snis_update_assets --force --destdir ${WEB_ASSET_CACHE} && touch $@

web-update-assets:
	rm -f ${WEB_ASSET_STAMP}
	$(Q)$(MAKE) ${WEB_ASSET_STAMP}

# The assets for the web client, put together the way the launcher's
# UPDATE ASSETS does it: the files in share/snis, with the downloaded assets
# laid over them.  Shaders and Lua scripts are code, and always come from this
# tree, even if the cache holds old copies (as one seeded from an installed
# asset directory would).  Only WEB_SOLARSYSTEM is bundled into
# snis_client.data.  The other solarsystems go in bin/web/solarsystems, each
# with a list of its files, and snis_asset_fetch_web.c fetches one when the
# server first moves the client there.
web-assets:	${WEB_ASSET_STAMP}
	$(Q)$(ECHO) '  ASSETS' $(OD)/web-assets && mkdir -p $(OD)/web-assets && \
		rsync -a --delete ${WEB_ASSET_EXCLUDES} share/snis/ $(OD)/web-assets/ && \
		rsync -a ${WEB_ASSET_EXCLUDES} --exclude=/shader-es --exclude=/luascripts \
			${WEB_ASSET_CACHE}/share/snis/ $(OD)/web-assets/
	$(Q)$(ECHO) '  ASSETS' bin/web/solarsystems && mkdir -p bin/web/solarsystems && \
		rsync -a --delete --exclude='/${WEB_SOLARSYSTEM}' --exclude='/*.files' \
			share/snis/solarsystems/ bin/web/solarsystems/ && \
		if [ -d ${WEB_ASSET_CACHE}/share/snis/solarsystems ]; then \
			rsync -a --exclude='/${WEB_SOLARSYSTEM}' \
				${WEB_ASSET_CACHE}/share/snis/solarsystems/ bin/web/solarsystems/; \
		fi && \
		for d in bin/web/solarsystems/*/; do \
			[ -d "$$d" ] || continue; \
			(cd $$d && find . -type f | sort) > $${d%/}.files; \
		done

bin/web/snis_client.js:	${CLIENTOBJS} ${SSGL} Makefile web-assets ${OPUSARCHIVE}
	@mkdir -p bin/web
	$(Q)$(ECHO) '  LINK' $@ && $(CC) ${MYCFLAGS} -o $@ ${CLIENTOBJS} ${SDLLIBS} ${LIBS} ${SNDLIBS} $(LDFLAGS) ${LIBOPUS} ${WEBLDFLAGS}

bin/web/snis_client.html:	snis_client_web.html
	@mkdir -p bin/web
	@cp $< $@
endif

$(OD)/snis_opcode_def.o:	snis_opcode_def.h snis_packet.h
$(OD)/rts_unit_data.o:	rts_unit_data.h
$(OD)/snis_bridge_update_packet.o:	snis_bridge_update_packet.h

# SDLCOMPILE because black_hole_lens.h includes graph_dev.h, for the lens struct it fills in,
# and graph_dev.h pulls in SDL.h.
$(OD)/black_hole_lens.o:	black_hole_lens.h graph_dev.h quat.h mathutils.h

# Breaking a ship's hull into the pieces it leaves: see mesh_fracture.h.
$(OD)/mesh_fracture.o:	mesh_fracture.h mesh.h mtwist.h

# Smoke, flame and sparks drawn as one mesh: see particle_batch.h.
$(OD)/particle_batch.o:	particle_batch.h mesh.h

# A ship's death, and what it is made of: see ship_death.h.  None of them draws anything, so plain
# COMPILE, clear of graph_dev.h and SDL.
$(OD)/ship_death.o:	ship_death.h
$(OD)/ship_death_fireball.o:	ship_death_fireball.h ship_death.h material.h
$(OD)/ship_death_shrapnel.o:	ship_death_shrapnel.h ship_death_fireball.h \
				ship_death.h particle_batch.h material.h
$(OD)/ship_death_wreck.o:	ship_death_wreck.h ship_death_fireball.h ship_death.h \
				mesh_fracture.h particle_batch.h material.h star_light.h
$(OD)/crater.o:	crater.h

bin/snis_server:	${SERVEROBJS} ${SSGL} Makefile ${BIN}
	$(Q)$(SERVERLINK)

bin/snis_client:	${CLIENTOBJS} ${SDLCLIENTOBJS} ${SSGL} Makefile ${BIN} ${OPUSARCHIVE}
	$(Q)$(CLIENTLINK)

bin/snis_multiverse:	${MULTIVERSEOBJS} ${SSGL} Makefile ${BIN}
	$(Q)$(MULTIVERSELINK)

ssgl/lsssgl ssgl/ssgl_server: ${SSGL}

bin/ssgl_server bin/lsssgl: ${SSGL} ${BIN}
	@cp ssgl/$(notdir $@) $@

bin/snis_text_to_speech.sh:	snis_text_to_speech.sh ${BIN}
	@cp snis_text_to_speech.sh bin/snis_text_to_speech.sh
	@chmod +x bin/snis_text_to_speech.sh

bin/mesh_viewer:	${SDLCLIENTOBJS} ${SSGL} Makefile ${BIN}
	$(Q)$(VIEWERLINK)

bin/shadow_lab:	${SHADOWLABOBJS} ${SSGL} Makefile ${BIN}
	$(Q)$(VIEWERLINK)

bin/star_light_preview:	${STARLIGHTPREVIEWOBJS} ${SSGL} Makefile ${BIN}
	$(Q)$(VIEWERLINK)

bin/earthlike:	${OD}/earthlike.o ${ELOBJS} Makefile ${BIN}
	$(Q)$(ELLINK)

util/mask_clouds:	util/mask_clouds.o ${MCOBJS} Makefile
	$(Q)$(MCLINK)

util/cloud-mask-normalmap:	util/cloud-mask-normalmap.o ${MCOBJS} Makefile
	$(Q)$(MCLINK)

$(OD)/starbase_metadata.o:	starbase_metadata.h
${OD}/infinite-taunt.o:	infinite-taunt.h

bin/infinite-taunt:	${OD}/infinite-taunt.o ${OD}/names.o ${OD}/mtwist.o ${OD}/string-utils.o ${OD}/stacktrace.o Makefile ${BIN}
	$(CC) -DTEST_TAUNT -o bin/infinite-taunt ${MYCFLAGS} ${OD}/mtwist.o infinite-taunt.c \
			${OD}/names.o ${OD}/string-utils.o ${OD}/stacktrace.o

bin/names:	names.c names.h ${OD}/mtwist.o ${BIN}
	$(CC) -DTEST_NAMES -o bin/names ${MYCFLAGS} ${OD}/mtwist.o names.c

$(OD)/ship_registration.o:	ship_registration.h
$(OD)/corporations.o:	corporations.h
$(OD)/snis_pull_down_menu.o:	snis_pull_down_menu.h
$(OD)/snis_sliders.o:	snis_sliders.h ui_colors.h
$(OD)/starmap_adjacency.o:	starmap_adjacency.h $(OD)/quat.o $(OD)/vec4.o

$(OD)/snis_voice_chat.o:	snis_voice_chat.c snis_voice_chat.h pthread_util.h wwviaudio.h ${OPUSARCHIVE}
	$(Q)$(COMPILE) ${VCHAT} ${OPUSCFLAGS}

$(OD)/replacement_assets.o:	replacement_assets.h
$(OD)/snis_asset_dir.o:	snis_asset_dir.h
$(OD)/snis_bin_dir.o:	snis_bin_dir.h

$(OD)/stl_parser.o:	stl_parser.c Makefile ${ODT}
	$(Q)$(COMPILE) ${SDLCFLAGS}

bin/stl_parser:	stl_parser.c $(OD)/matrix.o $(OD)/mesh.o $(OD)/mathutils.o $(OD)/quat.o $(OD)/mtwist.o \
		$(OD)/open-simplex-noise.o mikktspace/mikktspace.o \
		$(OD)/string-utils.o $(OD)/stacktrace.o Makefile ${BIN}
	$(CC) -DTEST_STL_PARSER ${MYCFLAGS} ${SDLCFLAGS} \
		-o bin/stl_parser stl_parser.c ${OD}/matrix.o ${OD}/mesh.o ${OD}/mathutils.o \
		${OD}/quat.o ${OD}/mtwist.o mikktspace/mikktspace.o ${OD}/string-utils.o ${OD}/open-simplex-noise.o \
		${OD}/stacktrace.o -lm $(LDFLAGS)

$(OD)/oriented_bounding_box.o:	oriented_bounding_box.h
$(OD)/shape_collision.o:	shape_collision.h
$(OD)/turret_aimer.o:	turret_aimer.h quat.h mathutils.h
$(OD)/arbitrary_spin.o:	arbitrary_spin.h
$(OD)/a_star.o:	a_star.h
$(OD)/a_star_test.o:	a_star.h

a_star_test:	a_star_test.o a_star.o Makefile
	$(CC) -g -o a_star_test a_star_test.c a_star.o -lm

$(OD)/elastic_collision.o:	elastic_collision.h
$(OD)/talking_stick.o:	talking_stick.h
$(OD)/transport_contract.o:	transport_contract.h

bin/test_transport_contract:	transport_contract.c transport_contract.h ${OD}/commodities.o \
				${OD}/names.o ${OD}/mtwist.o ${OD}/string-utils.o ${OD}/infinite-taunt.o \
				${OD}/stacktrace.o ${ODT} ${BIN}
	$(Q)$(ECHO) '  LINK' $@ && $(CC) ${MYCFLAGS} -DTEST_TRANSPORT_CONTRACT=1 \
			-o $@ transport_contract.c \
			${OD}/commodities.o ${OD}/names.o ${OD}/mtwist.o ${OD}/string-utils.o \
			${OD}/infinite-taunt.o ${OD}/stacktrace.o -lm

$(OD)/png_utils.o:	png_utils.c png_utils.h Makefile ${ODT}
	$(Q)$(COMPILE) ${PNGCFLAGS}

$(OD)/workqueue.o:	workqueue.h
$(OD)/string-utils.o:	string-utils.h stacktrace.h
$(OD)/read_menu_file.o:	read_menu_file.h string-utils.h
$(OD)/snis_client_forker.o:	snis_client_forker.h string-utils.h
$(OD)/snis_process_options.o:	snis_process_options.h string-utils.h
$(OD)/xdg_base_dir_spec.o:	xdg_base_dir_spec.h
$(OD)/snis_tweak.o:	snis_tweak.h

$(OD)/snis_client_debug.o: snis_debug.c snis_debug.h Makefile ${ODT}
	$(Q)$(SNISCLIENTDBGCOMPILE)

$(OD)/snis_xwindows_hacks.o:	snis_xwindows_hacks.c snis_xwindows_hacks.h Makefile ${ODT}
	$(Q)$(COMPILE) ${SDLCFLAGS}

$(OD)/snis_server_debug.o: snis_debug.c snis_debug.h Makefile ${ODT}
	$(Q)$(SNISSERVERDBGCOMPILE)

test_xdg_base_dir:	xdg_base_dir_spec.c xdg_base_dir_spec.h Makefile
	$(CC) -DTEST_XDG_BASE_DIR -o test_xdg_base_dir xdg_base_dir_spec.c

test_pronunciation:	pronunciation.c Makefile
	$(CC) -DTEST_PRONUNCIATION_FIXUP -o test_pronunciation pronunciation.c

$(OD)/planetary_ring_data.o:	planetary_ring_data.h mtwist.h png_utils.h

test_planetary_atmosphere:	planetary_atmosphere.c ${OD}/mtwist.o Makefile
	$(CC) ${CFLAGS} ${UBSANFLAG} ${ASANFLAG} ${BOUNDSFLAGS} -g -DTEST_PLANETARY_ATMOSPHERE_PROFILE -o test_planetary_atmosphere \
		planetary_atmosphere.c ${OD}/mtwist.o

$(OD)/key_value_parser.o:	key_value_parser.h

bin/test_key_value_parser:	key_value_parser.c key_value_parser.h Makefile ${BIN}
	$(Q)$(ECHO) '  LINK' $@ && $(CC) ${MYCFLAGS} -DTEST_KEY_VALUE_PARSER -o $@ key_value_parser.c

bin/test-matrix:	matrix.c Makefile ${BIN}
	$(Q)$(ECHO) '  LINK' $@ && $(CC) ${MYCFLAGS} -DTEST_MATRIX -o $@ matrix.c -lm

bin/test-mathutils:	mathutils.c mathutils.h Makefile ${BIN} ${OD}/mtwist.o
	$(Q)$(ECHO) '  LINK' $@ && $(CC) ${MYCFLAGS} -DTEST_MATHUTILS -o $@ mathutils.c ${OD}/mtwist.o -lm

bin/test_stringutils:	test_stringutils.c Makefile ${BIN} ${ODT} ${OD}/string-utils.o ${OD}/stacktrace.o
	$(Q)$(ECHO) '  LINK' $@ && $(CC) ${MYCFLAGS} -o $@ test_stringutils.c ${OD}/string-utils.o ${OD}/stacktrace.o

bin/test-space-partition:	space-part.c Makefile ${BIN}
	$(Q)$(ECHO) '  LINK' $@ && $(CC) ${MYCFLAGS} -DTEST_SPACE_PARTITION -o $@ space-part.c -lm

$(OD)/snis_hash.o:	snis_hash.h

test_snis_crypt:	snis_hash.c snis_hash.h
	$(CC) -DTEST_SNIS_CRYPT -o test_snis_crypt snis_hash.c ${CRYPTLIBS}

bin/test_marshal:	snis_marshal.c snis_marshal.h ${OD}/stacktrace.o ${OD}/string-utils.o Makefile ${BIN}
	$(Q)$(ECHO) '  LINK' $@ && $(CC) ${MYCFLAGS} -DTEST_MARSHAL -o $@ ${OD}/stacktrace.o snis_marshal.c ${OD}/string-utils.o -lm

$(OD)/snis_nl.o:	snis_nl.h

snis_nl:	snis_nl.o ${OD}/string-utils.o ${OD}/spelled_numbers.o
	$(CC) ${BOUNDSFLAGS} -g -DTEST_NL -o snis_nl ${OD}/string-utils.o ${OD}/spelled_numbers.o snis_nl.c

$(OD)/spelled_numbers.o:	spelled_numbers.h
$(OD)/scipher.o:	scipher.h mathutils.h

test_scipher:	scipher.c scipher.h ${OD}/mathutils.o ${OD}/mtwist.o
	$(CC) -g -fsanitize=address -DTEST_SCIPHER -o test_scipher scipher.c ${OD}/mathutils.o ${OD}/mtwist.o -lm

spelled_numbers:	spelled_numbers.c
	$(CC) -g -DSPELLED_NUMBERS_TEST_CASE -o spelled_numbers spelled_numbers.c -lm

ifeq (${WEB},0)
${SSGL}:
	(cd ssgl && ${MAKE} )
endif

mikktspace/mikktspace.o:
	(cd mikktspace && ${MAKE} )

mostly-clean:
	rm -f ${SERVEROBJS} ${CLIENTOBJS} ${MULTIVERSEOBJS} ${SDLCLIENTOBJS} ${SSGL} \
	${BINPROGS} ${UTILPROGS} ${TESTPROGS} ${FUZZPROGS} ${ELOBJS} \
	stl_parser snis_limited_client.c \
	test-space-partition snis_test_audio.o snis_test_audio joystick_test local_termios2.h \
	bin/names bin/infinite-taunt bin/stl_parser \
	bin/test-obj-parser \
	bin/snis-device-io-sample-1 bin/check-endianness \
	${OD}/*.o ${ODT} \
	bin/device-io-sample-1 bin/print_ship_attributes bin/snis_test_audio bin/test_crater \
	bin/test_snis_dmx test_scipher wwviaudio_basic_test \
	wwviaudio_recording_test a_star_test a_star_test.o a_star.o test_xdg_base_dir test_pronunciation \
	test_planetary_atmosphere test_snis_crypt snis_nl spelled_numbers \
	$(filter %.gz,${MAN1PAGES} ${MAN6PAGES}) \
	bin/yoke-test-program build_info.h ${METAINFOFILE} \
	solarsystem_config_test bin/check-endianness.wasm
	rm -fr bin/web
	rm -f ${BIN}
	rm -fr opus-1.3.1
	rm -f libopus.a libopus-web.a
	rm -fr opus-web-build
	if [ -x bin ]; then rmdir bin; fi
	# rm -fr ${OD}
	# Value of OD depends on if WEB=1, either object_files, or object_files_web,
	# but for cleaning, nuke both.
	rm -fr object_files object_files_web
	( cd ssgl && ${MAKE} clean )
	( cd mikktspace && ${MAKE} clean )

bin/test-quat:	test-quat.c ${OD}/quat.o ${OD}/matrix.o ${OD}/mathutils.o ${OD}/mtwist.o Makefile ${BIN}
	$(Q)$(ECHO) '  LINK' $@ && $(CC) ${MYCFLAGS} -o $@ test-quat.c ${OD}/quat.o ${OD}/matrix.o ${OD}/mathutils.o ${OD}/mtwist.o -lm

bin/test-fleet:	fleet.c fleet.h ${OD}/quat.o ${OD}/mathutils.o ${OD}/mtwist.o Makefile ${BIN}
	$(Q)$(ECHO) '  LINK' $@ && $(CC) ${MYCFLAGS} -DTESTFLEET=1 -o $@ fleet.c ${OD}/mathutils.o ${OD}/quat.o ${OD}/mtwist.o -lm

bin/test-mtwist: ${OD}/mtwist.o test-mtwist.c Makefile ${BIN}
	$(Q)$(ECHO) '  LINK' $@ && $(CC) ${MYCFLAGS} -o $@ ${OD}/mtwist.o test-mtwist.c

$(OD)/snis-device-io.o:	snis-device-io.h snis-device-io.c Makefile ${ODT}
	$(CC) -Wall -Wextra --pedantic -pthread -c -o ${OD}/snis-device-io.o snis-device-io.c

bin/device-io-sample-1:	device-io-sample-1.c ${OD}/snis-device-io.o ${OD}/string-utils.o ${BIN}
	$(CC) -Wall -Wextra --pedantic -pthread -o bin/device-io-sample-1 ${OD}/string-utils.o ${OD}/snis-device-io.o \
			device-io-sample-1.c

bin/yoke-test-program:	yoke-test-program.c ${OD}/snis-device-io.o ${OD}/string-utils.o \
	${SSGL} ${BIN} ${OD}/pthread_util.o
	$(CC) -Wall -Wextra --pedantic -pthread -o bin/yoke-test-program ${OD}/string-utils.o \
	${OD}/snis-device-io.o ${OD}/pthread_util.o yoke-test-program.c -Lssgl -lssglclient

bin/snis_arduino: snis_arduino.c ${OD}/snis-device-io.o ${OD}/string-utils.o ${OD}/stacktrace.o ${BIN}
	$(CC) ${ASANFLAG} ${UBSANFLAG} -Wall -Wextra --pedantic -pthread -o bin/snis_arduino \
			${OD}/snis-device-io.o ${OD}/string-utils.o ${OD}/stacktrace.o snis_arduino.c

${OD}/snis-console.o:	snis-console.c string-utils.h ${ODT}
	$(CC) ${ASANFLAG} ${UBSANFLAG} -Wall -Wextra --pedantic -pthread ${NCURSESCFLAGS} \
		-c -o ${OD}/snis-console.o snis-console.c

bin/snis-console:	${OD}/snis-console.o ${OD}/string-utils.o ${OD}/stacktrace.o ${BIN}
	$(CC) -pthread  ${NCURSESCFLAGS} -o bin/snis-console ${OD}/snis-console.o ${OD}/string-utils.o \
		${OD}/stacktrace.o ${NCURSESLIBS}

$(OD)/nonuniform_random_sampler.o:	nonuniform_random_sampler.h

bin/test_nonuniform_random_sampler:	nonuniform_random_sampler.c ${OD}/mathutils.o ${OD}/mtwist.o ${BIN}
	$(Q)$(ECHO) '  LINK' $@ && $(CC) ${MYCFLAGS} -DTEST_NONUNIFORM_SAMPLER -o $@ nonuniform_random_sampler.c ${OD}/mtwist.o ${OD}/mathutils.o -lm

bin/test-commodities:	commodities.c commodities.h Makefile ${OD}/string-utils.o ${BIN} ${OD}/stacktrace.o
	$(Q)$(ECHO) '  LINK' $@ && $(CC) ${MYCFLAGS} -DTESTCOMMODITIES=1 -o $@ commodities.c ${OD}/string-utils.o ${OD}/stacktrace.o

bin/test-obj-parser:	test-obj-parser.c mikktspace/mikktspace.o ${OD}/string-utils.o ${OD}/stl_parser.o ${OD}/mesh.o \
		${OD}/mtwist.o ${OD}/mathutils.o ${OD}/matrix.o ${OD}/quat.o ${OD}/open-simplex-noise.o ${OD}/stacktrace.o Makefile ${BIN}
	$(CC) ${BOUNDSFLAGS} -o bin/test-obj-parser mikktspace/mikktspace.o ${OD}/string-utils.o ${OD}/stl_parser.o ${OD}/mtwist.o \
		${OD}/mathutils.o ${OD}/matrix.o ${OD}/mesh.o ${OD}/quat.o ${OD}/open-simplex-noise.o ${OD}/stacktrace.o \
		test-obj-parser.c -lm

.PHONY:	test
test:	${TESTPROGS}
	@for t in ${TESTPROGS}; do \
		echo "  TEST" $$t; \
		if [ "${V}" = "1" ]; then \
			./$$t || exit 1; \
		else \
			./$$t > /dev/null || exit 1; \
		fi; \
	done

MESH_FRACTURE_TEST_OBJS=${OD}/mesh_fracture.o ${OD}/mesh.o ${OD}/mtwist.o ${OD}/mathutils.o ${OD}/quat.o \
	${OD}/matrix.o ${OD}/graph_dev_mesh_stub.o ${OD}/open-simplex-noise.o mikktspace/mikktspace.o

bin/test_mesh_fracture:	test_mesh_fracture.c ${MESH_FRACTURE_TEST_OBJS} Makefile ${BIN}
	$(Q)$(ECHO) '  LINK' $@ && $(CC) ${MYCFLAGS} -o $@ test_mesh_fracture.c ${MESH_FRACTURE_TEST_OBJS} -lm

bin/test_wled:	test_wled.c ${OD}/snis_wled.o Makefile ${BIN}
	$(Q)$(ECHO) '  LINK' $@ && $(CC) ${MYCFLAGS} -o $@ test_wled.c ${OD}/snis_wled.o

bin/test_solarsystem_config:	test_solarsystem_config.c ${OD}/solarsystem_config.o ${OD}/string-utils.o ${OD}/stacktrace.o ${BIN}
	$(Q)$(ECHO) '  LINK' $@ && $(CC) ${MYCFLAGS} -o $@ $< ${OD}/solarsystem_config.o ${OD}/string-utils.o ${OD}/stacktrace.o

bin/test_crater:	$(OD)/test_crater.o $(OD)/crater.o $(OD)/mathutils.o $(OD)/mtwist.o ${OD}/png_utils.o ${BIN}
	$(CC) ${BOUNDSFLAGS} -o $@ ${PNGCFLAGS} $(OD)/test_crater.o $(OD)/crater.o $(OD)/mtwist.o ${OD}/png_utils.o ${PNGLIBS} $(OD)/mathutils.o -lm

${MANSRCDIR}/%.gz:	${MANSRCDIR}/%
	gzip -9 - < $< > $@

bin/print_ship_attributes:	snis_entity_key_value_specification.h $(OD)/key_value_parser.o ${BIN}
	$(CC) -o bin/print_ship_attributes print_ship_attributes.c $(OD)/key_value_parser.o

local_termios2.h:	termios2.h
	$(Q)./check_for_termios2.sh

$(OD)/snis_dmx.o:	snis_dmx.h local_termios2.h
$(OD)/snis_wled.o:	snis_wled.h

bin/test_snis_dmx:	test_snis_dmx.c ${OD}/snis_dmx.o ${BIN}
	$(Q)$(CC) -pthread -o bin/test_snis_dmx test_snis_dmx.c ${OD}/snis_dmx.o

$(OD)/snis_test_audio.o:	${SNDOBJS} ${OGGOBJ}

bin/snis_test_audio:	${OD}/snis_test_audio.o ${SNDLIBS} Makefile ${BIN} ${OD}/mathutils.o ${OD}/mtwist.o
	$(CC) ${MYCFLAGS} -o bin/snis_test_audio ${OD}/snis_test_audio.o ${SNDOBJS} ${OGGOBJ} ${SNDLIBS} ${OD}/mathutils.o ${OD}/mtwist.o -lm -lbsd

install:	${BINPROGS} ${MAN1PAGES} ${MAN6PAGES} ${SSGL} ${METAINFOFILE}
	@# First check that PREFIX is sane, and esp. that it's not pointed at source
	@mkdir -p ${DESTDIR}${PREFIX}
	@touch ${DESTDIR}${PREFIX}/.canary-in-the-coal-mine.canary
	@if [ -f .canary-in-the-coal-mine.canary -o "${DESTDIR}" = "" -a "${PREFIX}" = "."  ] ; then \
		echo 1>&2 ; \
		echo "DESTDIR/PREFIX is ${DESTDIR}${PREFIX} -- cannot install here" 1>&2 ; \
		echo "Try: make PREFIX=/usr/local ; make PREFIX=/usr/local install" 1>&2  ; \
		echo 1>&2 ; \
		rm -f ${DESTDIR}${PREFIX}/.canary-in-the-coal-mine.canary ; \
		exit 1 ; \
	fi
	@ rm -f ${DESTDIR}${PREFIX}/.canary-in-the-coal-mine.canary
	mkdir -p ${DESTDIR}${PREFIX}/bin
	for x in ${BINPROGS} ; do \
		${INSTALL} -m 755 $$x \
				${DESTDIR}${PREFIX}/bin; \
	done
	${INSTALL} -m 755 bin/snis_launcher ${DESTDIR}${PREFIX}/bin
	${INSTALL} -m 755 bin/update_assets_from_launcher.sh ${DESTDIR}${PREFIX}/bin
	# for d in ${MATERIALDIR} ${LUASCRIPTDIR}/UTIL ${LUASCRIPTDIR}/TEST \
	#	${LUASCRIPTDIR}/MISSIONS ${LUASCRIPTDIR}/MISSIONS/lib ${SHADERDIR} ${SOUNDDIR} \
	#	${TEXTUREDIR} ${MODELDIR}/wombat ${SHADERDIR} ; do \
	#	mkdir -p $$d ; \
	# done
	# ${INSTALL} -m 644 ${CONFIGFILES} ${CONFIGFILEDIR}
	# ${INSTALL} -m 644 ${SOUNDFILES} ${SOUNDDIR}
	# ${INSTALL} -m 644 ${TEXTUREFILES} ${TEXTUREDIR}
	# ${INSTALL} -m 644  ${LUASCRIPTS} ${LUASCRIPTDIR}
	# ${INSTALL} -m 644  ${LUAUTILS} ${LUASCRIPTDIR}/UTIL
	# ${INSTALL} -m 644  ${LUAMISSIONS} ${LUASCRIPTDIR}/MISSIONS
	# ${INSTALL} -m 644  ${LUASRCLIBS} ${LUASCRIPTDIR}/MISSIONS/lib
	# ${INSTALL} -m 644  ${LUATESTS} ${LUASCRIPTDIR}/TEST
	# ${INSTALL} -m 644  ${MATERIALFILES} ${MATERIALDIR}
	# ${INSTALL} -m 644  ${MODELS} ${MODELDIR}
	# mkdir -p ${SOLARSYSTEMDIR}/default
	# ${INSTALL} -m 644 ${SOLARSYSTEMSRCDIR}/default/assets.txt ${SOLARSYSTEMDIR}/default
	# for d in dreadknight disruptor conqueror enforcer starbase starbase2 cargocontainer research-vessel ; do \
	#	mkdir -p ${MODELDIR}/$$d ; \
	#	${INSTALL} -m 644 ${MODELSRCDIR}/$$d/$$d.mtl ${MODELDIR}/$$d ; \
	#	cp ${MODELSRCDIR}/$$d/$$d.obj ${MODELDIR}/$$d ; \
	#	cp ${MODELSRCDIR}/$$d/$$d.png ${MODELDIR}/$$d ; \
	#	if [ -f ${MODELSRCDIR}/$$d/$$d"lights.png" ] ; then \
	#		cp ${MODELSRCDIR}/$$d/$$d"lights.png" ${MODELDIR}/$$d ; \
	#	fi ; \
	#	if [ -f ${MODELSRCDIR}/$$d/$$d"-lighting.png" ] ; then \
	#		cp ${MODELSRCDIR}/$$d/$$d"-lighting.png" ${MODELDIR}/$$d ; \
	#	fi ; \
	#done
	#${INSTALL} -m 644 ${MODELSRCDIR}/dreadknight/dreadknight-exhaust-plumes.h ${MODELDIR}/dreadknight
	#${INSTALL} -m 644 ${MODELSRCDIR}/wombat/snis3006lights.png ${MODELDIR}/wombat
	#${INSTALL} -m 644 ${MODELSRCDIR}/wombat/snis3006.mtl ${MODELDIR}/wombat
	#${INSTALL} -m 644 ${MODELSRCDIR}/wombat/snis3006.obj ${MODELDIR}/wombat
	#${INSTALL} -m 644 ${MODELSRCDIR}/wombat/snis3006.png ${MODELDIR}/wombat
	#${INSTALL} -m 644 ${DOCKING_PORT_FILES} ${MODELDIR}
	#${INSTALL} -m 644 ${SCAD_PARAMS_FILES} ${MODELDIR}
	#${INSTALL} -m 644 ${SHADERS} ${SHADERDIR}
	mkdir -p ${MAN6DIR}
	mkdir -p ${MAN1DIR}
	${INSTALL} -m 644 ${MAN1PAGES} ${MAN1DIR}
	${INSTALL} -m 644 ${MAN6PAGES} ${MAN6DIR}
	mkdir -p ${DESKTOPDIR}
	${INSTALL} -m 644 ${DESKTOPFILES} ${DESKTOPDIR}
	grep -v '^Icon=' < ${DESKTOPSRCDIR}/io.github.smcameron.space-nerds-in-space.desktop > ${DESKTOPDIR}/io.github.smcameron.space-nerds-in-space.desktop
	echo "Icon=${DESKTOPDIR}/io.github.smcameron.space-nerds-in-space.svg" >> ${DESKTOPDIR}/io.github.smcameron.space-nerds-in-space.desktop
	mkdir -p ${METAINFODIR}
	${INSTALL} -m 644 ${METAINFOFILE} ${METAINFODIR}
	${UPDATE_DESKTOP}
	mkdir -p ${DESTDIR}${PREFIX}/share/snis
	bin/snis_update_assets --force --destdir ${DESTDIR}${PREFIX} --srcdir ./share/snis

uninstall:
	@# check that PREFIX is sane
	@touch ${DESTDIR}${PREFIX}/.canary-in-the-coal-mine.canary
	@if [ -f .canary-in-the-coal-mine.canary ] ; then \
		echo 1>&2 ; \
		echo "DESTDIR/PREFIX is ${DESTDIR}${PREFIX} -- cannot uninstall here" 1>&2 ; \
		echo "Try: make PREFIX=/usr/local uninstall" 1>&2  ; \
		echo 1>&2 ; \
		rm -f ${DESTDIR}${PREFIX}/.canary-in-the-coal-mine.canary ; \
		exit 1 ; \
	fi
	@rm -f ${DESTDIR}${PREFIX}/.canary-in-the-coal-mine.canary
	if [ ! -d "${DESTDIR}${PREFIX}" ] ; then \
		echo "DESTDIR/PREFIX is not a directory." 1>&2 ;\
		exit 1 ;\
	fi
	for x in ${BINPROGS} ; do \
		rm -f ${DESTDIR}${PREFIX}/$$x ; \
	done
	rm -fr ${DESTDIR}${PREFIX}/share/snis
	rm -f ${MAN6DIR}/snis_client.6.gz ${MAN6DIR}/snis_client.6
	rm -f ${MAN6DIR}/snis_server.6.gz ${MAN6DIR}/snis_server.6
	rm -f ${MAN1DIR}/earthlike.1.gz ${MAN1DIR}/earthlike.1
	rm -f ${MAN6DIR}/ssgl_server.6
	rm -f ${MAN1DIR}/snis_test_audio.1.gz
	rm -f ${MAN6DIR}/snis_multiverse.6
	rm -f ${MAN6DIR}/snis_text_to_speech.sh.6
	rm -f ${MAN6DIR}/snis_update_assets.6
	rm -f ${MAN1DIR}/lsssgl.1
	rm -f ${DESKTOPDIR}/io.github.smcameron.space-nerds-in-space.desktop
	rm -f ${DESKTOPDIR}/io.github.smcameron.space-nerds-in-space.svg
	rm -f ${METAINFODIR}/io.github.smcameron.space-nerds-in-space.metainfo.xml
	${UPDATE_DESKTOP}

clean:	mostly-clean
	rm -f ${MODELS}
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

bin/snis_web_gateway:	snis_web_gateway.c ${SSGL} Makefile
	$(Q)$(ECHO) '  COMPILE' $< && $(CC) ${MYCFLAGS} -o $@ snis_web_gateway.c ${SSGL} -lssl -lcrypto -lpthread

bin/snis_update_assets:	util/snis_update_assets.c ${OD}/string-utils.o ${OD}/stacktrace.o util/progress_image.h
	$(Q)$(CC) ${MYCFLAGS} ${SDLCFLAGS} -o bin/snis_update_assets util/snis_update_assets.c \
		${OD}/string-utils.o ${OD}/stacktrace.o ${SDLLIBS} -lcrypto -lcurl

build_info.h: bin/check-endianness snis.h gather_build_info Makefile
	@echo "  GATHER BUILD INFO"
	$(Q)@./gather_build_info ${VCHAT} > build_info.h

cppcheck:
	cppcheck --force .

scan-build:
	make mostly-clean
	rm -fr /tmp/snis-scan-build-output
	util/run-scan-build /tmp/snis-scan-build-output
	xdg-open /tmp/snis-scan-build-output/*/index.html

# opus stuff for voice chat
opus-1.3.1.tar.gz:
	wget https://archive.mozilla.org/pub/opus/opus-1.3.1.tar.gz
	md5sum opus-1.3.1.tar.gz | grep d7c07db796d21c9cf1861e0c2b0c0617

opus-1.3.1:	opus-1.3.1.tar.gz
	tar xzf opus-1.3.1.tar.gz

libopus.a:	opus-1.3.1
	(cd opus-1.3.1 && ./configure && make && cp ./.libs/libopus.a ..)

# Built outside opus-1.3.1, so that the native build above can still use it.
# -pthread because the web client is threaded, and wasm cannot mix the two.
libopus-web.a:	opus-1.3.1
	rm -rf opus-web-build && mkdir opus-web-build
	(cd opus-web-build && emconfigure ../opus-1.3.1/configure CFLAGS="-O2 -pthread" \
		--disable-extra-programs --disable-doc --disable-rtcd --disable-intrinsics \
		--disable-stack-protector && emmake $(MAKE) && cp .libs/libopus.a ../libopus-web.a)

FUZZ_COMPILE=afl-clang-fast -g3 -fsanitize=address,undefined $< -lm -o $@

fuzz_obj_parser:	fuzz_obj_parser.c
	$(FUZZ_COMPILE) -I/usr/include/SDL2

fuzz_%:	fuzz_%.c
	$(FUZZ_COMPILE)

fuzz_process_manifest:	fuzz_process_manifest.c util/snis_update_assets.c Makefile
	$(FUZZ_COMPILE) -I/usr/include/SDL2 -lcrypto

run-fuzz-obj-parser:	fuzz_obj_parser put-cpu-in-hi-performance-mode
	/bin/rm -fr fuzz.out
	afl-fuzz -T "Fuzzing read_obj_parser" -i share/snis/models/cargocontainer \
		-o fuzz.out -- ./fuzz_obj_parser obj

run-fuzz-stl-parser:	fuzz_obj_parser put-cpu-in-hi-performance-mode
	/bin/rm -fr fuzz.out
	afl-fuzz -T "Fuzzing read_stl_parser" -i fuzztests/read_stl_file \
		-o fuzz.out -- ./fuzz_obj_parser stl

run-fuzz-snis-read-ship-types:	fuzz_snis_read_ship_types
run-fuzz-solarsystem-asset-spec-read:	fuzz_solarsystem_asset_spec_read
run-fuzz-read-thrust-attachments:	fuzz_read_thrust_attachments
run-fuzz-process_manifest:	fuzz_process_manifest
run-fuzz-read-joystick-config:	fuzz_read_joystick_config
run-fuzz-read-commodities:	fuzz_read_commodities
run-fuzz-read-starbase-metadata:	fuzz_read_starbase_metadata
run-fuzz-read-docking-port-attachments:	fuzz_read_docking_port_attachments

RUN_FUZZ_TARGETS=run-fuzz-snis-read-ship-types run-fuzz-solarsystem-asset-spec-read \
	run-fuzz-read-thrust-attachments run-fuzz-process_manifest \
	run-fuzz-read-joystick-config run-fuzz-read-commodities \
	run-fuzz-read-starbase-metadata run-fuzz-read-docking-port-attachments

${RUN_FUZZ_TARGETS}: put-cpu-in-hi-performance-mode
	/bin/rm -fr fuzz.out
	afl-fuzz -T "Fuzzing $(patsubst fuzz_%,%,$(filter fuzz_%,$^))" \
		-i fuzztests/$(patsubst fuzz_%,%,$(filter fuzz_%,$^)) \
		-o fuzz.out -- ./$(filter fuzz_%,$^)

put-cpu-in-hi-performance-mode:
	(cd /sys/devices/system/cpu && echo performance | sudo tee cpu*/cpufreq/scaling_governor)

${METAINFOFILE}:	${METAINFOFILE_TEMPLATE} snis_version.h FORCE
	@echo "  METAINFO"
	@./make_metainfo_file ${METAINFOFILE_TEMPLATE} > ${METAINFOFILE}

FORCE:

include Makefile.depend

#
# Copyright (c) 2010 Stephen M. Cameron 
# 
# Permission is hereby granted, free of charge, to any person
# obtaining a copy of this software and associated documentation
# files (the "Software"), to deal in the Software without
# restriction, including without limitation the rights to use,
# copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following
# conditions:
# 
# The above copyright notice and this permission notice shall be
# included in all copies or substantial portions of the Software.
# 
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
# EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
# OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
# NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
# HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
# WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
# OTHER DEALINGS IN THE SOFTWARE.
# 

VERSION=0.1

CC ?= gcc

USE_CUSTOM_STRLCPY=0

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

ifeq (${V},1)
Q=
ECHO=true
else
Q=@
ECHO=echo
endif

OSX=0

# -lrt is needed for clock_gettime(), but only for glibc before 2.17
# LINKFLAGS=-lrt
# LRTFLAGS=-lrt

COMPILE=$(CC) ${MYCFLAGS} -c -o $@ $< && $(ECHO) '  COMPILE' $<

MYCFLAGS=${DEBUGFLAG} ${OPTIMIZEFLAG} --pedantic -Wall -Wvla ${STOP_ON_WARN} -pthread ${LRTFLAGS} $(CFLAGS) \
	-DUSE_CUSTOM_STRLCPY=${USE_CUSTOM_STRLCPY}

SERVEROBJS=ssgl_sanitize.o ssgl_sleep.o \
	ssgl_socket_io.o ssgl_log.o ssgl_get_primary_host_ip_addr.o ssgl_get_gamelobby_port.o

ARCHIVEOBJS=ssgl_sleep.o ssgl_sanitize.o ssgl_recv_game_servers.o \
		ssgl_connect_to_lobby.o ssgl_socket_io.o ssgl_get_primary_host_ip_addr.o \
		ssgl_register_gameserver.o ssgl_log.o ssgl_register_for_bcast_packet.o \
		ssgl_get_gamelobby_port.o

LBSD=-lbsd
ifeq (${USE_CUSTOM_STRLCPY}, 1)
	ARCHIVEOBJS += ssgl_string.o
	SERVEROBJS += ssgl_string.o
	LBSD=
endif

LINKSERVER=$(CC) ${MYCFLAGS} ${LINKFLAGS} $< ${SERVEROBJS} -o $@ $(LDFLAGS) && $(ECHO) '  LINK' $<
LINKCLIENT=$(CC) ${MYCFLAGS} -L. -o $@ $< -lssglclient ${LBSD} $(LDFLAGS) && $(ECHO) '  LINK' $<

AR ?= ar
ARCHIVE=$(AR) -cr $@ ${ARCHIVEOBJS} && $(ECHO) '  ARCHIVE' $@

SRCS = ssgl_connect_to_lobby.c \
	ssgl_gameclient_example.c \
	ssgl_gameserver_example.c \
	ssgl_get_primary_host_ip_addr.c \
	ssgl_recv_game_servers.c \
	ssgl_register_gameserver.c \
	ssgl_sanitize.c \
	ssgl_server.c \
	ssgl_sleep.c \
	ssgl_string.c \
	ssgl_log.c \
	ssgl_socket_io.c \
	ssgl_connect_to_lobby.h \
	ssgl.h \
	ssgl_protocol_id.h \
	ssgl_sanitize.h \
	ssgl_socket_io.h \
	ssgl_string.h

TXTFILES = AUTHORS COPYING LICENSE README

all:	ssgl_server ssgl_gameclient_example ssgl_gameserver_example lsssgl

libssglclient.a:	${ARCHIVEOBJS}
	$(Q)$(ARCHIVE)

ssgl_log.o:	ssgl_log.c ssgl_log.h
	$(Q)$(COMPILE)

ssgl_get_gamelobby_port.o:	ssgl_get_gamelobby_port.c ssgl_get_gamelobby_port.h
	$(Q)$(COMPILE)

ssgl_sleep.o:	ssgl_sleep.c
	$(Q)$(COMPILE)

ssgl_socket_io.o:	ssgl_socket_io.c
	$(Q)$(COMPILE)

ssgl_sanitize.o:	ssgl_sanitize.c ssgl_sanitize.h
	$(Q)$(COMPILE)

ssgl_recv_game_servers.o:	ssgl_recv_game_servers.c
	$(Q)$(COMPILE)

ssgl_register_gameserver.o:	ssgl_register_gameserver.c
	$(Q)$(COMPILE)

ssgl_get_primary_host_ip_addr.o:	ssgl_get_primary_host_ip_addr.c
	$(Q)$(COMPILE)

ssgl_connect_to_lobby.o:	ssgl_connect_to_lobby.c
	$(Q)$(COMPILE)

ssgl_string.o:	ssgl_string.c ssgl_string.h Makefile
	$(Q)$(COMPILE)

ssgl_register_for_bcast_packet.o: ssgl_register_for_bcast_packet.c
	$(Q)$(COMPILE)

ssgl_server.o:	ssgl_server.c ${SERVEROBJS}
	$(Q)$(COMPILE)

ssgl_server:	ssgl_server.o
	$(Q)$(LINKSERVER)

ssgl_gameclient_example.o:	ssgl_gameclient_example.c ssgl.h libssglclient.a
	$(Q)$(COMPILE)

ssgl_gameserver_example.o:	ssgl_gameserver_example.c ssgl.h libssglclient.a
	$(Q)$(COMPILE)

ssgl_gameclient_example:	ssgl_gameclient_example.o
	$(Q)$(LINKCLIENT)

lsssgl:	ssgl_gameclient_example
	cp ssgl_gameclient_example lsssgl

ssgl_gameserver_example:	ssgl_gameserver_example.o
	$(Q)$(LINKCLIENT)

tarball: ${SRCS} ${TXTFILES} Makefile 
	mkdir ssgl-${VERSION}
	cp ${SRCS} ${TXTFILES} Makefile ssgl-${VERSION}
	tar cvf ssgl-${VERSION}.tar ./ssgl-${VERSION}
	gzip ssgl-${VERSION}.tar
	rm -fr ./ssgl-${VERSION}

clean:
	rm -fr *.o ssgl_server ssgl_gameclient_example ssgl_gameserver_example libssglclient.a ssgl-${VERSION}.tar.gz lsssgl


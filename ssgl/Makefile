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

SRCS = ssgl_connect_to_lobby.c \
	ssgl_gameclient_example.c \
	ssgl_gameserver_example.c \
	ssgl_get_primary_host_ip_addr.c \
	ssgl_recv_game_servers.c \
	ssgl_register_gameserver.c \
	ssgl_sanitize.c \
	ssgl_server.c \
	ssgl_sleep.c \
	ssgl_log.c \
	ssgl_socket_io.c \
	ssgl_connect_to_lobby.h \
	ssgl.h \
	ssgl_protocol_id.h \
	ssgl_sanitize.h \
	ssgl_socket_io.h

TXTFILES = AUTHORS COPYING LICENSE README

all:	ssgl_server ssgl_gameclient_example ssgl_gameserver_example

libssglclient.a:	ssgl_sleep.o	ssgl_sanitize.o ssgl_recv_game_servers.o \
	ssgl_connect_to_lobby.o ssgl_socket_io.o ssgl_get_primary_host_ip_addr.o \
	ssgl_register_gameserver.o ssgl_log.o
	ar -cr libssglclient.a ssgl_sleep.o ssgl_sanitize.o ssgl_recv_game_servers.o \
		ssgl_connect_to_lobby.o ssgl_socket_io.o ssgl_get_primary_host_ip_addr.o \
		ssgl_register_gameserver.o ssgl_log.o

ssgl_log.o:	ssgl_log.c ssgl_log.h
	gcc -g --pedantic -Wall -Werror -pthread -c ssgl_log.c

ssgl_sleep.o:	ssgl_sleep.c
	gcc -g --pedantic -Wall -Werror -pthread -c ssgl_sleep.c

ssgl_socket_io.o:	ssgl_socket_io.c
	gcc -g --pedantic -Wall -Werror -pthread -c ssgl_socket_io.c

ssgl_sanitize.o:	ssgl_sanitize.c ssgl_sanitize.h
	gcc -g --pedantic -Wall -Werror -pthread -c ssgl_sanitize.c

ssgl_recv_game_servers.o:	ssgl_recv_game_servers.c
	gcc -g --pedantic -Wall -Werror -pthread -c ssgl_recv_game_servers.c

ssgl_register_gameserver.o:	ssgl_register_gameserver.c
	gcc -g --pedantic -Wall -Werror -pthread -c ssgl_register_gameserver.c

ssgl_get_primary_host_ip_addr.o:	ssgl_get_primary_host_ip_addr.c
	gcc -g --pedantic -Wall -Werror -pthread -c ssgl_get_primary_host_ip_addr.c

ssgl_connect_to_lobby.o:	ssgl_connect_to_lobby.c
	gcc -g --pedantic -Wall -Werror -pthread -c ssgl_connect_to_lobby.c

ssgl_server:	ssgl_server.c ssgl.h ssgl_sanitize.o ssgl_sleep.o ssgl_socket_io.o ssgl_log.o
	gcc -g --pedantic -Wall -Werror -pthread -lrt ssgl_sanitize.o ssgl_sleep.o \
			ssgl_socket_io.o ssgl_log.o -o ssgl_server ssgl_server.c

ssgl_gameclient_example:	ssgl.h ssgl_gameclient_example.c libssglclient.a
	gcc -g --pedantic -Wall -Werror -pthread -L. -o ssgl_gameclient_example ssgl_gameclient_example.c -lssglclient

ssgl_gameserver_example:	ssgl.h ssgl_gameserver_example.c libssglclient.a
	gcc -g --pedantic -Wall -Werror -pthread -L. -o ssgl_gameserver_example ssgl_gameserver_example.c -lssglclient

tarball: ${SRCS} ${TXTFILES} Makefile 
	mkdir ssgl-${VERSION}
	cp ${SRCS} ${TXTFILES} Makefile ssgl-${VERSION}
	tar cvf ssgl-${VERSION}.tar ./ssgl-${VERSION}
	gzip ssgl-${VERSION}.tar
	rm -fr ./ssgl-${VERSION}

clean:
	rm -fr *.o ssgl_server ssgl_gameclient_example ssgl_gameserver_example libssglclient.a ssgl-${VERSION}.tar.gz

/*

Copyright (c) 2010 Stephen M. Cameron 

Permission is hereby granted, free of charge, to any person
obtaining a copy of this software and associated documentation
files (the "Software"), to deal in the Software without
restriction, including without limitation the rights to use,
copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the
Software is furnished to do so, subject to the following
conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

 */

#ifndef __SSGL_CONNECT_TO_LOBBY_H__
#define __SSGL_CONNECT_TO_LOBBY_H__

#ifdef DEFINE_SSGL_CONNECT_TO_LOBBY_GLOBALS
#define GLOBAL
#else
#define GLOBAL extern
#endif

GLOBAL int ssgl_connect_to_lobby(char *ssgl_hostname);

GLOBAL int ssgl_gameserver_connect_to_lobby(char *ssgl_hostname);
GLOBAL int ssgl_gameclient_connect_to_lobby(char *ssgl_hostname);

/* If port is <= 0 or port is > 65535, then the default gamelobby port, or $SSGL_PORT
 * will be used. Otherwise the specified port will be used.  The port is in host byte
 * order here. */
GLOBAL int ssgl_gameserver_connect_to_lobby_port(char *ssgl_hostname, int port);
GLOBAL int ssgl_gameclient_connect_to_lobby_port(char *ssgl_hostname, int port);

#undef GLOBAL
#endif


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
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/types.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <arpa/inet.h> /* for ntohs */

#include "ssgl.h"
#include "ssgl_string.h"

int lsssglmode = 0;
char *programname = "ssgl_gameclient_example";

static void usage(void)
{
	fprintf(stderr, "%s: usage:\n   %s [lobby-hostname] [gametypefilter]\n",
		programname, programname);
	fprintf(stderr, "      if lobby-hostname is omitted, localhost is used\n");
	fprintf(stderr, "      if gametypefilter is omitted, '*' is used.\n");
	exit(1);
}

int main(int argc, char *argv[])
{
	struct ssgl_game_server *game_server = NULL;
	struct ssgl_client_filter filter;
	int game_server_count, rc, i;
	char *hostname, *gametype;
	int sock;

	if (strlen(argv[0]) >= 6) {
		char *cmd = argv[0] + strlen(argv[0]) - 6;
		if (strcmp(cmd, "lsssgl") == 0) {
			lsssglmode = 1;
			programname = "lsssgl";
		}
	}

	if (argc < 1)
		usage();

	if (argc >= 2) {
		if (strcmp(argv[1], "help") == 0 ||
			strcmp(argv[1], "-help") == 0 ||
			strcmp(argv[1], "--help") == 0)
			usage();
	}
	if (argc == 1) {
		hostname = "localhost";
		gametype = "*";
	}
	if (argc == 2) {
		hostname = argv[1];
		gametype = "*";
	}
	if (argc >= 3) {
		hostname = argv[1];
		gametype = argv[2];
	}

	sock = ssgl_gameclient_connect_to_lobby(hostname);
	if (sock < 0) {
		fprintf(stderr, "ssgl_connect_to_lobby failed: %s\n", strerror(errno));
		exit(1);
	}

	strlcpy(filter.game_type, gametype, sizeof(filter.game_type));
	printf("Filtering games of type '%s' on host '%s'\n", filter.game_type, hostname);
	do {
		rc = ssgl_recv_game_servers(sock, &game_server, &game_server_count, &filter);
		if (rc) {
			fprintf(stderr, "ssgl_recv_game_servers failed: %s\n", strerror(errno));
			break;
		}
		printf("%20s %15s %20s %15s %20s %10s\n",
			"IP addr/port", "Game Type", "Instance/Map", "Server Nickname", "Location", "Protocol");
		printf("---------------------------------------------------------------------------------------------------------\n");
		for (i = 0; i < game_server_count; i++) {
			unsigned char *x = (unsigned char *) &game_server[i].ipaddr;
			char ipaddr_and_port[100];
			sprintf(ipaddr_and_port, "%d.%d.%d.%d/%d", x[0], x[1], x[2], x[3],
					ntohs(game_server[i].port));
			printf("%20s %15s %20s %15s %20s %10s\n", ipaddr_and_port,
				game_server[i].game_type,
				game_server[i].game_instance,
				game_server[i].server_nickname,
				game_server[i].location,
				game_server[i].protocol_version);
				
		}
		printf("\n");	
		if (game_server_count > 0)
			free(game_server);
		if (!lsssglmode)
			ssgl_sleep(5);  /* just a thread safe sleep. */
	} while (!lsssglmode);
	return 0;
}


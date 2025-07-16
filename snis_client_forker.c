/*
	Copyright (C) 2025 Stephen M. Cameron
	Author: Stephen M. Cameron

	This file is part of Spacenerds In Space.

	Spacenerds in Space is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	Spacenerds in Space is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with Spacenerds in Space; if not, write to the Free Software
	Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "xdg_base_dir_spec.h"
#include "string-utils.h"
#include "snis_client_forker.h"
#include "snis_process_options.h"

static int pipe_to_forker_process;

static struct snis_process_options options;

/* All these are from snis_client.c */
extern struct xdg_base_context *xdg_base_ctx;
extern int pipe_to_splash_screen;

static char *get_executable_path(void)
{
	static char path[PATH_MAX];
	memset(path, 0, sizeof(path));
	if (readlink("/proc/self/exe", path, PATH_MAX) == -1)
		return NULL;
	return path;
}

static void fork_ssgl(void)
{
	char *executable_path = get_executable_path();

	if (!executable_path)
		return;

	char *ssgl_server = malloc(PATH_MAX * 2);
	snprintf(ssgl_server, PATH_MAX * 2, "%s", executable_path);
	dirname(ssgl_server);
	strcat(ssgl_server, "/ssgl_server");

	int rc = fork();
	if (!rc) { /* child process */
		execl(ssgl_server, ssgl_server, NULL);
		rc = write(2, "execl failed.\n", 14);
		(void) rc; /* shut clang scan-build up. */
		_exit(1);
	}
	free(ssgl_server);
}

static char **build_multiverse_argv(char *multiverse_server)
{
	int argc = 10;
	char **argv;

	if (options.snis_multiverse.autowrangle)
		argc++;
	if (options.snis_multiverse.allow_remote_networks)
		argc++;
	if (options.snis_multiverse.has_fixed_port_number)
		argc += 2;
	if (options.snis_server.has_port_range)
		argc += 2;

	argv = calloc(argc, sizeof(char *));

	int i = 0;

	argv[i++] = strdup(multiverse_server);
	if (options.snis_multiverse.autowrangle)
		argv[i++] = strdup("-a");
	if (options.snis_multiverse.allow_remote_networks)
		argv[i++] = strdup("--allow-remote-networks");
	if (options.snis_multiverse.has_fixed_port_number) {
		argv[i++] = strdup("-p");
		argv[i++] = strdup(options.snis_multiverse.port_number);
	}
	if (options.snis_server.has_port_range) {
		argv[i++] = strdup("--snis-server-portrange");
		argv[i++] = strdup(options.snis_server.port_range);
	}
	/* We don't need to pass snis_server.NAT_ghetto_mode to multiverse
	 * because any snis_servers that multiverse starts will be able to reach
	 * multiverse because they are running on the same host because they are
	 * child processes.
	 */

	argv[i++] = strdup("-l");
	argv[i++] = strdup(options.lobbyhost);
	argv[i++] = strdup("-n");
	argv[i++] = strdup(options.snis_multiverse.nickname);
	argv[i++] = strdup("-L");
	argv[i++] = strdup(options.snis_multiverse.location);
	argv[i++] = strdup("-e");
	argv[i++] = strdup(options.snis_multiverse.exempt);
	argv[i++] = NULL;

	return argv;
}

static char **build_snis_server_argv(char *server)
{
	int argc = 10;
	char **argv;

	if (options.snis_server.allow_remote_networks)
		argc++;
	if (options.snis_server.has_port_range)
		argc += 2;
	if (options.no_lobby)
		argc += 1;
	if (options.snis_server.NAT_ghetto_mode)
		argc += 1;
	if (options.snis_server.enable_enscript)
		argc += 1;

	argv = calloc(argc, sizeof(char *));

	int i = 0;

	argv[i++] = strdup(server);
	if (options.snis_server.allow_remote_networks)
		argv[i++] = strdup("--allow-remote-networks");
	if (options.snis_server.has_port_range &&
				port_range_formatted_correctly(options.snis_server.port_range)) {
		argv[i++] = strdup("--portrange");
		argv[i++] = strdup(options.snis_server.port_range);
	}
	if (options.no_lobby) {
		argv[i++] = strdup("--nolobby");
	}
	if (options.snis_server.NAT_ghetto_mode)
		argv[i++] = strdup("-g");
	if (options.snis_server.enable_enscript)
		argv[i++] = strdup("-e");

	argv[i++] = strdup("-l");
	argv[i++] = strdup(options.lobbyhost);
	argv[i++] = strdup("-L");
	argv[i++] = strdup(options.snis_server.location);
	argv[i++] = strdup("-m");
	argv[i++] = strdup(options.snis_server.multiverse_location);
	argv[i++] = strdup("-s");
	argv[i++] = strdup(options.snis_server.solarsystem);
	argv[i++] = NULL;

	return argv;
}


static void fork_multiverse(void)
{
	char *executable_path = get_executable_path();
	if (!executable_path)
		return;

	char *multiverse_server = malloc(PATH_MAX * 2);
	char *snis_bin_dir = malloc(PATH_MAX * 2);

	snprintf(multiverse_server, PATH_MAX * 2, "%s", executable_path);
	dirname(multiverse_server);

	/* This is a bit of a hack. Probably shouldn't need to use SNISBINDIR here.
	 * we have to cut off the trailing "/bin", because snis_multiverse adds it
	 * back on.
	 */
	snprintf(snis_bin_dir, PATH_MAX * 2, "%s", multiverse_server);
	ssize_t len = strlen(snis_bin_dir) - 4;
	if (len >= 0 && strcmp(&snis_bin_dir[len], "/bin") == 0)
		dirname(snis_bin_dir); /* cut off trailing /bin */
	setenv("SNISBINDIR", snis_bin_dir, 0);
	fprintf(stderr, "set SNISBINDIR to %s\n", snis_bin_dir);

	strcat(multiverse_server, "/snis_multiverse");
	int fd = xdg_base_open_for_overwrite(xdg_base_ctx, "snis_multiverse_log.txt");
	if (fd < 0) {
		fd = open("/dev/null", O_WRONLY, 0644);
	}

	fprintf(stderr, "Forking multiverse\n");
	fprintf(stderr, "%s\n", multiverse_server);

	char **mv_argv = build_multiverse_argv(multiverse_server);

	int rc = fork();
	if (!rc) { /* child process */
		close(pipe_to_forker_process);
		/* Set stderr and stdout to got to snis_multiverse_log.txt */
		(void) dup2(fd, 1);
		(void) dup2(fd, 2);
		execv(multiverse_server, mv_argv);
		rc = write(2, "execl failed.\n", 14);
		(void) rc; /* shut clang scan-build up. */
		_exit(1);
	}
	free_argv(mv_argv);
	free(multiverse_server);
	close(fd);
}

static void fork_snis_server(void)
{
	char *executable_path = get_executable_path();

	if (!executable_path)
		return;

	char *snis_server = malloc(PATH_MAX * 2);
	snprintf(snis_server, PATH_MAX * 2, "%s", executable_path);
	dirname(snis_server);
	strcat(snis_server, "/snis_server");
	int fd = xdg_base_open_for_overwrite(xdg_base_ctx, "snis_server_log.txt");
	if (fd < 0) {
		fd = open("/dev/null", O_WRONLY, 0644);
	}

	fprintf(stderr, "Forking snis_server\n");
	fprintf(stderr, "%s\n", snis_server);

	char **ss_argv = build_snis_server_argv(snis_server);

	int rc = fork();
	if (!rc) { /* child process */
		close(pipe_to_forker_process);
		/* Set stderr and stdout to got to snis_server_log.txt */
		(void) dup2(fd, 1);
		(void) dup2(fd, 2);
		execv(snis_server, ss_argv);
		rc = write(2, "execl failed.\n", 14);
		(void) rc; /* shut clang scan-build up. */
		_exit(1);
	}
	free_argv(ss_argv);
	free(snis_server);
	close(fd);
}

static void fork_snis_process_terminator(int clients_too)
{
	pid_t process_list[1024];
	pid_t my_pid;
	int npids = 0;
	#define SERVER_PATTERN "'/ssgl_server|/snis_multiverse|/snis_server'"
	#define ALL_SNIS_PATTERN "'/ssgl_server|/snis_multiverse|/snis_server|snis_client'"


	my_pid = getpid();

	fprintf(stderr, "snis_client: Terminating all SNIS server processes\n");
	FILE *f;

	if (clients_too)
		f = popen("ps ax | egrep " ALL_SNIS_PATTERN " | grep -v grep", "r");
	else
		f = popen("ps ax | egrep " SERVER_PATTERN " | grep -v grep", "r");
	if (!f) {
		fprintf(stderr, "snis_client: popen failed: %s\n", strerror(errno));
		return;
	}

	/* Seek ... */
	do {
		int pid, rc;
		char buffer[1024];
		char *c = fgets(buffer, sizeof(buffer), f);
		if (!c)
			break;
		rc = sscanf(buffer, " %d", &pid);
		if (rc != 1)
			continue;
		fprintf(stderr, "%s", buffer);
		process_list[npids++] = pid;
	} while (1);

	/* ... and Destroy!  \m/ Kill'em All \m/ */
	for (int i = 0; i < npids; i++) {
		if (process_list[i] != my_pid) { /* Save ourself for last */
			if (kill(process_list[i], SIGTERM)) {
				fprintf(stderr, "kill %d: %s\n", process_list[i], strerror(errno));
			}
		}
	}
	if (clients_too)
		exit(0); /* "There's one more chip."  Taps forehead. */
}

static void fork_restart_snis_client(char **saved_argv)
{
	char *executable_path = get_executable_path();

	if (!executable_path)
		return;

	int rc = fork();
	if (!rc) { /* child process */
		execv(executable_path, saved_argv);
		rc = write(2, "execv failed.\n", 14);
		(void) rc; /* shut clang scan-build up. */
		_exit(1);
	}
}

void fork_update_assets(int background_task, int local_only, int show_progress)
{
	char *executable_path = get_executable_path();
	char cmd[PATH_MAX * 2];
	char logfilename[PATH_MAX];

	if (!executable_path) {
		fprintf(stderr, "executable_path is NULL!, Cannot update assets\n");
		return;
	}

	char *update_assets = malloc(PATH_MAX * 2);
	snprintf(update_assets, PATH_MAX * 2, "%s", executable_path);
	dirname(update_assets);
	strcat(update_assets, "/update_assets_from_launcher.sh dontask");

#if 0
	snprintf(cmd, sizeof(cmd), "%s", update_assets);
	fprintf(stderr, "'%s'\n", cmd);

	snprintf(cmd, sizeof(cmd), "gnome-terminal %s -- %s || xterm -e %s %s%s",
		should_wait ? "--wait" : "", update_assets, update_assets,
		should_wait ? "|| " : "", should_wait ? update_assets : "");
#endif

	char *x = xdg_base_data_filename(xdg_base_ctx, "asset_download_log.txt",
				logfilename, (int) sizeof(logfilename));
	if (!x)
		strlcpy(logfilename, "/tmp/asset_download_log.txt", sizeof(logfilename));

	char *logdirname = strdup(logfilename);
	dirname(logdirname);
	struct stat dirstat;
	int rc = stat(logdirname, &dirstat);
	if (rc != 0 && errno == ENOENT) {
		rc = mkdir(logdirname, 0755);
		if (rc != 0) {
			fprintf(stderr, "Failed to create directory %s: %s\n",
				logdirname, strerror(errno));
			free(logdirname);
			free(update_assets);
			return;
		}
	}
	free(logdirname);

	snprintf(cmd, sizeof(cmd), "%s %s %s > %s 2>&1 %s", update_assets,
				local_only ? "localonly" : "",
				show_progress ? "showprogress" : "",
				logfilename, background_task ? "&" : "");

	fprintf(stderr, "Asset updating command: %s\n", cmd);
	errno = 0;
	rc = system(cmd);
#if 0
	/* Unfortunately, we cannot detect if gnome-terminal or xterm succeeded */
	/* Because even if they do succeed, they background themselves and we */
	/* can't get the status, so it appears that they failed even when they didn't. */
#endif
	(void) rc;
	/* TODO: could probably do better error reporting here. */
	free(update_assets);
}

static int forker_read_options_from_pipe(int pipefd, struct snis_process_options *process_options)
{
	int rc;
	int bytes_read, bytes_to_read;

	char *c = (char *) process_options;
	bytes_read = 0;
	bytes_to_read = sizeof(*process_options);

	do {
		rc = read(pipefd, c, bytes_to_read);
		if (rc < 0) {
			if (errno == EINTR)
				continue;
			fprintf(stderr, "snis_client forker process: error reading from pipe: %s\n", strerror(errno));
			fprintf(stderr, "exiting.\n");
			exit(1);
		}
		if (rc == 0) /* eof */
			exit(1);
		bytes_to_read -= rc;
		bytes_read += rc;
		c += rc;
	} while (bytes_to_read > 0);
	return 0;
}

/* An SDL2 program can't just fork/exec at any time, so we fork off this forker thing
 * *before* SDL2 is initialized, that way we can fork off new processes from here without
 * pissing off SDL2 and XWindows and getting everybody killed.
 *
 * We then can command this thing via a pipe.
 */
void forker_process_start(int *pipe_to_forker_process, char **saved_argv)
{
	int rc, pipefd[2];
	char ch;

	options = snis_process_options_default();

	memset(&options, 0, sizeof(options));

	rc = pipe(pipefd);
	if (rc) {
		fprintf(stderr, "snis_client: start_forker_process, pipe() failed: %s\n",
			strerror(errno));
	}
	rc = fork();
	if (!rc) { /* child process */
		close(pipefd[1]); /* close the write side of the pipe */
		close(pipe_to_splash_screen); /* inherited from parent */

		do {
			errno = 0;
			rc = read(pipefd[0], &ch, 1);
			switch (rc) {
			case 1:
				switch (ch) {
				case FORKER_START_SSGL:
					fork_ssgl();
					break;
				case FORKER_START_MULTIVERSE:
					fork_multiverse();
					break;
				case FORKER_START_SNIS_SERVER:
					fork_snis_server();
					break;
				case FORKER_KILL_EM_ALL:
					fork_snis_process_terminator(1);
					break;
				case FORKER_UPDATE_ASSETS:
					fork_update_assets(1, 0, 0);
					break;
				case FORKER_KILL_SERVERS:
					fork_snis_process_terminator(0);
					break;
				case FORKER_RESTART_SNIS_CLIENT:
					fork_restart_snis_client(saved_argv);
					break;
				case FORKER_UPDATE_PROCESS_OPTIONS:
					if (forker_read_options_from_pipe(pipefd[0], &options))
						goto exit_forker_process;
					break;
				case FORKER_QUIT:
				default:
					break;
				}
				continue;
			case -1:
				if (errno == EINTR)
					continue;
				goto exit_forker_process;
				break;
			default:
				goto exit_forker_process;
				break;
			}
		} while (1);
exit_forker_process:
		close(pipefd[0]);
		exit(0);
	}

	/* Parent process */
	close(pipefd[0]); /* close the read side of the pipe */
	*pipe_to_forker_process = pipefd[1];

}


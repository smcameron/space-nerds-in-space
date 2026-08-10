/*
 * snis-console.c
 *
 * A multithreaded ncurses-based console manager for monitoring UNIX domain
 * UDP sockets matching /tmp/snis-console.*
 *
 * Compilation:
 *   gcc -Wall -Wextra -pthread snis-console.c -o snis-console -lncurses
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <errno.h>
#include <ncurses.h>
#include <arpa/inet.h>

#include "string-utils.h"
#include "snis_cardinal_colors.h"

#define MAX_CONSOLES 128
#define MAX_MSG_LEN 256
#define MAX_LINES 1000
#define PREFIX "snis-console."
#define PREFIX_LEN 13

struct textline {
	char *text;
	int color_pair;
};

struct console {
	char name[256];          /* e.g., "snis-console.1234" */
	char socket_path[512];   /* e.g., "/tmp/snis-console.1234" */
	int sockfd;
	struct textline lines[MAX_LINES];
	int line_count;
	int scroll_offset; /* 0 = pinned to bottom/latest */
	int active;
};

static struct console consoles[MAX_CONSOLES];
static int active_count = 0;
static int current_idx = -1;
static int screen_dirty = 1;

static pthread_mutex_t data_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_t discovery_thread;
static int running = 1;

/* Forward declarations */
static void redraw_ui(void);
static void add_line(struct console *c, const char *msg, uint32_t color);

static int map_color_to_pair(uint32_t color)
{
	switch (color) {
	case BLACK:         return 1;
	case WHITE:         return 2;
	case YELLOW:
	case AMBER:
	case LIGHT_AMBER:   return 3;
	case RED:
	case ORANGE:
	case DARKRED:
	case ORANGERED:     return 4;
	case MAGENTA:       return 5;
	case DARKGREEN:
	case LIMEGREEN:     return 6;
	case DARKTURQUOISE: return 7;
	default:            return 0; /* Default ncurses color pair */
	}
}

static char *tail_end_of_name(char *n)
{
	char *i;
	for (i = n; *i != '\0'; i++) {
		if (*i == '.' && *(i + 1) != '\0')
			return i + 1;
	}
	return NULL;
}

static int still_running(void)
{
	int rc;
	pthread_mutex_lock(&data_mutex);
	rc = running;
	pthread_mutex_unlock(&data_mutex);
	return rc;
}

/* Thread that monitors /tmp for socket additions/removals */
void *discovery_loop(void *arg)
{
	(void)arg;

	while (still_running()) {
		DIR *dir = opendir("/tmp");
		if (!dir) {
			sleep(1);
			continue;
		}
		struct dirent *entry;
		int found_mask[MAX_CONSOLES] = {0};

		pthread_mutex_lock(&data_mutex);

		while ((entry = readdir(dir)) != NULL) {

			if (strncmp(entry->d_name, PREFIX, PREFIX_LEN) != 0)
				continue;

			char full_path[512];
			snprintf(full_path, sizeof(full_path), "/tmp/%s", entry->d_name);

			/* Check if it's already tracked */
			int known = 0;
			for (int i = 0; i < MAX_CONSOLES; i++) {
				if (consoles[i].active && strcmp(consoles[i].socket_path, full_path) == 0) {
					known = 1;
					found_mask[i] = 1;
					break;
				}
			}
			if (known)
				continue;

			/* Add new socket if space permits */
			for (int i = 0; i < MAX_CONSOLES; i++) {
				if (consoles[i].active)
					continue;
				int fd = socket(AF_UNIX, SOCK_DGRAM, 0);
				if (fd < 0)
					break;

				/* Bind to a unique local path so we can receive datagrams */
				struct sockaddr_un local_addr;
				memset(&local_addr, 0, sizeof(local_addr));
				local_addr.sun_family = AF_UNIX;
				char *name = tail_end_of_name(entry->d_name);
				snprintf(local_addr.sun_path, sizeof(local_addr.sun_path),
					"/tmp/snis-console-rdr.%s", name);
				unlink(local_addr.sun_path);

				if (bind(fd, (struct sockaddr *)&local_addr, sizeof(local_addr)) < 0) {
					close(fd);
					break;
				}

				/* Connect to the remote endpoint to allow easy recv/send */
				struct sockaddr_un remote_addr;
				memset(&remote_addr, 0, sizeof(remote_addr));
				remote_addr.sun_family = AF_UNIX;
				strlcpy(remote_addr.sun_path, full_path, sizeof(remote_addr.sun_path));

				if (connect(fd, (struct sockaddr *)&remote_addr, sizeof(remote_addr)) < 0) {
					close(fd);
					unlink(local_addr.sun_path);
					break;
				}

				strlcpy(consoles[i].name, entry->d_name, sizeof(consoles[i].name));
				strlcpy(consoles[i].socket_path, full_path, sizeof(consoles[i].socket_path));
				consoles[i].sockfd = fd;
				consoles[i].line_count = 0;
				consoles[i].scroll_offset = 0;
				consoles[i].active = 1;
				found_mask[i] = 1;
				active_count++;

				if (current_idx == -1)
					current_idx = i;
				screen_dirty = 1;
				break;
			}
		}
		closedir(dir);

		/* Clean up removed sockets */
		for (int i = 0; i < MAX_CONSOLES; i++) {
			if (!consoles[i].active || found_mask[i])
				continue;
			/* Socket file disappeared */
			struct sockaddr_un un;
			socklen_t len = sizeof(un);
			if (getsockname(consoles[i].sockfd, (struct sockaddr *)&un, &len) == 0)
				unlink(un.sun_path);
			close(consoles[i].sockfd);

			for (int l = 0; l < consoles[i].line_count; l++)
				free(consoles[i].lines[l].text);

			consoles[i].active = 0;
			active_count--;

			if (current_idx == i) {
				current_idx = -1;
				for (int j = 0; j < MAX_CONSOLES; j++) {
					if (consoles[j].active) {
						current_idx = j;
						break;
					}
				}
			}
			screen_dirty = 1;
		}

		if (screen_dirty)
			redraw_ui();

		pthread_mutex_unlock(&data_mutex);
	}
	return NULL;
}

/* Helper to append a line of text to a console log buffer */
static void add_line(struct console *c, const char *msg, uint32_t color)
{
	int pair = map_color_to_pair(color);

	if (c->line_count < MAX_LINES) {
		c->lines[c->line_count].text = strdup(msg);
		c->lines[c->line_count].color_pair = pair;
		c->line_count++;
	} else {
		free(c->lines[0].text);
		memmove(&c->lines[0], &c->lines[1], sizeof(struct textline) * (MAX_LINES - 1));
		c->lines[MAX_LINES - 1].text = strdup(msg);
		c->lines[MAX_LINES - 1].color_pair = pair;
	}
}

/* Renders tab bar, active log view, and status bar */
static void redraw_ui(void)
{
	erase();
	int max_y, max_x;
	getmaxyx(stdscr, max_y, max_x);

	if (active_count == 0) {
		mvprintw(max_y / 2, (max_x - 30) / 2, "No active sockets in /tmp...");
		refresh();
		return;
	}

	/* 1. Draw Tab Bar */
	int x = 0;
	attron(A_REVERSE);
	mvhline(0, 0, ' ', max_x);
	for (int i = 0; i < MAX_CONSOLES; i++) {
		if (!consoles[i].active)
			continue;

		char tab_label[64];
		snprintf(tab_label, sizeof(tab_label), " %s ", consoles[i].name + PREFIX_LEN);

		if (i == current_idx) {
			attroff(A_REVERSE);
			mvprintw(0, x, "%s", tab_label);
			attron(A_REVERSE);
		} else {
			mvprintw(0, x, "%s", tab_label);
		}
		x += strlen(tab_label) + 1;
		if (x >= max_x)
			break;
	}
	attroff(A_REVERSE);

	/* 2. Draw Active Console Messages */
	if (current_idx >= 0 && consoles[current_idx].active) {
		struct console *c = &consoles[current_idx];
		int display_height = max_y - 3; /* Exclude tab bar, separator, status bar */

		/* Clamp scroll offset to valid bounds */
		int max_scroll = c->line_count - display_height;
		if (max_scroll < 0)
			max_scroll = 0;
		if (c->scroll_offset > max_scroll)
			c->scroll_offset = max_scroll;
		if (c->scroll_offset < 0)
			c->scroll_offset = 0;

		/* Calculate rendering start line based on scroll_offset */
		int start_line = c->line_count - display_height - c->scroll_offset;
		if (start_line < 0)
			start_line = 0;

		int end_line = start_line + display_height;
		if (end_line > c->line_count)
			end_line = c->line_count;

		int row = 2;
		for (int l = start_line; l < end_line; l++) {
			if (c->lines[l].color_pair > 0)
				attron(COLOR_PAIR(c->lines[l].color_pair));
			mvprintw(row++, 0, "%.*s", max_x, c->lines[l].text);
			if (c->lines[l].color_pair > 0)
				attroff(COLOR_PAIR(c->lines[l].color_pair));
		}
	}

	/* 3. Draw Status / Help Line */
	attron(A_REVERSE);
	mvhline(max_y - 1, 0, ' ', max_x);
	mvprintw(max_y - 1, 0, " [<-/->/TAB] Switch | [Up/Down/PgUp/PgDn] Scroll | [q] Quit | Active: %d",
				active_count);
	attroff(A_REVERSE);

	refresh();
	screen_dirty = 0;
}

int main(void)
{
	/* Initialize ncurses */
	initscr();
	cbreak();
	noecho();
	keypad(stdscr, TRUE);
	curs_set(0);
	timeout(100); /* 100ms non-blocking wgetch */

	if (has_colors()) {
		start_color();
		if (can_change_color())
			init_color(COLOR_BLACK, 0, 0, 0); /* we want black, not gray */
		init_pair(1, COLOR_GREEN, COLOR_BLACK);
		init_pair(2, COLOR_WHITE, COLOR_BLACK);
		init_pair(3, COLOR_YELLOW, COLOR_BLACK);
		init_pair(4, COLOR_RED, COLOR_BLACK);
		init_pair(5, COLOR_MAGENTA, COLOR_BLACK);
		init_pair(6, COLOR_GREEN, COLOR_BLACK);
		init_pair(7, COLOR_CYAN, COLOR_BLACK);
	}

	memset(consoles, 0, sizeof(consoles));

	/* Start background discovery thread */
	if (pthread_create(&discovery_thread, NULL, discovery_loop, NULL) != 0) {
		endwin();
		fprintf(stderr, "Failed to create discovery thread\n");
		return 1;
	}

	/* Main UI and I/O event loop */
	while (still_running()) {
		pthread_mutex_lock(&data_mutex);

		/* Poll sockets for incoming datagrams */
		for (int i = 0; i < MAX_CONSOLES; i++) {
			if (!consoles[i].active)
				continue;

			unsigned char buf[MAX_MSG_LEN + 2];
			ssize_t bytes_read;
get_more:
			bytes_read = recv(consoles[i].sockfd, buf, sizeof(buf), MSG_DONTWAIT);

			if (bytes_read > 0) {
				unsigned char len = buf[0];
				uint32_t color;
				memcpy(&color, &buf[1], sizeof(color));
				color = ntohl(color);
				if (bytes_read >= len + 1) {
					char msg[MAX_MSG_LEN + 1];
					memcpy(msg, &buf[5], len);
					msg[len] = '\0';

					add_line(&consoles[i], msg, color);
					if (i == current_idx) {
						screen_dirty = 1;
					}
				}
				goto get_more;
			}
		}


		pthread_mutex_unlock(&data_mutex);

		/* Handle keyboard navigation */
		int ch = getch();
		if (ch == ERR)
			continue;

		pthread_mutex_lock(&data_mutex);
		switch (ch) {
		case 'q':
		case 'Q':
		case 033: /* Escape key */
			running = 0;
			break;
		case '\t':
		case KEY_RIGHT:
			/* Next console tab */
			if (active_count > 0) {
				do {
					current_idx = (current_idx + 1) % MAX_CONSOLES;
				} while (!consoles[current_idx].active);
				screen_dirty = 1;
			}
			break;
		case KEY_LEFT:
			/* Previous console tab */
			if (active_count > 0) {
				do {
					current_idx = (current_idx - 1 + MAX_CONSOLES) % MAX_CONSOLES;
				} while (!consoles[current_idx].active);
				screen_dirty = 1;
			}
			break;
		case KEY_UP:
			if (current_idx >= 0 && consoles[current_idx].active) {
				consoles[current_idx].scroll_offset++;
				screen_dirty = 1;
			}
			break;
		case KEY_DOWN:
			if (current_idx >= 0 && consoles[current_idx].active) {
				if (consoles[current_idx].scroll_offset > 0) {
					consoles[current_idx].scroll_offset--;
					screen_dirty = 1;
				}
			}
			break;
		case KEY_PPAGE: /* Page Up */
			if (current_idx >= 0 && consoles[current_idx].active) {
				int max_y, max_x;
				(void) max_x;
				getmaxyx(stdscr, max_y, max_x);
				int page_size = max_y - 3;
				consoles[current_idx].scroll_offset += page_size;
				screen_dirty = 1;
			}
			break;
		case KEY_NPAGE: /* Page Down */
			if (current_idx >= 0 && consoles[current_idx].active) {
				int max_y, max_x;
				(void) max_x;
				getmaxyx(stdscr, max_y, max_x);
				int page_size = max_y - 3;
				consoles[current_idx].scroll_offset -= page_size;
				if (consoles[current_idx].scroll_offset < 0) {
					consoles[current_idx].scroll_offset = 0;
				}
				screen_dirty = 1;
			}
			break;
		}
		if (screen_dirty)
			redraw_ui();
		pthread_mutex_unlock(&data_mutex);
	}

	/* Clean up threads and terminal state */
	pthread_join(discovery_thread, NULL);

	for (int i = 0; i < MAX_CONSOLES; i++) {
		if (!consoles[i].active)
			continue;

		struct sockaddr_un un;
		socklen_t len = sizeof(un);
		if (getsockname(consoles[i].sockfd, (struct sockaddr *)&un, &len) == 0) {
			unlink(un.sun_path);
		}
		close(consoles[i].sockfd);
		for (int l = 0; l < consoles[i].line_count; l++) {
			free(consoles[i].lines[l].text);
		}
	}

	endwin();
	return 0;
}


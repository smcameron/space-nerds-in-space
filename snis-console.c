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
#include <poll.h>

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

	/* Input pane buffer */
	char input_buf[81];      /* Max 80 characters + null terminator */
	int input_len;
};

static struct console consoles[MAX_CONSOLES] = { 0 };
static int active_count = 0; /* count of active snis_servers detected */
static int current_idx = -1; /* current snis server being displayed */
static int screen_dirty = 1; /* Screen needs redrawing? */

static pthread_mutex_t data_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_t discovery_thread;
static int running = 1;

/* Forward declarations */
static void maybe_redraw_ui(int *dirty);
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

		maybe_redraw_ui(&screen_dirty);

		pthread_mutex_unlock(&data_mutex);
		sleep(2);
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
static void maybe_redraw_ui(int *dirty)
{

	if (!*dirty)
		return;

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

		/* Display height is reduced by 4:
		 * Row 0: Tab Bar
		 * Row 1: Blank Separator / Header
		 * Row max_y - 2: Input Field Pane (1 line)
		 * Row max_y - 1: Status / Help Line
		 */
		int display_height = max_y - 4;

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

		/* Draw 1-line Input Pane */
		int input_row = max_y - 2;
		mvprintw(input_row, 0, "> %-80s", c->input_buf);
	}

	/* 3. Draw Status / Help Line */
	attron(A_REVERSE);
	mvhline(max_y - 1, 0, ' ', max_x);
	mvprintw(max_y - 1, 0, " [<-/->/TAB] Switch | [Up/Down/PgUp/PgDn] Scroll | [q] Quit | Active: %d",
				active_count);
	attroff(A_REVERSE);

	/* Position physical ncurses cursor at current prompt input offset */
	if (current_idx >= 0 && consoles[current_idx].active) {
		struct console *c = &consoles[current_idx];
		move(max_y - 2, 2 + c->input_len);
	}

	refresh();
	*dirty = 0;
}

static void initialize_ncurses(void)
{
	/* Initialize ncurses */
	initscr();
	cbreak();
	noecho();
	keypad(stdscr, TRUE);
	curs_set(1);
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
}

/* Set up array of file descriptors for use by poll() and fd_to_console_map[].
 * Must hold data_mutex
 */
static void set_up_file_descriptors(struct pollfd fds[], int fd_to_console_map[], int *nfds, int maxfds)
{
	/* Add STDIN (keyboard) to poll set */
	fds[0].fd = STDIN_FILENO;
	fds[0].events = POLLIN;
	*nfds = 1;

	fd_to_console_map[0] = -1;

	/* Add all active console sockets to poll set */
	for (int i = 0; i < maxfds; i++) {
		if (consoles[i].active) {
			fds[*nfds].fd = consoles[i].sockfd;
			fds[*nfds].events = POLLIN;
			fd_to_console_map[*nfds] = i;
			(*nfds)++;
		}
	}
}

/* Wait for something to happen on any of the file descriptors */
#define POLL_ERROR 0
#define POLL_INTERRUPTED -1
#define POLL_COMPLETE 1
static int wait_for_action_on_fds(struct pollfd fds[], int nfds)
{
	int poll_res;

	/* Block here until activity happens on keyboard OR sockets (500ms timeout) */
	poll_res = poll(fds, nfds, 500);
	if (poll_res < 0) {
		if (errno == EINTR)
			return POLL_INTERRUPTED;
		return POLL_ERROR;
	}
	return POLL_COMPLETE;
}

static void handle_incoming_data_on_sockets(struct pollfd fds[], int fd_to_console_map[], int nfds, int *dirty)
{
	for (int i = 1; i < nfds; i++) {
		if (!(fds[i].revents & POLLIN))
			continue;

		int c_idx = fd_to_console_map[i];
		if (!consoles[c_idx].active)
			continue;

		unsigned char buf[MAX_MSG_LEN + 2];
		ssize_t bytes_read;

		while ((bytes_read = recv(consoles[c_idx].sockfd, buf, sizeof(buf), MSG_DONTWAIT)) > 0) {
			unsigned char len = buf[0];
			uint32_t color;
			memcpy(&color, &buf[1], sizeof(color));
			color = ntohl(color);

			if (bytes_read < len + 1)
				continue;

			char msg[MAX_MSG_LEN + 1];
			memcpy(msg, &buf[5], len);
			msg[len] = '\0';

			add_line(&consoles[c_idx], msg, color);
			if (c_idx == current_idx)
				*dirty = 1;
		}
	}
}

static void handle_keyboard_input(struct pollfd fds[], int *dirty)
{
	struct console *c = NULL;

	/* Check for stdin / user keyboard input */
	if (!(fds[0].revents & POLLIN))
		goto finished_keyboard_input;

	int ch = getch();
	if (ch == ERR)
		goto finished_keyboard_input;

	switch (ch) {
	case 033: /* Escape key */
		running = 0;
		break;
	case '\t':
	case KEY_RIGHT:
		if (active_count > 0) {
			do {
				current_idx = (current_idx + 1) % MAX_CONSOLES;
			} while (!consoles[current_idx].active);
			*dirty = 1;
		}
		break;
	case KEY_LEFT:
		if (active_count > 0) {
			do {
				current_idx = (current_idx - 1 + MAX_CONSOLES) % MAX_CONSOLES;
			} while (!consoles[current_idx].active);
			*dirty = 1;
		}
		break;
	case KEY_UP:
		if (current_idx >= 0 && consoles[current_idx].active) {
			consoles[current_idx].scroll_offset++;
			*dirty = 1;
		}
		break;
	case KEY_DOWN:
		if (current_idx >= 0 && consoles[current_idx].active) {
			if (consoles[current_idx].scroll_offset > 0) {
				consoles[current_idx].scroll_offset--;
				*dirty = 1;
			}
		}
		break;
	case KEY_PPAGE:
		if (current_idx >= 0 && consoles[current_idx].active) {
			int max_y, max_x;
			(void) max_x;
			getmaxyx(stdscr, max_y, max_x);
			consoles[current_idx].scroll_offset += (max_y - 3);
			*dirty = 1;
		}
		break;
	case KEY_NPAGE:
		if (current_idx >= 0 && consoles[current_idx].active) {
			int max_y, max_x;
			(void) max_x;
			getmaxyx(stdscr, max_y, max_x);
			consoles[current_idx].scroll_offset -= (max_y - 3);
			if (consoles[current_idx].scroll_offset < 0) {
				consoles[current_idx].scroll_offset = 0;
			}
			*dirty = 1;
		}
		break;
	case '\n':
	case '\r':
	case KEY_ENTER:
		if (current_idx < 0 || !consoles[current_idx].active)
			break;
		c = &consoles[current_idx];
		if (c->input_len <= 0)
			break;
		add_line(c, c->input_buf, DARKTURQUOISE);

		struct sockaddr_un dest_addr;
		memset(&dest_addr, 0, sizeof(dest_addr));
		dest_addr.sun_family = AF_UNIX;

		char *tab_name = tail_end_of_name(c->name);
		if (tab_name) {
			snprintf(dest_addr.sun_path, sizeof(dest_addr.sun_path),
					"/tmp/snis-console.%s", tab_name);
			sendto(c->sockfd, c->input_buf, c->input_len, 0,
				   (struct sockaddr *)&dest_addr, sizeof(dest_addr));
		}

		c->input_buf[0] = '\0';
		c->input_len = 0;
		c->scroll_offset = 0;
		*dirty = 1;
		break;
	case KEY_BACKSPACE:
	case 127:
	case '\b':
		if (current_idx < 0 || !consoles[current_idx].active)
			break;
		c = &consoles[current_idx];
		if (c->input_len > 0) {
			c->input_len--;
			c->input_buf[c->input_len] = '\0';
			*dirty = 1;
		}
		break;
	default:
		if (current_idx < 0 || !consoles[current_idx].active)
			break;
		c = &consoles[current_idx];
		if (ch >= 32 && ch <= 126 && c->input_len < 80) {
			c->input_buf[c->input_len++] = (char)ch;
			c->input_buf[c->input_len] = '\0';
			*dirty = 1;
		}
		break;
	}

finished_keyboard_input:
	return;
}

static void clean_up(void)
{
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
}

int main(void)
{
	initialize_ncurses();

	/* Start background discovery thread */
	if (pthread_create(&discovery_thread, NULL, discovery_loop, NULL) != 0) {
		endwin();
		fprintf(stderr, "Failed to create discovery thread\n");
		return 1;
	}

	/* Main UI and I/O event loop */
	while (still_running()) {
		struct pollfd fds[MAX_CONSOLES + 1];
		int fd_to_console_map[MAX_CONSOLES + 1];
		int nfds = 0;

		pthread_mutex_lock(&data_mutex);
		set_up_file_descriptors(fds, fd_to_console_map, &nfds, MAX_CONSOLES);
		pthread_mutex_unlock(&data_mutex);

		int rc = wait_for_action_on_fds(fds, nfds);
		if (rc == POLL_INTERRUPTED)
			continue;
		if (rc == POLL_ERROR)
			break;

		pthread_mutex_lock(&data_mutex);
		handle_incoming_data_on_sockets(fds, fd_to_console_map, nfds, &screen_dirty);
		handle_keyboard_input(fds, &screen_dirty);
		pthread_mutex_unlock(&data_mutex);
		maybe_redraw_ui(&screen_dirty);
	}
	clean_up();
	return 0;
}


#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

void clean_spaces(char *line)
{
	char *s, *d;
	int skip_spaces = 1;

	s = line;
	d = line;

	while (*s) {
		if ((*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') && skip_spaces) {
			s++;
			continue;
		}
		skip_spaces = 0;
		if (*s == '\t' || *s == '\n' || *s == '\r')
			*s = ' ';
		if (*s == ' ')
			skip_spaces = 1;
		*d = *s;
		s++;
		d++;
	}
	*d = '\0';
}

void remove_trailing_whitespace(char *s)
{
	int len = strlen(s) - 1;

	while (len >= 0) {
		switch (s[len]) {
		case '\t':
		case ' ':
		case '\r':
		case '\n':
			s[len] = '\0';
			len--;
			break;
		default:
			return;
		}
	}
}

void uppercase(char *w)
{
	char *i;

	for (i = w; *i; i++)
		*i = toupper(*i);
}

void lowercase(char *w)
{
	char *i;

	for (i = w; *i; i++)
		*i = tolower(*i);
}

char *dir_name(char *path)
{
	char *x;
	int l;

	x = strdup(path);
	if (!x)
		return NULL;
	l = strlen(x);
	while (l > 0 && x[l - 1] != '/') {
		x[l - 1] = '\0';
		l--;
	}
	return x;
}

char *trim_whitespace(char *s)
{
	char *x, *z;

	for (x = s; *x == ' ' || *x == '\t'; x++)
		;
	z = x + (strlen(x) - 1);

	while (z >= x && (*z == ' ' ||  *z == '\t' || *z == '\n')) {
		*z = '\0';
		z--;
	}
	return x;
}

static char *skip_leading_whitespace(char *s)
{
	char *i;

	for (i = s; *i == ' ' || *i == '\t' || *i == '\n';)
		i++;
	return i;
}

int has_prefix(char *prefix, char *str)
{
	return strncmp(prefix, str, strlen(prefix)) == 0;
}

char *slurp_file(const char *path, int *bytes)
{
	int rc, fd;
	struct stat statbuf;
	char *buffer;
	int bytesleft, bytesread, count;

	rc = stat(path, &statbuf);
	if (rc) {
		fprintf(stderr, "stat(%s): %s\n", path, strerror(errno));
		return NULL;
	}

	fd = open(path, O_RDONLY);
	if (fd < 0) {
		fprintf(stderr, "open(%s): %s\n", path, strerror(errno));
		return NULL;
	}
	buffer = malloc(statbuf.st_size + 1);

	count = 0;
	bytesleft = (int) statbuf.st_size;
	do {
		bytesread = read(fd, buffer + count, bytesleft);
		if (bytesread < 0) {
			if (errno == EINTR)
				continue;
			else {
				fprintf(stderr, "read: %d bytes from %s failed: %s\n",
						bytesleft, path, strerror(errno));
				close(fd);
				free(buffer);
				return NULL;
			}
		} else {
			bytesleft -= bytesread;
			count += bytesread;
		}
	} while (bytesleft > 0);
	close(fd);
	buffer[statbuf.st_size] = '\0';
	if (bytes)
		*bytes = (int) statbuf.st_size;
	return buffer;
}

void remove_single_quotes(char *s)
{
	char *src = s;
	char *dest = s;

	do {
		if (*src == '\'') {
			src++;
			continue;
		}
		if (dest == src) {
			if (!*src)
				break;
			dest++;
			src++;
			continue;
		}
		*dest = *src;
		if (!*src)
			break;
		dest++;
		src++;
	} while (1);
}

int strchrcount(char *s, int c)
{
	char *x;
	int count = 0;

	for (x = s; *x; x++)
		if (*x == c)
			count++;
	return count;
}

size_t strlcpy(char *dest, const char *src, size_t n)
{
	size_t i;

	for (i = 0;; i++) {
		if (i < n)
			dest[i] = src[i];
		else {
			dest[n - 1] = '\0';
			break;
		}
		if (src[i] == '\0')
			return i;
	}
	for (;; i++)
		if (src[i] == '\0')
			break;
	return i;
}

/* Printing a pointer to a function via %p is forbidden by ISO C, so we have this BS instead: */
void format_function_pointer(char *buffer, void (*function_pointer)(void))
{
	int direction = 0x01020304;
	int i, p, start, end;
	char *endian = (char *) &direction;

	if (endian[0] == 0x01) { /* big endian */
		direction = 1;
		start = 0;
		end = sizeof(function_pointer);
	} else {
		direction = -1;
		start = sizeof(function_pointer) - 1;
		end = -1;
	}
	sprintf(buffer, "0x");
	p = 1;
	for (i = start; i != end; i += direction) {
		sprintf(&buffer[p * 2], "%02X", ((unsigned char *) &function_pointer)[i]);
		p++;
	}
}

/* Given a string of the form "xxxxx : yyyyy", return "yyyyy"
 * Basically, return the part of the string after the colon,
 * skipping any leading whitespace after the colon.  This does
 * not allocate, it returns a pointer inside the given string.
 */
char *get_field(char *line)
{
	char *i;

	for (i = line; *i != '\0' && *i != ':';)
		i++;
	if (*i == ':') {
		i++;
		return skip_leading_whitespace(i);
	}
	return i;
}

/* get_abbreviated_command_arg is for processing console commands.
 * There are commands like * "DESCRIBE BLAH". But the user does not have to
 * type DESCRIBE, they just have to type enough of DESCRIBE to be unique
 * among the various commands, so "DESC BLAH" or even "DE BLAH" should work,
 * if unique.  But then we need to find the command argument after whatever
 * they typed. so,
 *
 *   get_abbreviated_command_arg("DESCRIBE", "DESC BLAH");
 *
 * should return a pointer to the B in DESC BLAH
 *
 *   get_abbreviated_command_arg("DESCRIBE", "DECSRIBE BLAH");
 *
 * should return NULL. (DECSCRIBE is misspelled).
 */
char *get_abbreviated_command_arg(char *expected_command, char *user_input)
{
	char *e, *i;
	int mismatch = 0;

	e = expected_command;
	i = user_input;

	for (;*i && *e;) {
		if (*i == *e) {
			i++;
			e++;
			continue;
		}
		if (*i == ' ') {
			i++;
			break;
		}
		mismatch = 1;
		break;
	}
	if (mismatch)
		return NULL;
	while (*i == ' ')
		i++;
	return i;
}

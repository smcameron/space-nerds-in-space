#include <ctype.h>
#include <string.h>

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

	do {
		switch (s[len]) {
		case '\t':
		case ' ':
		case '\r':
		case '\n':
			s[len] = '\0';
			len--;
		default:
			return;
		}
	} while (1);
}

void uppercase(char *w)
{
	char *i;

	for (i = w; *i; i++)
		*i = toupper(*i);
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


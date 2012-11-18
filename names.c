#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "names.h"

static char *vowel[] = {
	"a",
	"e",
	"i",
	"o",
	"u",
};

static char *consonant[] = {
	"b", "c", "ch", "d", "f", "g", "h", "i", "j", "k", "l", "m", "n", "p", "qu", "r", "s",
	"t", "v", "w", "x", "y", "z", "gh", "kh", "ph", "rh", "sh", "th", "sp", "wh", "zh", "st",
	"en", "in", "er", "ss", "bb", "mm", "nn", "kn", "ts", "xx", "ll", "ff", "pp", 
};

static char *pattern[] = {
	"vcvce",
	"cvcvc",
	"cvcv"
	"vcv",
	"vcvvc"
	"cvvcv",
	"cvcvcvc",
	"cvcvvc",
	"vvcv",
	"cvcvv",
	"vcvvcvc",
	"vcvcvcvc",
	"cvvcvc",
	"cvc",
};

#define ARRAYSIZE(x) (sizeof(x) / (sizeof(x[0])))

static void append_stuff(char *s, char *a[], int asize)
{
	int e;

	e = rand() % asize;
	strcat(s, a[e]);
}

char *random_name(void)
{
	char *i;
	int  p, v, c;
	char *result;

	result = malloc(100);
	p = rand() % ARRAYSIZE(pattern);
	memset(result, 0, 100);

	for (i = pattern[p]; *i; i++) {
		if (*i == 'v')
			append_stuff(result, vowel, ARRAYSIZE(vowel));
		else
			append_stuff(result, consonant, ARRAYSIZE(consonant));
	}
	for (i = result; *i; i++)
		*i = toupper(*i);
	return result;
}

#if 0
#include <stdio.h>
int main(int argc, char *argv[])
{
	int i, seed;
	char *n;

	if (argc > 1) {
		sscanf(argv[1], "%d", &seed);
		srand(seed);
	}

	for (i = 0; i < 1000; i++) {
		n = random_name();
		printf("%s\n", n);
		free(n);
	}
	return 0;
}
#endif

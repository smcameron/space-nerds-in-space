#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "names.h"
#include <stdio.h>

static char *vowel[] = {
	"a",
	"e",
	"i",
	"o",
	"u",
};

static char *consonant[] = {
	"b", "ch", "d", "f", "g", "j", "k", "l", "m", "n", "p", "r", "s",
	"t", "v", "x", "z", "ph", "sh", "th", "sp", "st",
};

static char *pattern[] = {
	"vcvc",
	"cvcvc",
	"cvcv",
	"vcv",
	"cvc",
	"cvv",
	"cvcvv",
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
	int  p;
	char *result;

	result = malloc(100);
	p = rand() % ARRAYSIZE(pattern);
	memset(result, 0, 100);

	for (i = pattern[p]; *i; i++) {
		if (*i == 'v')
			append_stuff(result, vowel, ARRAYSIZE(vowel));
		else
			append_stuff(result, consonant, ARRAYSIZE(consonant));
		/* printf("zzz result = %s, pattern = %s, *i = %c\n", result, pattern[p], *i); */
	}
	for (i = result; *i; i++)
		*i = toupper(*i);
	return result;
}

#ifdef TEST_NAMES 
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

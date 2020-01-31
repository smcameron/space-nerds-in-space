#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "arraysize.h"
#include "names.h"
#include "mtwist.h"

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

static char *ending[] = {
	"on", "ino", "ion", "an", "ach", "tar", "ron", "o", "son", "berg",
	"ton", "ides", "ski", "nova", "petra", "aster", "ius", "ipha", "ian"
};

static char *pattern[] = {
	"vcvc",
	"cvcvc",
	"cvcv",
	"vcv",
	"cvc",
	"cvv",
	"cvcvv",
	"vccvv",
	"vccvvc",
	"cvccvvc",
	"cvvccvc",
	"cvvccvc",
	"cvvccvc",
	"vcvcd",
	"cvcvcd",
	"cvcvd",
	"vcvd",
	"cvcd",
	"cvvd",
	"cvcvvd",
	"vccvvd",
	"vccvvcd",
	"cvccvvcd",
	"cvvccvcd",
	"cvvccvcd",
	"cvvccvcd",
};

static void append_stuff(struct mtwist_state *mt, char *s, char *a[], int asize)
{
	int e;

	e = mtwist_next(mt) % asize;
	strcat(s, a[e]);
}

char *random_name(struct mtwist_state *mt)
{
	char *i;
	int  p;
	char *result;

	result = malloc(100);
	p = mtwist_next(mt) % ARRAYSIZE(pattern);
	memset(result, 0, 100);

	for (i = pattern[p]; *i; i++) {
		if (*i == 'v')
			append_stuff(mt, result, vowel, ARRAYSIZE(vowel));
		else if (*i == 'c')
			append_stuff(mt, result, consonant, ARRAYSIZE(consonant));
		else
			append_stuff(mt, result, ending, ARRAYSIZE(ending));
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
	struct mtwist_state *mt;

	if (argc > 1) {
		sscanf(argv[1], "%d", &seed);
		mt = mtwist_init(seed);
	} else { 
		mt = mtwist_init(12345);
	}

	for (i = 0; i < 1000; i++) {
		n = random_name(mt);
		printf("%s\n", n);
		free(n);
	}
	return 0;
}
#endif

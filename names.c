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

static void append_stuff(struct mtwist_state *mt, char buffer[], int bufsize, char *a[], int asize)
{
	int e;

	e = mtwist_next(mt) % asize;
	int len = strlen(buffer) + strlen(a[e]);
	int remaining_chars = bufsize - strlen(buffer) - 1;
	if (remaining_chars > 1)
		strncat(buffer, a[e], remaining_chars);
	if (len < bufsize - 1)
		buffer[len] = '\0';
	else
		buffer[bufsize - 1] = '\0';
}

void random_name(struct mtwist_state *mt, char buffer[], int bufsize)
{
	char *i;
	int  p;

	buffer[0] = '\0';
	p = mtwist_next(mt) % ARRAYSIZE(pattern);

	for (i = pattern[p]; *i; i++) {
		if (*i == 'v')
			append_stuff(mt, buffer, bufsize, vowel, ARRAYSIZE(vowel));
		else if (*i == 'c')
			append_stuff(mt, buffer, bufsize, consonant, ARRAYSIZE(consonant));
		else
			append_stuff(mt, buffer, bufsize, ending, ARRAYSIZE(ending));
		/* printf("zzz buffer = %s, pattern = %s, *i = %c\n", buffer, pattern[p], *i); */
	}
	for (i = buffer; *i; i++)
		*i = toupper(*i);
}

#ifdef TEST_NAMES 
#include <stdio.h>
int main(int argc, char *argv[])
{
	int i, seed;
	char buffer[100];
	struct mtwist_state *mt;

	if (argc > 1) {
		sscanf(argv[1], "%d", &seed);
		mt = mtwist_init(seed);
	} else { 
		mt = mtwist_init(12345);
	}

	for (i = 0; i < 1000; i++) {
		random_name(mt, buffer, sizeof(buffer));
		printf("%s\n", buffer);
	}
	return 0;
}
#endif

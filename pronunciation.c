#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <regex.h>

#include "arraysize.h"
#include "pronunciation.h"
#include "string-utils.h"

static struct fixup_list_entry {
	char *pattern;
	char *replacement;
	regex_t *re;
} fixup_list[] = {
	{ " [Ss][Bb]-0", " star base ", NULL },
	{ "^[Ss][Bb]-0", "star base ", NULL },
	{ " [Ss][Bb]-", " star base ", NULL },
	{ "^[Ss][Bb]-", "star base ", NULL },
	{ " [Ww][Gg]-0", " warp gate ", NULL },
	{ "^[Ww][Gg]-0", "warp gate ", NULL },
	{ " [Ww][Gg]-", " warp gate ", NULL },
	{ "^[Ww][Gg]-", "warp gate ", NULL },
	{ " [Ww][Pp]-0", " waypoint ", NULL },
	{ "^[Ww][Pp]-0", "waypoint ", NULL },
	{ " [Ww][Pp]-", " waypoint ", NULL },
	{ "^[Ww][Pp]-", "waypoint ", NULL },
};

#define TMP_PRONOUNCE_BUFFER_SIZE 4096

static int fix_pronunciation_buffer(char *input, char *output, struct fixup_list_entry *fixup)
{
	int rc;
	regmatch_t match;

	if (!fixup->re) {
		fixup->re = malloc(sizeof(*fixup->re));
		rc = regcomp(fixup->re, fixup->pattern, 0);
		if (rc) {
			fprintf(stderr, "Failed to compile regular expression '%s'\n", fixup->pattern);
			free(fixup->re);
			fixup->re = NULL;
			return 0;
		}
	}

	rc = regexec(fixup->re, input, 1, &match, 0);
	if (rc) {
		strlcpy(output, input, TMP_PRONOUNCE_BUFFER_SIZE);
		return 0;
	}

	memset(output, 0, TMP_PRONOUNCE_BUFFER_SIZE);
	strlcpy(output, input, match.rm_so);
	strncat(output, fixup->replacement, TMP_PRONOUNCE_BUFFER_SIZE - strlen(output) - 1);
	strncat(output, &input[match.rm_eo], TMP_PRONOUNCE_BUFFER_SIZE - strlen(output) - 1);
	return 1;
}

/* Fix up input for text to speech.  Returns newly allocated string. */
char *fix_pronunciation(char *input)
{
	char data_buffer[TMP_PRONOUNCE_BUFFER_SIZE * 2] = { 0 };
	char *buffer[2] = { &data_buffer[0], &data_buffer[TMP_PRONOUNCE_BUFFER_SIZE] };
	int current_buffer = 0;
	int next_buffer = 1;
	int count = 0;
	int i;

	strlcpy(buffer[current_buffer], input, TMP_PRONOUNCE_BUFFER_SIZE);
	buffer[current_buffer][4095] = '\0';

	for (i = 0; (size_t) i < ARRAYSIZE(fixup_list); i++) {
		do {
			count = fix_pronunciation_buffer(buffer[current_buffer], buffer[next_buffer], &fixup_list[i]);
			current_buffer = next_buffer;
			next_buffer = (current_buffer + 1) & 0x01;
		} while (count > 0);
	}
	return strdup(buffer[current_buffer]);
}

#undef TMP_PRONOUNCE_BUFFER_SIZE

#ifdef TEST_PRONUNCIATION_FIXUP

static void testcase(char *input)
{
	char *output;

	output = fix_pronunciation(input);
	printf("'%s' -> '%s'\n", input, output);
	free(output);
}

int main(int argc, char *argv)
{
	char *output;

	testcase("Setting a course for SB-01 ok?");
	testcase("SB-02 is thataway!");
	testcase("sb-01 and SB-02 and sb-03 are being attacked!");
	testcase("SB-21 and SB-42");
	testcase("Setting a course for WG-01 ok?");
	testcase("WG-02 is thataway!");
	testcase("WG-01 and WG-02 and WG-03 are being attacked!");
	testcase("WG-25 and WG-26");
	return 0;
}
#endif

/*
	Copyright (C) 2016 Stephen M. Cameron
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

#include "spelled_numbers.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "arraysize.h"

static struct number_map_entry {
	char *name;
	float value;
} number_map[] = {
	{ "percent", 0.01 },
	{ "zero", 0 },
	{ "one third", 0.33 },
	{ "one half", 0.50 },
	{ "one quarter", 0.25 },
	{ "three quarters", 0.75 },
	{ "two thirds", 0.67 },
	{ "one fourth", 0.25 },
	{ "one", 1 },
	{ "two", 2 },
	{ "three", 3 },
	{ "five", 5 },
	{ "niner", 9 },
	{ "ten", 10 },
	{ "eleven", 11 },
	{ "twelve", 12 },
	{ "thirteen", 13 },
	{ "fourteen", 14 },
	{ "fifteen", 15 },
	{ "sixteen", 16 },
	{ "seventeen", 17 },
	{ "eighteen", 18 },
	{ "nineteen", 19 },
	{ "twenty", 20 },
	{ "score", 20 },
	{ "thirty", 30 },
	{ "forty", 40 },
	{ "fifty", 50 },
	{ "sixty", 60 },
	{ "seventy", 70 },
	{ "eighty", 80 },
	{ "ninety", 90 },
	{ "a hundred", 100 },
	{ "hundred", 100 },
	{ "a thousand", 1000 },
	{ "thousand", 1000 },
	{ "four", 4 }, /* these need to come after, eg. fourteen, sixteen, etc. */
	{ "six", 6 },
	{ "seven", 7 },
	{ "eight", 8 },
	{ "nine", 9 },
};

static int seems_ok(int preceding_char, int next_char)
{
	/* printf("preceding = '%c', next = '%c'\n", preceding_char, next_char); */
	if ((preceding_char >= 'a' && preceding_char <= 'z') ||
		(preceding_char >= 'A' && preceding_char <= 'Z'))
		return 0;

	if (next_char == '\0')
		return 1;

	if ((next_char >= 'a' && next_char <= 'z') ||
		(next_char >= 'A' && next_char <= 'Z'))
		return 0;
	return 1;
}

static int straight_replace_entry(char *input, struct number_map_entry *e)
{
	char *m;
	char replacement[100];
	int i, len, wordlen;
	int preceding_char, next_char;

	m = strstr(input, e->name);
	if (m) {
		if (e->value < 1.0 && e->value > 0.0)
			sprintf(replacement, "%0.2f", e->value);
		else
			sprintf(replacement, "%.0f", e->value);
		if (strcmp(e->name, "percent") == 0)
			strcpy(replacement, "");
		len = strlen(replacement);
		wordlen = strlen(e->name);
		if ((size_t) len > strlen(e->name)) {
			printf("Bug, '%s' is longer than '%s'\n",
				replacement, e->name);
			return -1;
		}
		if (m == input)
			preceding_char = -1;
		else
			preceding_char = *(m - 1);
		next_char = *(m + wordlen);

		if (!seems_ok(preceding_char, next_char))
			return 0;

		for (i = 0; i < len; i++) {
			m[i] = replacement[i];
		}
		m = m + len;
		for (; *(m + wordlen - len); m++)
			*m = *(m + wordlen - len);
		*m = '\0';
		return 1;
	}
	return 0;
}

static void straight_replace_loop(char *input, struct number_map_entry *e)
{
	int rc;
	do {
		rc = straight_replace_entry(input, e);
	} while (rc == 1);
}

static int find_next_digit_group(char *s, int begin_search, int *start, int *end)
{
	int i, len;
	int first = -1;
	int last = -1;

	len = strlen(s);
	if (begin_search >= len)
		return -1;
	for (i = begin_search; i < len; i++) {
		if (isdigit(s[i])) {
			if (first < 0)
				first = i;
		} else {
			if (first >= 0) {
				last = i - 1;
				*start = first;
				*end = last;
				return first;
			}
		}
	}
	if (first >= 0 && last < 0) {
		*end = len - 1;
		*start = first;
		return first;
	}
	return -1;
}

static int value_of_digit_group(char *s, int start, int end)
{
	int i, value, place;

	/* fprintf(stderr, "Computing value of '%s' from %d to %d\n", s, start, end); */

	place = 1;
	value = 0;
	for (i = end; i >= start; i--) {
		value += ((int) (s[i] - '0')) * place;
		place *= 10;
	}
	return value;
}

/* Check if there is anything between start + 1 and end - 1 inclusive
 * that isn't whitespace, or the word 'and'.  Return true if there's something
 * else, false if it's only whitespace and/or the word 'and'.
 */
static int anything_between(char *s, int start, int end)
{
	int i, all_spaces;
	char *and;

	/* Nothing but spaces between start and end? */
	for (i = start + 1; i < end; i++)
		if (s[i] != ' ')
			goto harder_test;
	return 0; /* nothing but spaces between start and end. */

harder_test:

	if ((end - start) < 6)
		return 1; /* not enough room for those non-spaces to be " and " */

	and = strstr(&s[start + 1], " and ");
	if (!and)
		return 1; /* Nope, wasn't " and " */

	/* Blot out the and, and see if now it's all spaces. */
	and[1] = ' ';
	and[2] = ' ';
	and[3] = ' ';

	all_spaces = 1;
	for (i = start + 1; i < end; i++)
		if (s[i] != ' ')
			all_spaces = 0;
	if (all_spaces)
		return 0; /* After removing the 'and', nothing was left but spaces. */

	/* After removing the 'and', there's still something else.  Put the 'and' back. */
	and[1] = 'a';
	and[2] = 'n';
	and[3] = 'd';
	return 1;
}

static void replace_segment(char *s, int start, int end, char *replacement)
{
	int i, len = strlen(replacement);
	char *src, *dest;

	if (1 + end - start < len) {
		fprintf(stderr, "problem: 1 + end - start < strlen(replacement)\n");
		fprintf(stderr, "s = '%s', replacement = '%s'", s, replacement);
		fprintf(stderr, "start = %d, end = %d, strlen(replacement) = %d\n",
			start, end, (int) strlen(replacement));
		fflush(stderr);
	}

	for (i = 0; i < len; i++)
		s[start + i] = replacement[i];
	src = &s[end + 1];
	dest = &s[start + len];
	while (*src) {
		*dest = *src;
		src++;
		dest++;
	}
	*dest = '\0';
}

static int combine_numeric_values_once(char *input)
{
	int len, first_group, fs, fe, second_group, ss, se;
	int start = 0;
	int v1, v2, v3;
	char new_number[100];
	int changed_something = 0;

	/* printf("Combine numeric_values_once('%s')\n", input); */
	len = strlen(input);
	do {
		first_group = find_next_digit_group(input, start, &fs, &fe);
		/* printf("first_Group = %d-%d\n", first_group, fe); */
		if (first_group < 0)
			return changed_something;

		second_group = find_next_digit_group(input, fe + 1, &ss, &se);
		/* printf("second_Group = %d-%d\n", second_group, se); */
		if (second_group < 0)
			return changed_something;

		if (anything_between(input, fe, ss)) {
			start = se + 1;
			if (start > len)
				return changed_something;
			continue;
		}
		/* printf("nothing between\n"); */

		v1 = value_of_digit_group(input, fs, fe);
		v2 = value_of_digit_group(input, ss, se);

		/*****
		printf("v1 = %d, v2 = %d\n", v1, v2);
		printf("v2 %% 1000) = %d\n", v2 % 1000);
		printf("v1 < v2) = %d\n", v1 < v2);
		*****/

		if ((v2 % 1000) == 0 && v1 < v2) {
			v3 = v1 * v2;
		} else if ((v2 % 100) == 0 && v1 < v2) {
			v3 = v1 * v2;
		} else {
			v3 = v1 + v2;
		}
		sprintf(new_number, "%d", v3);
		/* printf("v3 = %d\n", v3); */
		replace_segment(input, fs, se, new_number);
		/* printf("after rplacement input = '%s'\n", input); */
		start = fs + strlen(new_number);
		changed_something = 1;
	} while (1);
}

char *handle_spelled_numbers_in_place(char *input)
{
	int i, changes;

	for (i = 0; (size_t) i < ARRAYSIZE(number_map); i++) {
		straight_replace_loop(input, &number_map[i]);
	}
	do {
		changes = combine_numeric_values_once(input);
	} while (changes != 0);
	return input;
}

char *handle_spelled_numbers(char *input)
{
	char *x = strdup(input);
	return handle_spelled_numbers_in_place(x);
}

static char *one[] = { "zero", "one", "two", "three", "four", "five", "six", "seven", "eight", "nine" };
static char *teen[] = { "ten", "eleven", "twelve", "thirteen", "fourteen", "fifteen", "sixteen",
				"seventeen", "eightteen", "nineteen" };
static char *tens[] = { "", "", "twenty", "thirty", "forty", "fifty", "sixty", "seventy", "eighty", "ninety" };

static int frac_to_words(float number, char *buffer, int buflen, int max_decimal_places)
{
	int i, rc, x;
	float frac = fabsf(number - truncf(number));
	int digit;

	buflen--;
	rc = snprintf(buffer, buflen, "point");
	if (rc < 0)
		return -1;
	buflen -= rc;
	x = rc;
	for (i = 0; i < max_decimal_places; i++) {
		char *word;
		frac = (frac - trunc(frac)) * 10.0;
		digit = (int) (trunc(frac));
		word = one[digit];
		rc = snprintf(&buffer[x], buflen, " %s", word);
		if (rc < 0)
			return -1;
		x += rc;
		buflen -= x;
		if (buflen < 2)
			return -1;
	}
	return x;
}

static int int_to_words(int number, char *buffer, int buflen);

/* number is the number to convert.
 * number_name = "billion", "million", "thousand", or "hundred"
 * number_value = the integer value of number_name
 * output the buffer to store the output words in
 * outputlen = capacity of output buffer
 */
static int int_to_words_helper(int number, char *number_name, int number_value, char *output, int outputlen)
{
	char num[100];
	char rest[100];
	int rc;

	int n = number / number_value; /* i.e. millions = number / 1000000 */
	int remaining = number - number_value * n;
	rc = int_to_words(n, num, 100);
	if (rc < 0)
		return rc;
	if (remaining != 0) {
		rc = int_to_words(remaining, rest, 100);
		if (rc < 0)
			return rc;
		rc = snprintf(output, outputlen, "%s %s %s", num, number_name, rest);
	} else {
		rc = snprintf(output, outputlen, "%s %s", num, number_name);
	}
	return rc;
}

static int int_to_words(int number, char *buffer, int buflen)
{
	int rc, x;

	x = 0;
	if (number < 0)
		number = -number;
	if (number < 10) {
		rc = snprintf(&buffer[x], buflen, "%s", one[number]);
		if (rc < 0)
			return -1;
		buflen -= rc;
		x += rc;
		return x;
	}
	if (number < 20) {
		rc = snprintf(&buffer[x], buflen, "%s", teen[number - 10]);
		if (rc < 0)
			return -1;
		buflen -= rc;
		x += rc;
		return x;
	}
	if (number < 100) {
		char rest[100];
		int ts = number / 10;
		int remaining = number - 10 * ts;
		if (remaining != 0) {
			rc = int_to_words(remaining, rest, 100);
			if (rc < 0)
				return rc;
			rc = snprintf(buffer, buflen, "%s %s", tens[ts], rest);
		} else {
			rc = snprintf(buffer, buflen, "%s", tens[ts]);
		}
		return rc;
	}
	if (number < 1000)
		return int_to_words_helper(number, "hundred", 100, buffer, buflen);
	if (number < 1000000)
		return int_to_words_helper(number, "thousand", 1000, buffer, buflen);
	if (number < 1000000000)
		return int_to_words_helper(number, "million", 1000000, buffer, buflen);
	/* if (number < 1000000000000) */ /* always true as number is an int and a trillion won't fit. */
		return int_to_words_helper(number, "billion", 1000000000, buffer, buflen);
}

static int integer_to_words(int number, char *buffer, int buflen)
{
	int rc = 0;

	if (number < 0) {
		rc = snprintf(buffer, buflen, "minus ");
		if (rc < 0)
			return rc;
		number = -number;
	}
	rc = int_to_words(number, &buffer[rc], buflen - rc);
	return rc;
}

/* Convert numeric values to words, e.g. 1234.5 becomes "one thousand two hundred thirty-four point five" */
int numbers_to_words(float number, int max_decimal_places, char *buffer, int buflen)
{
	int i, rc;
	float f;
	char ipart[500], fpart[500];

	i = trunc(number);
	f = fabsf(number) - abs(i);

	rc = integer_to_words(i, ipart, 500);
	if (rc < 0)
		return rc;
	if (max_decimal_places > 0) {
		rc = frac_to_words(f, fpart, 500, max_decimal_places);
		if (rc < 0)
			return rc;
	}

	if ((f * 1000) >= 1.0 && max_decimal_places > 0)
		rc = snprintf(buffer, buflen, "%s %s", ipart, fpart);
	else
		rc = snprintf(buffer, buflen, "%s", ipart);
	return rc;
}

#ifdef SPELLED_NUMBERS_TEST_CASE
static int total = 0;
static int testcase(char *input, char *expected)
{
	char *output = handle_spelled_numbers(input);

	total++;
	if (output == NULL) {
		printf("Failed: input = '%s'\n"
			"        output = '%s'\n"
			"      expected = '%s'\n",
			input, output, expected);
		return 1;
	}
	if (strcmp(output, expected) != 0) {
		printf("Failed: input = '%s'\n"
			"        output = '%s'\n"
			"      expected = '%s'\n",
			input, output, expected);
		free(output);
		return 1;
	}
	free(output);
	return 0;
}

static int testcase_frac_to_words(float input, int max_decimal_places, char *expected)
{
	int rc;
	char output[1000];

	rc = frac_to_words(input, output, 1000, max_decimal_places);
	if (rc < 0) {
		printf("Failed: frac_to_words(%f, ...) returned rc = %d\n", input, rc);
		return 1;
	}
	if (strcmp(expected, output) != 0) {
		printf("Failed: frac_to_words(%f, ...) returned '%s', expected '%s'\n", input, output, expected);
		return 1;
	}
	return 0;
}

static int testcase_integer_to_words(int input, char *expected)
{
	int rc;
	char output[1000];
	rc = integer_to_words(input, output, 1000);
	if (rc < 0) {
		printf("Failed: int_to_words(%d, ...) returned rc = %d\n", input, rc);
		return 1;
	}
	if (strcmp(expected, output) != 0) {
		printf("Failed: int_to_words(%d, ...) returned '%s', expected '%s'\n", input, output, expected);
		return 1;
	}
	return 0;
}

static int testcase_numbers_to_words(float input, int max_decimal_places, char *expected)
{
	int rc;
	char output[1000];
	rc = numbers_to_words(input, max_decimal_places, output, 1000);
	if (rc < 0) {
		printf("Failed: numbers_to_words(%f, ...) returned rc = %d\n", input, rc);
		return 1;
	}
	if (strcmp(expected, output) != 0) {
		printf("Failed: numbers_to_words(%f, ...) returned '%s', expected '%s'\n", input, output, expected);
		return 1;
	}
	return 0;
}

static int test_frac_to_words(void)
{
	int rc = 0;

	rc += testcase_frac_to_words(0.123, 3, "point one two three");
	rc += testcase_frac_to_words(7.123, 2, "point one two");
	rc += testcase_frac_to_words(7.7777, 3, "point seven seven seven");
	rc += testcase_frac_to_words(-7.127777, 4, "point one two seven seven");
	rc += testcase_frac_to_words(-7.127777, 1, "point one");
	rc += testcase_frac_to_words(1.77770e3, 3, "point six nine nine"); /* float precision */
	return rc;
}

static int test_int_to_words(void)
{
	int rc = 0;
	char buffer[1000];

	rc += testcase_integer_to_words(0, "zero");
	rc += testcase_integer_to_words(9, "nine");
	rc += testcase_integer_to_words(10, "ten");
	rc += testcase_integer_to_words(-1, "minus one");
	rc += testcase_integer_to_words(-2, "minus two");
	rc += testcase_integer_to_words(20, "twenty");
	rc += testcase_integer_to_words(25, "twenty five");
	rc += testcase_integer_to_words(-34, "minus thirty four");
	rc += testcase_integer_to_words(100, "one hundred");
	rc += testcase_integer_to_words(500, "five hundred");
	rc += testcase_integer_to_words(520, "five hundred twenty");
	rc += testcase_integer_to_words(1520, "one thousand five hundred twenty");
	rc += testcase_integer_to_words(11520, "eleven thousand five hundred twenty");
	rc += testcase_integer_to_words(211520, "two hundred eleven thousand five hundred twenty");
	rc += testcase_integer_to_words(3000005, "three million five");
	rc += testcase_integer_to_words(3211520, "three million two hundred eleven thousand five hundred twenty");
	rc += testcase_integer_to_words(43211520,
		"forty three million two hundred eleven thousand five hundred twenty");
	rc += testcase_integer_to_words(543211520,
		"five hundred forty three million two hundred eleven thousand five hundred twenty");
}

static int test_numbers_to_words(void)
{
	int rc = 0;

	rc += test_frac_to_words();
	rc += test_int_to_words();

	rc += testcase_numbers_to_words(0.0, 3, "zero");
	rc += testcase_numbers_to_words(1.1, 3, "one point one zero zero");
	rc += testcase_numbers_to_words(13.1, 2, "thirteen point one zero");
	rc += testcase_numbers_to_words(113.1, 3, "one hundred thirteen point zero nine nine");
	rc += testcase_numbers_to_words(113.123, 3, "one hundred thirteen point one two three");
	rc += testcase_numbers_to_words(113.123, 4, "one hundred thirteen point one two three zero");
	rc += testcase_numbers_to_words(1e6, 3, "one million");
	rc += testcase_numbers_to_words(-1.005e6, 3, "minus one million five thousand");
	rc += testcase_numbers_to_words(-1.005033e6, 3, "minus one million five thousand thirty three");
	rc += testcase_numbers_to_words(-1.0005e7, 3, "minus ten million five thousand");
	rc += testcase_numbers_to_words(3.1415927, 6, "three point one four one five nine two");
	rc += testcase_numbers_to_words(3.1415927, 0, "three");

	return rc;
}

int main(int argc, char *argv[])
{
	int rc = 0;
	char x[] = {"this is a test 9 100\n"};
	char y[] = {"20 1"};
	char z[] = {"2 100 64 1000"};
	int s, e;

	rc = find_next_digit_group(x, 0, &s, &e);
	if (rc < 0 || s != 15 || e != 15) {
		fprintf(stderr, "1. find_next_digit_group failed, expected rc = 15, %d,%d, got rc = %d, %d,%d\n",
			16, 16, rc, s, e);
		exit(1);
	}
	rc = find_next_digit_group(x, 16, &s, &e);
	if (rc != 17 || s != 17 || e != 19) {
		fprintf(stderr, "2. find_next_digit_group failed, expected 17, 17, 19, got %d, %d, %d\n", rc, s, e);
		exit(1);
	}
	rc = find_next_digit_group(y, 0, &s, &e);
	if (rc < 0 || s != 0 || e != 1) {
		fprintf(stderr, "1. find_next_digit_group failed, expected rc = 0, %d,%d, got rc = %d, %d,%d\n",
			0, 1, rc, s, e);
		exit(1);
	}
	rc = find_next_digit_group(y, 2, &s, &e);
	if (rc != 3 || s != 3 || e != 3) {
		fprintf(stderr, "4. find_next_digit_group failed, expected 3, 3, 3, got %d, %d, %d\n", rc, s, e);
		exit(1);
	}
	rc = combine_numeric_values_once(y);
	if (strcmp(y, "21") != 0) {
		fprintf(stderr, "Expected '21', got '%s', rc = %d\n", y, rc);
		exit(1);
	}
	rc = combine_numeric_values_once(z);
	if (strcmp(z, "200 64000") != 0) {
		fprintf(stderr, "Expected '200 64000', got '%s', rc = %d\n", z, rc);
		exit(1);
	}

	rc = 0;
	rc += testcase("one hundred", "100");
	rc += testcase("two hundred", "200");

#if 0
	/* Some cases not handled yet ... */
	rc += testcase("nineteen fifty six", "1956");
	rc += testcase("it was a different time, nineteen fifty seven, fifty eight.",
			"it was a different time, 1957, 58.");
	rc += testcase("twentyfour", "24");
#endif

	rc += testcase("two hundred and ten", "210");
	rc += testcase("two hundred and and ten", "200 and and 10");
	rc += testcase("ten thousand and one hundred", "10100");
	rc += testcase("six hundred and seven", "607");
	rc += testcase("six hundred and then seven more", "600 and then 7 more");
	rc += testcase("a hundred", "100");
	rc += testcase("a thousand", "1000");
	rc += testcase("two hundred thousand", "200000");
	rc += testcase("two hundred sixty seven thousand", "267000");
	rc += testcase("three hundred fifty eight", "358");
	rc += testcase("one", "1");
	rc += testcase("two", "2");
	rc += testcase("three", "3");
	rc += testcase("four", "4");
	rc += testcase("five", "5");
	rc += testcase("six", "6");
	rc += testcase("seven", "7");
	rc += testcase("eight", "8");
	rc += testcase("nine", "9");
	rc += testcase("ten", "10");
	rc += testcase("eleven", "11");
	rc += testcase("twelve", "12");
	rc += testcase("thirteen", "13");
	rc += testcase("fourteen", "14");
	rc += testcase("fifteen", "15");
	rc += testcase("sixteen", "16");
	rc += testcase("seventeen", "17");
	rc += testcase("eighteen", "18");
	rc += testcase("nineteen", "19");
	rc += testcase("twenty", "20");
	rc += testcase("twenty one", "21");
	rc += testcase("twenty two", "22");
	rc += testcase("twenty three", "23");
	rc += testcase("twenty four", "24");
	rc += testcase("someone", "someone");
	rc += testcase("one quarter", "0.25");
	rc += testcase("one third", "0.33");
	rc += testcase("one half", "0.50");
	rc += testcase("two thirds", "0.67");
	rc += testcase("three quarters", "0.75");
	rc += testcase("75 percent", "75 ");
	rc += testcase("thirty five percent", "35 ");
	rc += testcase("two thirds percent", "0.67 ");

	/* It is a pretty severe bug that the following test case fails. */
	rc += testcase("one hundred and ten thousand", "110000");

	rc += test_numbers_to_words();

	if (rc) {
		printf("Failed %d out of %d test cases\n", rc, total);
		return 1;
	}
	printf("All test cases pass.\n");
	return 0;
}
#endif

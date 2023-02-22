#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "string-utils.h"

int total_tests = 0;
int tests_passed = 0;
int tests_failed = 0;

void test_strlcpy(int num, const char *src, size_t n, char *expected_dest, size_t expected_rc)
{
	char s[100], d[100];

	int t = tests_failed;
	total_tests++;
	strcpy(s, src);
	memset(d, 'x', 100);
	size_t rc = strlcpy(d, src, n);
	if (rc != expected_rc) {
		printf("strlcpy test %d Failed with '%s', rc = %lu, expected %lu\n",
			num, src, rc, expected_rc);
		tests_failed++;
	}
	if (strcmp(d, expected_dest) != 0) {
		printf("strlcpy test %d Failed with '%s', got '%s', expected '%s'\n",
			num, src, d, expected_dest);
		tests_failed++;
	}
	if (t == tests_failed)
		tests_passed++;
}

static void test_string_transformer(int num, char *funcname, void func(char *s), char *str, char *expected)
{
	char *tmp;

	total_tests++;
	tmp = strdup(str);
	func(tmp);
	if (strcmp(expected, tmp) != 0) {
		printf("%s() failed test %d with '%s', expected '%s', got '%s'\n",
			funcname, num, str, expected, tmp);
		tests_failed++;
	} else {
		tests_passed++;
	}
	free(tmp);
}

static void test_remove_trailing_whitespace(int num, char *str, char *expected)
{
	test_string_transformer(num, "remove_trailing_whitespace", remove_trailing_whitespace, str, expected);
}

static void test_clean_spaces(int num, char *str, char *expected)
{
	test_string_transformer(num, "clean_spaces", clean_spaces, str, expected);
}

static void test_uppercase(int num, char *str, char *expected)
{
	test_string_transformer(num, "uppercase", uppercase, str, expected);
}

static void test_lowercase(int num, char *str, char *expected)
{
	test_string_transformer(num, "lowercase", lowercase, str, expected);
}

static void test_remove_single_quotes(int num, char *str, char *expected)
{
	test_string_transformer(num, "remove_single_quotes", remove_single_quotes, str, expected);
}

static void test_trim_whitespace(int num, char *str, char *expected)
{
	char *tmp = strdup(str);
	int len = strlen(str);
	int f = tests_failed;

	total_tests++;
	char *x = trim_whitespace(tmp);
	if (strcmp(x, expected) != 0) {
		printf("trim_whitespace test %d, '%s' failed, expected '%s', got '%s'\n",
			num, str, expected, x);
		tests_failed++;
	}
	if ((x - tmp) > len || (x - tmp) < 0) {
		printf("trim_whitespace test %d, '%s' failed, returned pointer out of range.\n", num, str);
		tests_failed++;
	}
	if (f == tests_failed)
		tests_passed++;
}

static void test_dir_name(int num, char *str, char *expected)
{
	char *tmp = dir_name(str);
	total_tests++;
	if (strcmp(tmp, expected) != 0) {
		printf("dir_name() failed tests %d, '%s', expected '%s', got '%s'\n",
			num, str, expected, tmp);
		tests_failed++;
	} else {
		tests_passed++;
	}
	free(tmp);
}

static void test_strchrcount(int num, char *str, int c, int expected)
{
	int n = strchrcount(str, c);
	total_tests++;
	if (n != expected) {
		printf("strchrcount() failed tests %d, '%s', expected %d, got %d\n",
			num, str, expected, n);
		tests_failed++;
	} else {
		tests_passed++;
	}
}

static void test_has_prefix(int num, char *prefix, char *str, int expected)
{
	int n = has_prefix(prefix, str);

	total_tests++;
	if (n != expected) {
		tests_failed++;
		printf("Test %d: has_prefix(%s, %s) failed, expected %d, got %d\n", num, prefix, str, expected, n);
	} else {
		tests_passed++;
	}
}

static void test_get_field(int num, char *line, int expected_offset)
{
	char *x = get_field(line);

	total_tests++;
	if (x != line + expected_offset) { /* deliberate pointer comparison */
		tests_failed++;
		printf("Test %d, get_field(%s) failed, expected %p('%s'), got %p('%s')\n",
			num, line, (void *) &line[expected_offset], &line[expected_offset], (void *) x, x);
	} else {
		tests_passed++;
	}
}

static void test_get_abbreviated_command_arg(int num, char *command, char *input, int expected)
{
	char *a = get_abbreviated_command_arg(command, input);

	total_tests++;
	if (expected == -1 && a == NULL) {
		tests_passed++;
		return;
	}
	if ((int) (a - input) != expected) {
		tests_failed++;
		printf("Test %d, get_abbreviated_command_arg(%s, %s) failed, expected offset %d, got %d\n",
			num, command, input, expected, (int) (a - input));
	} else {
		tests_passed++;
	}
}

int main(__attribute__((unused)) int argc, __attribute__((unused)) char *argv[])
{
	test_strlcpy(1, "abcdef", 5, "abcd", 6);
	test_strlcpy(2, "abcdef", 10, "abcdef", 6);
	test_strlcpy(3, "abcdef", 6, "abcde", 6);

	test_remove_trailing_whitespace(4, "abc", "abc");
	test_remove_trailing_whitespace(5, "abc ", "abc");
	test_remove_trailing_whitespace(6, "abc  ", "abc");
	test_remove_trailing_whitespace(7, "abc\t", "abc");
	test_remove_trailing_whitespace(8, "abc\r", "abc");
	test_remove_trailing_whitespace(9, "abc\r\t\n ", "abc");
	test_remove_trailing_whitespace(10, "\r\t\n abc\r\t\n ", "\r\t\n abc");

	test_clean_spaces(11, "abc", "abc");
	test_clean_spaces(12, " abc", "abc");
	test_clean_spaces(13, " abc ", "abc ");
	test_clean_spaces(14, "  abc  ", "abc ");
	test_clean_spaces(15, "  a b c  ", "a b c ");
	test_clean_spaces(16, "  a  b  c  ", "a b c ");
	test_clean_spaces(17, "\ta \rb \n c\t  \t", "a b c ");

	test_trim_whitespace(18, "abc", "abc");
	test_trim_whitespace(19, " abc", "abc");
	test_trim_whitespace(20, " abc ", "abc");
	test_trim_whitespace(21, "  abc  ", "abc");
	test_trim_whitespace(22, "  a b c  ", "a b c");
	test_trim_whitespace(23, "  a  b  c  ", "a  b  c");
	test_trim_whitespace(24, "\ta \rb \n c\t  \t", "a \rb \n c");

	test_uppercase(25, "abc", "ABC");
	test_uppercase(26, "ABC", "ABC");
	test_uppercase(27, "abcABC", "ABCABC");
	test_uppercase(28, "this is a test", "THIS IS A TEST");

	test_lowercase(29, "abc", "abc");
	test_lowercase(30, "ABC", "abc");
	test_lowercase(31, "abcABC", "abcabc");
	test_lowercase(32, "THIS IS A TEST", "this is a test");

	test_dir_name(33, "///", "///");
	test_dir_name(34, "/a/b/c/", "/a/b/c/");
	test_dir_name(35, "/a/b/c", "/a/b/");

	test_has_prefix(36, "abc", "abcdefg", 1);
	test_has_prefix(37, "abc", "abc", 1);
	test_has_prefix(38, "abcd", "abc", 0);
	test_has_prefix(39, "abcd", "dabc", 0);

	test_remove_single_quotes(40, "\'", "");
	test_remove_single_quotes(41, "\'\'", "");
	test_remove_single_quotes(42, "\'\'\'", "");
	test_remove_single_quotes(43, "\'this is a test\'", "this is a test");
	test_remove_single_quotes(44, "don\'t fear the reaper", "dont fear the reaper");

	test_strchrcount(45, "mississippi", 's', 4);
	test_strchrcount(46, "mississippi", 'x', 0);
	test_strchrcount(47, "mississippi", 'i', 4);
	test_strchrcount(48, "mississippi", 'm', 1);
	test_strchrcount(49, "mississippi", 'p', 2);
	test_strchrcount(50, "this is a test ", ' ', 4);

	test_get_field(51, "x: y", 3);
	test_get_field(52, "xxx: \tyyy", 6);
	test_get_field(53, "   xxx: \t7", 9);
	test_get_field(54, "xxx:", 4);

	test_get_abbreviated_command_arg(55, "blast", "blast 1   ", 6);
	test_get_abbreviated_command_arg(56, "blast", "blast    1", 9);
	test_get_abbreviated_command_arg(57, "blast", "blast\t   1\t   ", 5);
	test_get_abbreviated_command_arg(58, "x", "blast\t   1", -1);
	test_get_abbreviated_command_arg(59, "blastx", "bl   1\n", 5);
	test_get_abbreviated_command_arg(60, "vars", "    v   1\n", 8);

	if (tests_passed == total_tests && tests_failed == 0)
		printf("%d tests passed\n", tests_passed);
	else
		printf("%d/%d tests failed, %d/%d tests passed.\n",
			tests_failed, total_tests, tests_passed, total_tests);
	return 0;
}

#ifndef STRING_UTILS_H__
#define STRING_UTILS_H__

void clean_spaces(char *line);
void remove_trailing_whitespace(char *s);
void uppercase(char *w);
void lowercase(char *w);
char *dir_name(char *path); /* returns malloc'ed string */
char *trim_whitespace(char *s); /* trims trailing whitespace */
char *skip_leading_whitespace(char *s); /* returns ptr into string after skipping leading whitespace */
int has_prefix(char *prefix, char *str);
char *slurp_file(const char *path, int *bytes); /* returns allocated string containingg contents of file */
void remove_single_quotes(char *text);
int strchrcount(char *s, int c); /* returns count of occurrences of c within s */

#if USE_CUSTOM_STRLCPY == 1
size_t strlcpy(char *dest, const char *src, size_t n);
#else
#include <bsd/string.h>
#endif

void format_function_pointer(char *buffer, void (*function_pointer)(void));

/* Given a string of the form "xxxxx : yyyyy", return "yyyyy"
 * Basically, return the part of the string after the colon,
 * skipping any leading whitespace after the colon.  This does
 * not allocate, it returns a pointer inside the given string.
 */
char *get_field(char *line);

#endif

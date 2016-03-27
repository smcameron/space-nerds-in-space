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

#endif

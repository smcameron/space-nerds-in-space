#ifndef STRING_UTILS_H__
#define STRING_UTILS_H__

void clean_spaces(char *line);
void remove_trailing_whitespace(char *s);
void uppercase(char *w);
char *dir_name(char *path); /* returns malloc'ed string */
char *trim_whitespace(char *s);
int has_prefix(char *prefix, char *str);

#endif

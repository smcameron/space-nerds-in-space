#ifndef STRING_UTILS_H__
#define STRING_UTILS_H__

void clean_spaces(char *line);
void remove_trailing_whitespace(char *s);
void uppercase(char *w);
void lowercase(char *w);
char *dir_name(char *path); /* returns malloc'ed string */
char *trim_whitespace(char *s); /* trims trailing whitespace */
int has_prefix(char *prefix, char *str);
char *slurp_file(const char *path, int *bytes); /* returns allocated string containingg contents of file */
void remove_single_quotes(char *text);
int strchrcount(char *s, int c); /* returns count of occurrences of c within s */
size_t strlcpy(char *dest, const char *src, size_t n);

void format_function_pointer(char *buffer, void (*function_pointer)(void));

/* Given a string of the form "xxxxx : yyyyy", return "yyyyy"
 * Basically, return the part of the string after the colon,
 * skipping any leading whitespace after the colon.  This does
 * not allocate, it returns a pointer inside the given string.
 */
char *get_field(char *line);

/* get_abbreviated_command_arg is for processing console commands.
 * There are commands like * "DESCRIBE BLAH". But the user does not have to
 * type DESCRIBE, they just have to type enough of DESCRIBE to be unique
 * among the various commands, so "DESC BLAH" or even "DE BLAH" should work,
 * if unique.  But then we need to find the command argument after whatever
 * they typed. so,
 *
 *   get_abbreviated_command_arg("DESCRIBE", "DESC BLAH");
 *
 * should return a pointer to the B in DESC BLAH
 *
 *   get_abbreviated_command_arg("DESCRIBE", "DECSRIBE BLAH");
 *
 * should return NULL. (DECSCRIBE is misspelled).
 */
char *get_abbreviated_command_arg(char *expected_command, char *user_input);

void print_args(int argc, char *argv[]);


/* Returns true if a port range is formatted correctly, that is sscanf can scan it as %d%*[:,-]%d"
   (two integers separated by a colon, comma or dash) and if the first integer is less than or
   equal to the 2nd integer, and the first integer > 1023, and the 2nd integer less than or equal
   to 65535. */
int port_range_formatted_correctly(char *port_range);

/* Copy program arguments into an something suitable for calling execv() */
void save_args(int argc, char *argv[], char ***saved_argv);
/* Free args allocated by save_args(); */
void free_argv(char **argv);


#endif

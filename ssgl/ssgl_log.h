#ifndef SSGL_LOGFILE_H__
#define SSGL_LOGFILE_H__

extern FILE *ssgl_logfile;
#define SSGL_INFO 1
#define SSGL_WARN 2
#define SSGL_ERROR 3

extern int ssgl_open_logfile(char *logfilevariable);
extern void ssgl_log(int level, const char* format, ...);
extern void ssgl_close_logfile(void);
extern void ssgl_set_log_level(int level);

#endif

#ifndef SSGL_LOGFILE_H__
#define SSGL_LOGFILE_H__

extern FILE *ssgl_logfile;

extern int ssgl_open_logfile(char *logfilename);
extern void ssgl_log(const char* format, ...);
extern void ssgl_close_logfile(void);

#endif

#include <stdio.h>
#include <stdarg.h>
#include <sys/time.h>
#include <time.h>

FILE *ssgl_logfile;

int ssgl_open_logfile(char *logfilename)
{
	ssgl_logfile = fopen(logfilename, "a+");
	return ssgl_logfile == NULL;
}

void ssgl_log(const char* format, ...)
{
        va_list args;
	struct timeval tv;
	time_t now;
	struct tm nowtm;
	char tmbuf[64], buf[64];

	gettimeofday(&tv, NULL);
	now = tv.tv_sec;
	localtime_r(&now, &nowtm);
	strftime(tmbuf, sizeof(tmbuf), "%Y-%m-%d %H:%M:%S", &nowtm);
	snprintf(buf, sizeof buf, "%s.%06ld", tmbuf, tv.tv_usec);

        fprintf(ssgl_logfile, "%s: ", buf);
        va_start( args, format );
        vfprintf(ssgl_logfile, format, args);
        va_end( args );
	fflush(ssgl_logfile);
}

void ssgl_close_logfile(void)
{
	if (ssgl_logfile)
		fclose(ssgl_logfile);
	ssgl_logfile = NULL;
}


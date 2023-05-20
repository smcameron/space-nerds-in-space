#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>

#include "ssgl_log.h"

FILE *ssgl_logfile;
static char *default_logfilename = "/dev/null";

static int loglevel = 2;

void ssgl_set_log_level(int level)
{
	loglevel = level;
	ssgl_log(loglevel, "ssgl log level set to %d\n", loglevel);
}

int ssgl_open_logfile(char *logfilevariable)
{
	char *loglevelstring;
	char *logfilename;
	int ll = -1;

	logfilename = getenv(logfilevariable);
	if (!logfilename)
		logfilename = default_logfilename;
	ssgl_logfile = fopen(logfilename, "a+");
	loglevelstring = getenv("SSGL_LOG_LEVEL");	
	if (ssgl_logfile && loglevelstring && 1 == sscanf(loglevelstring, "%d", &ll))
		ssgl_set_log_level(ll);
	return ssgl_logfile == NULL;
}

void ssgl_log(int level, const char* format, ...)
{
        va_list args;
	struct timeval tv;
	time_t now;
	struct tm nowtm;
	char tmbuf[64], buf[128];

	if (level < loglevel)
		return;

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


#include <signal.h>
#include <execinfo.h>
#include <stdio.h>
#include <stdlib.h>

#include "stacktrace.h"

void stacktrace(char *msg)
{
	void *trace[30];
	char **traceline = (char **) NULL;
	int i, trace_size = 0;

	trace_size = backtrace(trace, 30);
	traceline = backtrace_symbols(trace, trace_size);
	if (!traceline) {
		fprintf(stderr, "backtrace_symbols failed, no stack trace.\n");
		return;
	}
	fprintf(stderr, "%s\n", msg);
	fprintf(stderr, "Stack trace:\n");
	for (i=0; i < trace_size; ++i)
		fprintf(stderr, "- %s\n", traceline[i]);
	free(traceline);
}

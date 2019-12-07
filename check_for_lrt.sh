#!/bin/sh

quiet=0;
if [ "$1" = "-q" ]
then
	quiet=1
fi

TMPFILE=/tmp/clock_gettime_test$$
cat << EOF > "${TMPFILE}".c
#include <stdio.h>
#include <time.h>

int main(int argc, char *argv[])
{
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	printf("%ld, %ld\n", ts.tv_sec, ts.tv_nsec);

	return 0;
}
EOF

qecho()
{
	if [ "$quiet" = "0" ]
	then
		echo $*
	fi
}

qecho -n "  CHECKING for -lrt clock_gettime requirement " 1>&2
gcc -o "${TMPFILE}".x "${TMPFILE}".c > /dev/null 2>&1
if [ "$?" = "0" ]
then
	qecho "... OK without -lrt"
	echo ""
else
	qecho "... not OK without -lrt."
	qecho -n "  CHECKING for -lrt clock_gettime requirement " 1>&2
	gcc -o "${TMPFILE}".x "${TMPFILE}".c -lrt > /dev/null 2>&1
	if [ "$?" = "0" ]
	then
		qecho "... OK with -lrt"
		echo "-lrt"
	else
		qecho "... not OK with or without -lrt"
		echo "-lrt"
	fi
fi

/bin/rm -f "${TMPFILE}".c
/bin/rm -f "${TMPFILE}".x

return 0

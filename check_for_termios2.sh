#!/bin/sh

TMPFILE=/tmp/termios_test$$
cat << EOF > "${TMPFILE}".c
#include <termios.h>
#include <stdio.h>

int main(int argc, char *argv[])
{
	struct termios2 blah;

	printf("%p\n", &blah);
	return 0;
}
EOF

printf "  CHECKING for struct termios2" 1>&2
gcc -c "${TMPFILE}".c > /dev/null 2>&1
if [ "$?" = "0" ]
then
	echo "... OK"
	echo > local_termios2.h
else
	echo "... not OK. We will define it ourself."
	cat termios2.h > local_termios2.h
fi

/bin/rm -f "${TMPFILE}".c
/bin/rm -f "${TMPFILE}".o

return 0

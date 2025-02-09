#!/bin/sh
#
# This script edits snis_launcher to insert the correct prefix
# It's called by "make install"
#
# Typical usage:
#     modify_snis_launcher.sh snis_launcher /usr/local > bin/snis_launcher
#

INPUT="$1"
DESTDIR="$2"
PREFIX="$3"

if [ "$PREFIX" = "" ]
then
	cat "$INPUT"
	exit 0
fi

if [ "$PREFIX" = "." -a "$DESTDIR" = "" ]
then
	cat "$INPUT"
	exit 0
fi

# replace slashes in PREFIX and DESTDIR with escaped slashes
export PREFIX
export DESTDIR
ESCAPED_PREFIX=$(echo "$PREFIX" | sed -e 's/[/]/\\\//g')
ESCAPED_DESTDIR=$(echo "$DESTDIR" | sed -e 's/[/]/\\\//g')


# modify input to replace "PREFIX=." with the correct prefix value
sed -e 's/^PREFIX=[.]$/PREFIX='"$ESCAPED_PREFIX"'/g' -e 's/^DESTDIR=$/DESTDIR='"$ESCAPED_DESTDIR"'/g' < "$INPUT"


#!/bin/sh
#
# This script edits snis_launcher to insert the correct prefix
# It's called by "make install"
#
# Typical usage:
#     modify_snis_launcher.sh snis_launcher /usr/local > bin/snis_launcher
#

INPUT="$1"
PREFIX="$2"

if [ "$PREFIX" = "" ]
then
	cat "$INPUT"
	exit 0
fi

if [ "$PREFIX" = "." ]
then
	cat "$INPUT"
	exit 0
fi

# replace slashes in PREFIX with escaped slashes
export PREFIX
ESCAPED_PREFIX=$(echo "$PREFIX" | sed -e 's/[/]/\\\//g')

# modify input to replace "PREFIX=." with the correct prefix value
sed -e 's/^PREFIX=[.]$/PREFIX='"$ESCAPED_PREFIX"'/g' < "$INPUT"


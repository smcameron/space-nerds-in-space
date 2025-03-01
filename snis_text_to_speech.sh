#!/bin/sh
#
#	Copyright (C) 2016 Stephen M. Cameron
#	Author: Stephen M. Cameron
#
#	This file is part of Spacenerds In Space.
#
#	Spacenerds in Space is free software; you can redistribute it and/or modify
#	it under the terms of the GNU General Public License as published by
#	the Free Software Foundation; either version 2 of the License, or
#	(at your option) any later version.
#
#	Spacenerds in Space is distributed in the hope that it will be useful,
#	but WITHOUT ANY WARRANTY; without even the implied warranty of
#	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#	GNU General Public License for more details.
#
#	You should have received a copy of the GNU General Public License
#	along with Spacenerds in Space; if not, write to the Free Software
#	Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
#
# Usage:
#
# snis_text_to_speech.sh 'text you want it to speak'
#
# If this script isn't working for you, try:
#
#    snis_text_to_speech.sh --debug
#

echo "snis_text_to_speech.sh $1" 1>&2

export SNISTTSLOCKDIR=/tmp/snisttslockdir

if [ "${SNIS_TTS_VOLUME}" = "" ]
then
	SNIS_TTS_VOLUME="0.33"
fi

check_nanotts()
{
	nanotts -c 'hello' > /dev/null 2>&1
	if [ "$?" = "0" ]
	then
		echo "    OK - nanotts is present and executable"
	else
		echo "NOT OK!!!   Failed to run nanotts"
	fi
}

check_presence()
{
	if [ "$1" = "nanotts" ]
	then
		check_nanotts
		return
	fi
	program="$1"
	arg="$2"
	expected="$3"
	if [ -x /usr/bin/"$program" ]
	then
		echo "    OK - $program is present and executable"
	else
		echo "    OK - /usr/bin/$program does not seem to exist.  Maybe it is in a weird place."
	fi
	$program "$arg" > /dev/null 2>&1
	code="$?"
	if [ "$code" = "127" ]
	then
		echo "NOT OK!!!   Failed to run $program"
	fi
	if [ "$code" != "$expected" ]
	then
		echo "NOT OK!!! Attempting to run $program got exit code $code instead of $expected"
	else
		echo "    OK - $program seems OK"
	fi
}

if [ "$1" = "--debug" ]
then
	echo "snis_text_to_speech debugging mode enabled"
	echo "Testing /tmp"
	stat /tmp
	if [ "$?" != "0" ]
	then
		echo "NOT OK!!! Failed to stat /tmp" 1>&2
		exit 1
	fi
	echo "    OK - Checking permissions on /tmp"
	stat /tmp | grep 'Access.*1777[/]drwxrwxrwt' > /dev/null 2>&1
	if [ "$?" != "0" ]
	then
		echo "NOT OK!!! Permissions on /tmp do not seem to be correct, should be 1777"
	else
		echo "    OK - Permissions on /tmp seem to be ok (1777)"
	fi
	echo "    OK - Checking if we can create a directory in /tmp"
	TMPFILE=/tmp/snistmp$$
	mkdir "$TMPFILE" || (echo "NOT OK!!! Failed to create dir in /tmp" && exit 1)
	echo "    OK - Created directory in /tmp OK"
	echo "    Ok - Checking if we can remove the directory in /tmp"
	rmdir "$TMPFILE" || (echo "NOT OK!!! Failed to rmdir $TMPFILE" && exit 1)
	echo "    OK - Removed directory in /tmp OK"
	check_presence nanotts
	check_presence play /dev/null 2
	check_presence aplay /dev/null 1
	check_presence pico2wave x 1
	check_presence espeak '' 0
	exit 1
fi

release_lock()
{
	/bin/rmdir "$SNISTTSLOCKDIR"
}


get_lock()
{
	count=0
	worked=0
	# Try for 50 seconds -- this needs to be somewhat long so we don't
	# give up while another message is playing
	# TODO: implement a proper ordered queue for the messages instead of using
	# OS processes contending for a lock as an implicit unordered queue
	while [ $count -lt 200 ]
	do
		mkdir "$SNISTTSLOCKDIR" > /dev/null 2>&1
		if [ "$?" = "0" ]
		then
			trap release_lock INT EXIT
			worked=1;
			break;
		fi
		count=$(expr $count + 1)
		sleep 0.25
	done
	if [ "$worked" != "1" ]
	then
		exit 1
	fi
}

do_text_to_speech()
{
	get_lock
	nanotts -v en-GB -p "$1" 2>/dev/null
	if [ "$?" = "0" ]
	then
		return
	fi 
	echo "nanotts didn't work"
	tmpfile=/tmp/tts-$$.wav
	pico2wave -l=en-GB -w "$tmpfile" "$1" || espeak "$1"
	play -q --volume "${SNIS_TTS_VOLUME}" "$tmpfile" > /dev/null 2>&1 || aplay "$tmpfile" > /dev/null 2>&1
	# mplayer /tmp/x.wav
	/bin/rm -f "$tmpfile"
}

# We no longer run this in the background as now snis_client has an internal queue
# for text to speech and should not invoke this script more than once concurrently.
do_text_to_speech "$1"


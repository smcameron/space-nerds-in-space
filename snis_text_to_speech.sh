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

export SNISTTSLOCKDIR=/tmp/snisttslockdir

release_lock()
{
	/bin/rmdir "$SNISTTSLOCKDIR"
}


get_lock()
{
	count=0
	worked=0
	while [ $count -lt 20 ]
	do
		mkdir "$SNISTTSLOCKDIR" > /dev/null 2>&1
		if [ "$?" = "0" ]
		then
			trap release_lock INT EXIT
			worked=1;
			break;
		fi
		count=$(expr $count + 1)
		sleep 1
	done
	if [ "$worked" != "1" ]
	then
		exit 1
	fi
}

do_text_to_speech()
{
	get_lock
	tmpfile=/tmp/tts-$$.wav
	pico2wave -l=en-GB -w "$tmpfile" "$1" || espeak "$1"
	play -q --volume 0.33 "$tmpfile" > /dev/null 2>&1 || aplay "$tmpfile" > /dev/null 2>&1
	# mplayer /tmp/x.wav
	/bin/rm -f "$tmpfile"
}

do_text_to_speech "$1" &


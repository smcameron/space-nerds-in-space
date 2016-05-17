#!/bin/sh

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
	aplay "$tmpfile" > /dev/null 2>&1
	# mplayer /tmp/x.wav
	/bin/rm -f "$tmpfile"
}

do_text_to_speech "$1" &


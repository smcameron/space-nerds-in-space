#!/bin/sh

do_text_to_speech()
{
	tmpfile=/tmp/tts-$$.wav
	pico2wave -w "$tmpfile" "$1" || espeak "$1"
	aplay "$tmpfile" > /dev/null 2>&1
	# mplayer /tmp/x.wav
	/bin/rm -f "$tmpfile"
}

do_text_to_speech "$1" &


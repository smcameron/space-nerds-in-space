#ifndef SNIS_SPEAK_H__
#define SNIS_SPEAK_H__
/*
	This file (snis_speak.h) is entirely machine-generated. To the extent
	possible under law, the author(s) have dedicated all copyright and related and
	neighboring rights to this software to the public domain worldwide.  This
	software is distributed without any warranty.

	You should have received a copy of the CC0 Public Domain Dedication along with
	this software. If not, see <http://creativecommons.org>.
*/

/*
 * Say text aloud, at volume (0.0 to 1.0).  text has had its pronunciation
 * fixed and contains no single quotes.
 *
 * snis_speak.c runs snis_text_to_speech.sh and waits for it to finish.
 * snis_speak_web.c hands text to the browser's speech synthesis, which
 * queues it and returns at once.
 */

#ifdef DEFINE_SNIS_SPEAK_GLOBALS
#define GLOBAL
#else
#define GLOBAL extern
#endif

GLOBAL void snis_speak(const char *text, float volume);

#undef GLOBAL
#endif

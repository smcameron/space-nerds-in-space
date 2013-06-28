#ifndef SNIS_KEYBOARD_H_
#define SNIS_KEYBOARD_H_

enum keyaction {
		keynone		= 0,
		keydown		= 1,
		keyup		= 2,
		keyleft		= 3,
		keyright	= 4,
                keytorpedo	= 5,
		keytransform	= 6,
                keyfullscreen	= 7,
		keythrust	= 8,
                keyquit		= 9,
                keypausehelp	= 10,
		keyreverse	= 11,
		keyf1		= 12,
		keyf2		= 13,
		keyf3		= 14,
		keyf4		= 15,
		keyf5		= 16,
                keyf6		= 17,
		keyf7		= 18,
		keyf8		= 19,
		keyf9		= 20,
		keyonscreen	= 21,
		keyviewmode	= 22,
		keyzoom		= 23,
		keyunzoom	= 24,
		keyphaser	= 25
#define NKEYSTATES 26
};

struct keyboard_state {
	int pressed[NKEYSTATES];
};

#endif


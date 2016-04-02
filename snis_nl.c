/*
	Copyright (C) 2016 Stephen M. Cameron
	Author: Stephen M. Cameron

	This file is part of Spacenerds In Space.

	Spacenerds in Space is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	Spacenerds in Space is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with Spacenerds in Space; if not, write to the Free Software
	Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "snis_nl.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>

#include "arraysize.h"
#include "string-utils.h"

#define POS_UNKNOWN		0
#define POS_NOUN		1
#define POS_VERB		2
#define POS_ARTICLE		3
#define POS_PREPOSITION		4
#define POS_SEPARATOR		5
#define POS_ADJECTIVE		6
#define POS_ADVERB		7
#define POS_NUMBER		8
#define POS_NAME		9
#define POS_PRONOUN		10

static const char * const part_of_speech[] = {
	"unknown",
	"noun",
	"verb",
	"article",
	"preposition",
	"separator",
	"adjective",
	"adverb",
	"number",
	"name",
	"pronoun",
};

struct verb_data {
	char *syntax;	/* Syntax of verb, denoted by characters.
			 * 'n' -- single noun (or pronoun)
			 * 'l' -- one or more nouns (or pronoun)
			 * 'p' -- preposition
			 * 'q' -- quantity, that is to say, a number.
			 * 'a' -- adjective
			 *
			 * For example: "put", as in "put the butter on the bread with the knife"
			 * has a syntax of "npnpn", while "put" as in "put the coat on",
			 * has a syntax of "np".
			 */
};

struct noun_data {
	int blah;
};

struct article_data {
	int definite;
};

struct preposition_data {
	int blah;
};

struct separator_data {
	int blah;
};

struct adjective_data {
	int blah;
};

struct adverb_data {
	int blah;
};

struct number_data {
	float value;
};

struct name_data {
	uint32_t id;
};

struct pronoun_data {
	int blah;
};

#define MAX_SYNONYMS 100
static int nsynonyms = 0;
static struct synonym_entry {
	char *syn, *canonical;
} synonym[MAX_SYNONYMS + 1] = { { 0 } }; 

static struct dictionary_entry {
	char *word;
	char *canonical_word;
	int p_o_s; /* part of speech */
	__extension__ union {
		struct noun_data noun;
		struct verb_data verb;
		struct article_data article;
		struct preposition_data preposition;
		struct separator_data separator;
		struct adjective_data adjective;
		struct adverb_data adverb;
		struct number_data number;
		struct pronoun_data pronoun;
	};
} dictionary[] = {
	{ "navigate",		"navigate",	POS_VERB, .verb = { "pn" }, },
	{ "set",		"set",		POS_VERB, .verb = { "npq" }, },
	{ "set",		"set",		POS_VERB, .verb = { "npa" }, },
	{ "lower",		"lower",	POS_VERB, .verb = { "npq" }, },
	{ "lower",		"lower",	POS_VERB, .verb = { "n" }, },
	{ "raise",		"raise",	POS_VERB, .verb = { "nq" }, },
	{ "raise",		"raise",	POS_VERB, .verb = { "npq" }, },
	{ "raise",		"raise",	POS_VERB, .verb = { "n" }, },
	{ "engage",		"engage",	POS_VERB, .verb = { "n" }, },
	{ "disengage",		"disengage",	POS_VERB, .verb = { "n" }, },
	{ "turn",		"turn",		POS_VERB, .verb = { "pn" }, },
	{ "turn",		"turn",		POS_VERB, .verb = { "aq" }, },
	{ "compute",		"compute",	POS_VERB, .verb = { "npn" }, },
	{ "report",		"report",	POS_VERB, .verb = { "n" }, },
	{ "yaw",		"yaw",		POS_VERB, .verb = { "aq" }, },
	{ "pitch",		"pitch",	POS_VERB, .verb = { "aq" }, },
	{ "roll",		"roll",		POS_VERB, .verb = { "aq" }, },
	{ "zoom",		"zoom",		POS_VERB, .verb = { "p" }, },
	{ "zoom",		"zoom",		POS_VERB, .verb = { "pq" }, },
	{ "shut",		"shut",		POS_VERB, .verb = { "an" }, },
	{ "shut",		"shut",		POS_VERB, .verb = { "na" }, },
	{ "launch",		"launch",	POS_VERB, .verb = { "n" }, },
	{ "eject",		"eject",	POS_VERB, .verb = { "n" }, },
	{ "full",		"full",		POS_VERB, .verb = { "n" }, },

	{ "drive",		"drive",	POS_NOUN, .noun = { 0 }, },
	{ "system",		"system",	POS_NOUN, .noun = { 0 }, },
	{ "starbase",		"starbase",	POS_NOUN, .noun = { 0 }, },
	{ "base",		"starbase",	POS_NOUN, .noun = { 0 }, },
	{ "planet",		"planet",	POS_NOUN, .noun = { 0 }, },
	{ "ship",		"ship",		POS_NOUN, .noun = { 0 }, },
	{ "bot",		"bot",		POS_NOUN, .noun = { 0 }, },
	{ "shields",		"shields",	POS_NOUN, .noun = { 0 }, },
	{ "throttle",		"throttle",	POS_NOUN, .noun = { 0 }, },
	{ "factor",		"factor",	POS_NOUN, .noun = { 0 }, },
	{ "coolant",		"coolant",	POS_NOUN, .noun = { 0 }, },
	{ "level",		"level",	POS_NOUN, .noun = { 0 }, },
	{ "energy",		"energy",	POS_NOUN, .noun = { 0 }, },
	{ "power",		"energy",	POS_NOUN, .noun = { 0 }, },
	{ "asteroid",		"asteroid",	POS_NOUN, .noun = { 0 }, },
	{ "nebula",		"nebula",	POS_NOUN, .noun = { 0 }, },
	{ "star",		"star",		POS_NOUN, .noun = { 0 }, },
	{ "range",		"range",	POS_NOUN, .noun = { 0 }, },
	{ "distance",		"range",	POS_NOUN, .noun = { 0 }, },
	{ "weapons",		"weapons",	POS_NOUN, .noun = { 0 }, },
	{ "screen",		"screen",	POS_NOUN, .noun = { 0 }, },
	{ "robot",		"robot",	POS_NOUN, .noun = { 0 }, },
	{ "torpedo",		"torpedo",	POS_NOUN, .noun = { 0 }, },
	{ "phasers",		"phasers",	POS_NOUN, .noun = { 0 }, },
	{ "maneuvering",	"maneuvering",	POS_NOUN, .noun = { 0 }, },
	{ "thruster",		"thrusters",	POS_NOUN, .noun = { 0 }, },
	{ "thrusters",		"thrusters",	POS_NOUN, .noun = { 0 }, },
	{ "sensor",		"sensors",	POS_NOUN, .noun = { 0 }, },
	{ "science",		"science",	POS_NOUN, .noun = { 0 }, },
	{ "comms",		"comms",	POS_NOUN, .noun = { 0 }, },
	{ "enemy",		"enemy",	POS_NOUN, .noun = { 0 }, },
	{ "derelict",		"derelict",	POS_NOUN, .noun = { 0 }, },
	{ "computer",		"computer",	POS_NOUN, .noun = { 0 }, },
	{ "fuel",		"fuel",		POS_NOUN, .noun = { 0 }, },
	{ "radiation",		"radiation",	POS_NOUN, .noun = { 0 }, },
	{ "wavelength",		"wavelength",	POS_NOUN, .noun = { 0 }, },
	{ "charge",		"charge",	POS_NOUN, .noun = { 0 }, },
	{ "magnets",		"magnets",	POS_NOUN, .noun = { 0 }, },
	{ "gate",		"gate",		POS_NOUN, .noun = { 0 }, },
	{ "percent",		"percent",	POS_NOUN, .noun = { 0 }, },
	{ "sequence",		"sequence",	POS_NOUN, .noun = { 0 }, },
	{ "core",		"core",		POS_NOUN, .noun = { 0 }, },
	{ "code",		"code",		POS_NOUN, .noun = { 0 }, },
	{ "hull",		"hull",		POS_NOUN, .noun = { 0 }, },
	{ "scanner",		"scanner",	POS_NOUN, .noun = { 0 }, },
	{ "scanners",		"scanners",	POS_NOUN, .noun = { 0 }, },
	{ "detail",		"details",	POS_NOUN, .noun = { 0 }, },
	{ "report",		"report",	POS_NOUN, .noun = { 0 }, },
	{ "damage",		"damage",	POS_NOUN, .noun = { 0 }, },
	{ "course",		"course",	POS_NOUN, .noun = { 0 }, },


	{ "a",			"a",		POS_ARTICLE, .article = { 0 }, },
	{ "an",			"an",		POS_ARTICLE, .article = { 0 }, },
	{ "the",		"the",		POS_ARTICLE, .article = { 1 }, },

	{ "above",		"above",	POS_PREPOSITION, .preposition = { 0 }, },
	{ "aboard",		"aboard",	POS_PREPOSITION, .preposition = { 0 }, },
	{ "across",		"across",	POS_PREPOSITION, .preposition = { 0 }, },
	{ "after",		"after",	POS_PREPOSITION, .preposition = { 0 }, },
	{ "along",		"along",	POS_PREPOSITION, .preposition = { 0 }, },
	{ "alongside",		"alongside",	POS_PREPOSITION, .preposition = { 0 }, },
	{ "at",			"at",		POS_PREPOSITION, .preposition = { 0 }, },
	{ "atop",		"atop",		POS_PREPOSITION, .preposition = { 0 }, },
	{ "around",		"around",	POS_PREPOSITION, .preposition = { 0 }, },
	{ "before",		"before",	POS_PREPOSITION, .preposition = { 0 }, },
	{ "behind",		"behind",	POS_PREPOSITION, .preposition = { 0 }, },
	{ "beneath",		"beneath",	POS_PREPOSITION, .preposition = { 0 }, },
	{ "below",		"below",	POS_PREPOSITION, .preposition = { 0 }, },
	{ "beside",		"beside",	POS_PREPOSITION, .preposition = { 0 }, },
	{ "besides",		"besides",	POS_PREPOSITION, .preposition = { 0 }, },
	{ "between",		"between",	POS_PREPOSITION, .preposition = { 0 }, },
	{ "by",			"by",		POS_PREPOSITION, .preposition = { 0 }, },
	{ "down",		"down",		POS_PREPOSITION, .preposition = { 0 }, },
	{ "during",		"during",	POS_PREPOSITION, .preposition = { 0 }, },
	{ "except",		"except",	POS_PREPOSITION, .preposition = { 0 }, },
	{ "for",		"for",		POS_PREPOSITION, .preposition = { 0 }, },
	{ "from",		"from",		POS_PREPOSITION, .preposition = { 0 }, },
	{ "in",			"in",		POS_PREPOSITION, .preposition = { 0 }, },
	{ "inside",		"inside",	POS_PREPOSITION, .preposition = { 0 }, },
	{ "of",			"of",		POS_PREPOSITION, .preposition = { 0 }, },
	{ "off",		"off",		POS_PREPOSITION, .preposition = { 0 }, },
	{ "on",			"on",		POS_PREPOSITION, .preposition = { 0 }, },
	{ "onto",		"onto",		POS_PREPOSITION, .preposition = { 0 }, },
	{ "out",		"out",		POS_PREPOSITION, .preposition = { 0 }, },
	{ "outside",		"outside",	POS_PREPOSITION, .preposition = { 0 }, },
	{ "through",		"through",	POS_PREPOSITION, .preposition = { 0 }, },
	{ "throughout",		"throughout",	POS_PREPOSITION, .preposition = { 0 }, },
	{ "to",			"to",		POS_PREPOSITION, .preposition = { 0 }, },
	{ "toward",		"toward",	POS_PREPOSITION, .preposition = { 0 }, },
	{ "under",		"under",	POS_PREPOSITION, .preposition = { 0 }, },
	{ "up",			"up",		POS_PREPOSITION, .preposition = { 0 }, },
	{ "until",		"until",	POS_PREPOSITION, .preposition = { 0 }, },
	{ "with",		"with",		POS_PREPOSITION, .preposition = { 0 }, },
	{ "within",		"within",	POS_PREPOSITION, .preposition = { 0 }, },
	{ "without",		"without",	POS_PREPOSITION, .preposition = { 0 }, },

	{ "or",			"or",		POS_SEPARATOR, .separator = { 0 }, },
	{ "and",		"and",		POS_SEPARATOR, .separator = { 0 }, },
	{ "then",		"then",		POS_SEPARATOR, .separator = { 0 }, },
	{ ",",			",",		POS_SEPARATOR, .separator = { 0 }, },
	{ ".",			".",		POS_SEPARATOR, .separator = { 0 }, },
	{ ";",			";",		POS_SEPARATOR, .separator = { 0 }, },
	{ "!",			"!",		POS_SEPARATOR, .separator = { 0 }, },
	{ "?",			"?",		POS_SEPARATOR, .separator = { 0 }, },

	{ "damage",		"damage",	POS_ADJECTIVE, .adjective = { 0 }, },
	{ "status",		"status",	POS_ADJECTIVE, .adjective = { 0 }, },
	{ "warp",		"warp",		POS_ADJECTIVE, .adjective = { 0 }, },
	{ "impulse",		"impulse",	POS_ADJECTIVE, .adjective = { 0 }, },
	{ "docking",		"docking",	POS_ADJECTIVE, .adjective = { 0 }, },
	{ "star",		"star",		POS_ADJECTIVE, .adjective = { 0 }, },
	{ "space",		"space",	POS_ADJECTIVE, .adjective = { 0 }, },
	{ "mining",		"mining",	POS_ADJECTIVE, .adjective = { 0 }, },
	{ "energy",		"energy",	POS_ADJECTIVE, .adjective = { 0 }, },
	{ "main",		"main",		POS_ADJECTIVE, .adjective = { 0 }, },
	{ "navigation",		"navigation",	POS_ADJECTIVE, .adjective = { 0 }, },
	{ "comms",		"comms",	POS_ADJECTIVE, .adjective = { 0 }, },
	{ "engineering",	"engineering",	POS_ADJECTIVE, .adjective = { 0 }, },
	{ "science",		"science",	POS_ADJECTIVE, .adjective = { 0 }, },
	{ "enemy",		"enemy",	POS_ADJECTIVE, .adjective = { 0 }, },
	{ "derelict",		"derelict",	POS_ADJECTIVE, .adjective = { 0 }, },
	{ "solar",		"solar",	POS_ADJECTIVE, .adjective = { 0 }, },
	{ "nearest",		"nearest",	POS_ADJECTIVE, .adjective = { 0 }, },
	{ "closest",		"nearest",	POS_ADJECTIVE, .adjective = { 0 }, },
	{ "nearby",		"nearest",	POS_ADJECTIVE, .adjective = { 0 }, },
	{ "close",		"nearest",	POS_ADJECTIVE, .adjective = { 0 }, },
	{ "up",			"up",		POS_ADJECTIVE, .adjective = { 0 }, },
	{ "down",		"down",		POS_ADJECTIVE, .adjective = { 0 }, },
	{ "left",		"left",		POS_ADJECTIVE, .adjective = { 0 }, },
	{ "right",		"right",	POS_ADJECTIVE, .adjective = { 0 }, },
	{ "self",		"self",		POS_ADJECTIVE, .adjective = { 0 }, },
	{ "destruct",		"destruct",	POS_ADJECTIVE, .adjective = { 0 }, },
	{ "self-destruct",	"self-destruct",	POS_ADJECTIVE, .adjective = { 0 }, },
	{ "short",		"short",	POS_ADJECTIVE, .adjective = { 0 }, },
	{ "long",		"long",		POS_ADJECTIVE, .adjective = { 0 }, },
	{ "range",		"range",	POS_ADJECTIVE, .adjective = { 0 }, },
	{ "full",		"maximum",	POS_ADJECTIVE, .adjective = { 0 }, },
	{ "max",		"maximum",	POS_ADJECTIVE, .adjective = { 0 }, },
	{ "maximum",		"maximum",	POS_ADJECTIVE, .adjective = { 0 }, },

	{ "percent",		"percent",	POS_ADVERB, .adverb = { 0 }, },
	{ "quickly",		"quickly",	POS_ADVERB, .adverb = { 0 }, },
	{ "rapidly",		"quickly",	POS_ADVERB, .adverb = { 0 }, },
	{ "swiftly",		"quickly",	POS_ADVERB, .adverb = { 0 }, },
	{ "slowly",		"slowly",	POS_ADVERB, .adverb = { 0 }, },

	{ "it",			"it",		POS_PRONOUN, .pronoun = { 0 }, },
	{ "me",			"me",		POS_PRONOUN, .pronoun = { 0 }, },
	{ "them",		"them",		POS_PRONOUN, .pronoun = { 0 }, },
	{ "all",		"all",		POS_PRONOUN, .pronoun = { 0 }, },
	{ "everything",		"everything",	POS_PRONOUN, .pronoun = { 0 }, },

	{ 0, 0, 0, .verb = { 0 }, }, /* mark the end of the dictionary */
};

#define MAX_MEANINGS 10
struct nl_token {
	char *word;
	int pos[MAX_MEANINGS];
	int meaning[MAX_MEANINGS];
	int npos; /* number of possible parts of speech */
	struct number_data number;
	struct name_data name;
};

static char *fixup_punctuation(char *s)
{
	char *src, *dest;
	char *n = malloc(3 * strlen(s) + 1);

	dest = n;
	for (src = s; *src; src++) {
		char *x = index(",:;.!?", *src);
		if (!x) {
			*dest = *src;
			dest++;
			continue;
		}
		*dest = ' '; dest++;
		*dest = *src; dest++;
		*dest = ' '; dest++;
	}
	*dest = '\0';
	return n;
}

static struct nl_token **tokenize(char *txt, int *nwords)
{
	char *s = fixup_punctuation(txt);
	char *t;
	char *w[100];
	int i, count;
	struct nl_token **word = NULL;

	count = 0;
	do {
		t = strtok(s, " ");
		s = NULL;
		if (!t)
			break;
		w[count] = strdup(t);
		count++;
		if (count >= 100)
			break;
	} while (1);
	word = malloc(sizeof(*word) * count);
	for (i = 0; i < count; i++) {
		word[i] = malloc(sizeof(*word[0]));
		memset(word[i], 0, sizeof(*word[0]));
		word[i]->word = w[i];
		word[i]->npos = 0;
	}
	*nwords = count;
	if (s)
		free(s);
	return word;
}

static void free_tokens(struct nl_token *word[], int nwords)
{
	int i;

	for (i = 0; i < nwords; i++)
		free(word[i]);
}

static void classify_token(struct nl_token *t)
{
	int i, j;
	float x, rc;
	char c;

	if (t->npos >= MAX_MEANINGS)
		return;

	/* Find all the dictionary meanings */
	for (i = 0; dictionary[i].word != NULL; i++) {
		if (strcmp(dictionary[i].word, t->word) != 0)
			continue;
		t->pos[t->npos] = dictionary[i].p_o_s;
		t->meaning[t->npos] = i;
		t->npos++;
		if (t->npos >= MAX_MEANINGS)
			break;
	}
	if (t->npos >= MAX_MEANINGS)
		return;

	/* Find all synonym meanings */
	for (i = 0; synonym[i].syn != NULL; i++) {
		if (strcmp(synonym[i].syn, t->word) != 0)
			continue;
		for (j = 0; dictionary[j].word != NULL; j++) {
			if (strcmp(dictionary[j].word, synonym[i].canonical) != 0)
				continue;
			t->pos[t->npos] = dictionary[j].p_o_s;
			t->meaning[t->npos] = j;
			t->npos++;
			if (t->npos >= MAX_MEANINGS)
				break;
		}
	}

	/* See if it's a percent */
	rc = sscanf(t->word, "%f%c", &x, &c);
	if (rc == 2 && c == '%') {
		t->pos[t->npos] = POS_NUMBER;
		t->meaning[t->npos] = -1;
		t->number.value = x / 100.0;
		t->npos++;
		if (t->npos >= MAX_MEANINGS)
			return;
	} else  {
		/* See if it's a number */
		rc = sscanf(t->word, "%f", &x);
		if (rc == 1) {
			t->pos[t->npos] = POS_NUMBER;
			t->meaning[t->npos] = -1;
			t->number.value = x;
			t->npos++;
		}
		if (t->npos >= MAX_MEANINGS)
			return;
	}

	/* TODO: See if it's an external name */
}

static void classify_tokens(struct nl_token *t[], int ntokens)
{
	int i;

	for (i = 0; i < ntokens; i++)
		classify_token(t[i]);
}

static void print_token_instance(struct nl_token *t, int i)
{
	printf("%s[%d]:", t->word, i);
	switch (t->pos[i]) {
	case POS_NUMBER:
		printf(" (%s: %f)\n", part_of_speech[t->pos[i]], t->number.value);
		break;
	case -1:
		printf("(not parsed)\n");
		break;
	case POS_UNKNOWN:
	case POS_NAME:
		printf("(%s)\n", part_of_speech[t->pos[i]]);
		break;
	case POS_VERB:
		printf("(%s %s (%s))\n", dictionary[t->meaning[i]].verb.syntax,
			part_of_speech[t->pos[i]],
			dictionary[t->meaning[i]].canonical_word);
		break;
	default:
		printf("(%s (%s))\n", part_of_speech[t->pos[i]],
			dictionary[t->meaning[i]].canonical_word);
		break;
	}
}

static void print_token(struct nl_token *t)
{
	int i;

	if (t->npos == 0)
		printf("%s:(unknown)\n", t->word);
	for (i = 0; i < t->npos; i++)
		print_token_instance(t, i);
}

static void print_tokens(struct nl_token *t[], int ntokens)
{
	int i;
	for (i = 0; i < ntokens; i++)
		print_token(t[i]);
}

#define MAX_MEANINGS 10
#define MAX_WORDS 100

static char *starting_syntax = "v";

/*
 * This method of parsing is something I came up with in 1984 or 1985 while trying
 * to write a game similar to Infocom's Zork.
 *
 * The idea is we have a series of state machines parsing a command concurrently.
 * Each state machine has a "expected syntax".
 * As we come to a word (token) that has multiple interpretations, we replicate
 * the state machine once for each interpretation, and let each state machine
 * move forward.  At the end, hopefully only one state machine -- the one with
 * the correct interpretation -- is left alive.  (otherwise there is an ambiguity)
 */
struct nl_parse_machine {
	struct nl_parse_machine *next, *prev;
	char *syntax;				/* expected syntax for this machine, see struct verb_data, above
						 * This starts out as "v" (verb), and when a verb is encounted, it
						 * changes to the syntax for that verb, and the syntax_pos is reset to 0
						 */
	int syntax_pos;				/* This state machine's current position in the syntax */
	int current_token;			/* This machine's current position in the token[] array */
	int meaning[MAX_WORDS];			/* This machine's chosen interpretation of meaning of token[x].
						 * meaning[x] stores an index into token[x].meaning[] and token[x].pos[]
						 */
	int state;				/* State of this state machine: Start-> Running-> other-states */
#define NL_STATE_START 0
#define NL_STATE_RUNNING 1
#define NL_STATE_FAILED 2
#define NL_STATE_SUCCESS 3
#define NL_STATE_DONE 4
	float score;				/* Measure of how well this SUCCESSFUL parsing matched the input.
						 * Not _every_ word has to match to be considered successful,
						 * The score is essentially (count of tokens matched / total tokens)
						 */
};

static void nl_parse_machine_init(struct nl_parse_machine *p,
		char *syntax, int syntax_pos, int current_token, int *meaning)
{
	p->next = NULL;
	p->prev = NULL;
	p->syntax = syntax;
	p->syntax_pos = syntax_pos;
	p->current_token = current_token;
	memset(p->meaning, -1, sizeof(p->meaning));
	if (meaning && current_token > 0) {
		memcpy(p->meaning, meaning, sizeof(*meaning) * current_token);
		/* printf("----- init, last meaning: %d\n", p->meaning[current_token - 1]); */
	}
	p->state = NL_STATE_RUNNING;
	p->score = 0.0;
}

static void insert_parse_machine_before(struct nl_parse_machine **list, struct nl_parse_machine *entry)
{
	if (*list == NULL) {
		*list = entry;
		(*list)->prev = NULL;
		(*list)->next = NULL;
		return;
	}
	entry->prev = (*list)->prev;
	entry->next = *list;
	if (entry->next)
		entry->next->prev = entry;
	if (entry->prev)
		entry->prev->next = entry;
	*list = entry;
}

static void nl_parse_machine_list_remove(struct nl_parse_machine **list, struct nl_parse_machine *p)
{
	if (*list == NULL)
		return;

	if (p == *list) {
		*list = (*list)->next;
		if (*list)
			(*list)->prev = NULL;
		free(p);
		return;
	}
	if (p->prev)
		p->prev->next = p->next;
	if (p->next)
		p->next->prev = p->prev;
	free(p);
	return;
}

static void nl_parse_machine_list_prune(struct nl_parse_machine **list)
{
	struct nl_parse_machine *next, *p = *list;

	while (p) {
		switch (p->state) {
		case NL_STATE_RUNNING:
		case NL_STATE_SUCCESS:
			p = p->next;
			break;
		default:
			next = p->next;
			nl_parse_machine_list_remove(list, p);
			p = next;
			break;
		}
	}
}

static struct nl_parse_machine *nl_parse_machines_find_highest_score(struct nl_parse_machine **list)
{
	float highest_score;
	struct nl_parse_machine *highest, *p = *list;
	highest = NULL;

	while (p) {
		if (!highest || highest_score < p->score) {
			highest = p;
			highest_score = p->score;
		}
		p = p->next;
	}
	return highest;
}

static void parse_machine_free_list(struct nl_parse_machine *list)
{
	struct nl_parse_machine *next;

	while (list) {
		next = list->next;
		free(list);
		list = next;
	}
}

static void nl_parse_machine_process_token(struct nl_parse_machine **list, struct nl_parse_machine *p,
				struct nl_token **token, int ntokens, int looking_for_pos)
{
	struct nl_parse_machine *new_parse_machine;
	int i, found = 0;
	int p_o_s;

	for (i = 0; i < token[p->current_token]->npos; i++) {
		p_o_s = token[p->current_token]->pos[i]; /* part of speech */
		if (p_o_s == looking_for_pos) {
			p->meaning[p->current_token] = i;
			if (found == 0) {
				p->syntax_pos++;
				p->current_token++;
			} else {
				new_parse_machine = malloc(sizeof(*new_parse_machine));
				nl_parse_machine_init(new_parse_machine, p->syntax,
						p->syntax_pos + 1, p->current_token + 1, p->meaning);
				insert_parse_machine_before(list, new_parse_machine);
			}
			found++;
			break;
		} else {
			if (looking_for_pos == POS_NOUN && (p_o_s == POS_ARTICLE || p_o_s == POS_ADJECTIVE)) {
				/* Don't advance syntax_pos, but advance to next token */
				p->meaning[p->current_token] = i;
				if (found == 0) {
					p->current_token++;
				} else {
					new_parse_machine = malloc(sizeof(*new_parse_machine));
					nl_parse_machine_init(new_parse_machine, p->syntax,
							p->syntax_pos, p->current_token + 1, p->meaning);
					insert_parse_machine_before(list, new_parse_machine);
				}
				found++;
			}
		}
	}
	if (!found) {
		/* didn't find a required meaning for this token, we're done */
		p->state = NL_STATE_FAILED;
	}
}

static void nl_parse_machine_step(struct nl_parse_machine **list,
			struct nl_parse_machine *p, struct nl_token **token, int ntokens)
{
	int i, de, p_o_s;
	int found;
	struct nl_parse_machine *new_parse_machine;

	if (p->state != NL_STATE_RUNNING)
		return;

	/* We ran out of syntax */
	if (p->syntax_pos >= strlen(p->syntax)) {
		if (p->current_token >= ntokens) {
			p->state = NL_STATE_SUCCESS;
			return;
		} else {
			p->syntax = starting_syntax;
			p->syntax_pos = 0;
		}
	}

	/* We ran out of tokens */
	if (p->current_token >= ntokens) {
		/* printf("Ran out tokens, but only %d/%d through syntax '%s'.\n",
				p->syntax_pos, (int) strlen(p->syntax), p->syntax); */
		p->state = NL_STATE_DONE;
		return;
	}

#if 0
	printf("Parsing: token = %d(%s)syntax = '%c'[%s] pos(", p->current_token,
		token[p->current_token]->word, p->syntax[p->syntax_pos], p->syntax);
	for (i = 0; i < token[p->current_token]->npos; i++) {
		p_o_s = token[p->current_token]->pos[i]; /* part of speech */
		printf("%s", part_of_speech[p_o_s]);
		if (p_o_s == POS_VERB)
			printf("[%s]", dictionary[token[p->current_token]->meaning[i]].verb.syntax);
		printf(" ");
	}
	printf(")\n");
#endif
	switch (p->syntax[p->syntax_pos]) {
	case 'v': /* starting state, just looking for verbs... */
		found = 0;
		for (i = 0; i < token[p->current_token]->npos; i++) {
			p_o_s = token[p->current_token]->pos[i]; /* part of speech */
			de = token[p->current_token]->meaning[i]; /* dictionary entry */
			switch (p_o_s) {
			case POS_VERB:
				new_parse_machine = malloc(sizeof(*new_parse_machine));
				p->meaning[p->current_token] = i;
				nl_parse_machine_init(new_parse_machine, dictionary[de].verb.syntax,
							0, p->current_token + 1, p->meaning);
				insert_parse_machine_before(list, new_parse_machine);
				found++;
				break;
			default: /* not a verb, ignore */
				break;
			}
		}
		if (found) {
			p->state = NL_STATE_DONE;
		} else {
			/* didn't find a verb meaning for this token, move to next token */
			p->current_token++;
			if (p->current_token >= ntokens)
				p->state = NL_STATE_DONE;
		}
		break;
	case 'p':
		nl_parse_machine_process_token(list, p, token, ntokens, POS_PREPOSITION);
		break;
	case 'n':
		nl_parse_machine_process_token(list, p, token, ntokens, POS_NOUN);
		break;
	case 'q':
		nl_parse_machine_process_token(list, p, token, ntokens, POS_NUMBER);
		break;
	case 'a':
		nl_parse_machine_process_token(list, p, token, ntokens, POS_ADJECTIVE);
		break;
	case 's':
		nl_parse_machine_process_token(list, p, token, ntokens, POS_SEPARATOR);
		break;
	case '\0': /* We reached the end of the syntax, success */
		if (p->current_token >= ntokens) {
			p->state = NL_STATE_SUCCESS;
		} else {
			p->syntax = starting_syntax;
			p->syntax_pos = 0;
		}
		break;
	default:
		fprintf(stderr, "Unexpected syntax entry '%c'\n", p->syntax[p->syntax_pos]);
		break;
	}
}

static int nl_parse_machines_still_running(struct nl_parse_machine **list)
{
	struct nl_parse_machine *i;

	for (i = *list; i != NULL; i = i->next)
		if (i->state == NL_STATE_RUNNING)
			return 1;
	return 0;
}

static void nl_parse_machine_print(struct nl_parse_machine *p, struct nl_token **token, int ntokens, int number)
{
	int i;

	printf("State machine %d ('%s,%d, ', ", number, p->syntax, p->syntax_pos);
	switch (p->state) {
	case NL_STATE_START:
		printf("START)");
		break;
	case NL_STATE_RUNNING:
		printf("RUNNING)");
		break;
	case NL_STATE_FAILED:
		printf("FAILED)");
		break;
	case NL_STATE_SUCCESS:
		printf("SUCCESS)");
		break;
	case NL_STATE_DONE:
		printf("DONE)");
		break;
	default:
		printf("UNKNOWN (%d))", p->state);
		break;
	}
	printf(" -- score = %f\n", p->score);
	for (i = 0; i < ntokens; i++) {
		print_token_instance(token[i], p->meaning[i]);
	}
}

static void nl_parse_machines_print(struct nl_parse_machine **list, struct nl_token **token, int ntokens)
{
	int x;
	struct nl_parse_machine *i;

	x = 0;
	for (i = *list; i != NULL; i = i->next)
		nl_parse_machine_print(i, token, ntokens, x++);
}

static void nl_parse_machines_run(struct nl_parse_machine **list, struct nl_token **tokens, int ntokens)
{
	int iteration = 0;
	struct nl_parse_machine *i;

	do {
		/* printf("--- iteration %d ---\n", iteration);
		nl_parse_machines_print(list, tokens, ntokens); */
		for (i = *list; i != NULL; i = i->next)
			nl_parse_machine_step(list, i, tokens, ntokens);
		iteration++;
		nl_parse_machine_list_prune(list);
	} while (nl_parse_machines_still_running(list));
	/* printf("--- iteration %d ---\n", iteration);
	nl_parse_machines_print(list, tokens, ntokens); */
}

static void nl_parse_machine_score(struct nl_parse_machine *p, int ntokens)
{
	int i;

	float score = 0.0;

	if (p->state != NL_STATE_SUCCESS || ntokens == 0) {
		p->score = 0.0;
		return;
	}

	for (i = 0; i < p->current_token; i++) {
		if (p->meaning[i] != -1)
			score += 1.0;
	}
	p->score = score / (float) ntokens;
}

static void nl_parse_machines_score(struct nl_parse_machine **list, int ntokens)
{
	struct nl_parse_machine *p = *list;

	for (p = *list; p; p = p->next)
		nl_parse_machine_score(p, ntokens);
}

static void extract_meaning(char *original_text, struct nl_token *token[], int ntokens)
{
	struct nl_parse_machine *list, *p;

	list = NULL;
	p = malloc(sizeof(*p));
	nl_parse_machine_init(p, starting_syntax, 0, 0, NULL);
	insert_parse_machine_before(&list, p);
	nl_parse_machines_run(&list, token, ntokens);
	nl_parse_machine_list_prune(&list);
	nl_parse_machines_score(&list, ntokens);
	p = nl_parse_machines_find_highest_score(&list);
	if (p) {
		printf("-------- Final interpretation: ----------\n");
		nl_parse_machine_print(p, token, ntokens, 0);
	} else {
		printf("Failure to comprehend '%s'\n", original_text);
	}
}

void snis_nl_parse_natural_language_request(char *txt)
{
	int ntokens;
	struct nl_token **token = NULL;
	char *original = strdup(txt);

	lowercase(txt);
	token = tokenize(txt, &ntokens);
	classify_tokens(token, ntokens);
	// print_tokens(token, ntokens);
	extract_meaning(original, token, ntokens);
	free_tokens(token, ntokens);
	if (token)
		free(token);
	free(original);
}

void snis_nl_add_synonym(char *synonym_word, char *canonical_word)
{
	if (nsynonyms >= MAX_SYNONYMS) {
		fprintf(stderr, "Too many synonyms, discarding '%s/%s'\n", synonym_word, canonical_word);
		return;
	}
	synonym[nsynonyms].syn = strdup(synonym_word);
	synonym[nsynonyms].canonical = strdup(canonical_word);
	nsynonyms++;
}

#ifdef TEST_NL
static void init_synonyms(void)
{
	snis_nl_add_synonym("cut", "lower");
	snis_nl_add_synonym("decrease", "lower");
	snis_nl_add_synonym("boost", "raise");
	snis_nl_add_synonym("increase", "raise");
	snis_nl_add_synonym("calculate", "compute");
	snis_nl_add_synonym("figure", "compute");
	snis_nl_add_synonym("activate", "engage");
	snis_nl_add_synonym("actuate", "engage");
	snis_nl_add_synonym("start", "engage");
	snis_nl_add_synonym("energize", "engage");
	snis_nl_add_synonym("deactivate", "disengage");
	snis_nl_add_synonym("deenergize", "disengage");
	snis_nl_add_synonym("stop", "disengage");
	snis_nl_add_synonym("shutdown", "disengage");
	snis_nl_add_synonym("deploy", "launch");
}

int main(int argc, char *argv[])
{
	int i;

	init_synonyms();

	for (i = 1; i < argc; i++)
		snis_nl_parse_natural_language_request(argv[i]);
	return 0;
}

#endif

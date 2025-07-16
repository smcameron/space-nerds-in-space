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
#include "spelled_numbers.h"

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
	"externalnoun",
	"auxverb",
	"expletive",
};

#define MAX_SYNONYMS 100
struct synonym_entry {
	char *syn, *canonical;
};

#define MAX_DICTIONARY_ENTRIES 1000
struct dictionary_entry {
	char *word;
	char *canonical_word;
	int p_o_s; /* part of speech */
	__extension__ union {
		struct snis_nl_noun_data noun;
		struct snis_nl_verb_data verb;
		struct snis_nl_article_data article;
		struct snis_nl_preposition_data preposition;
		struct snis_nl_separator_data separator;
		struct snis_nl_adjective_data adjective;
		struct snis_nl_adverb_data adverb;
		struct snis_nl_number_data number;
		struct snis_nl_pronoun_data pronoun;
		struct snis_nl_external_noun_data external_noun;
	};
};

#define MAX_MEANINGS 11
struct nl_token {
	char *word;
	int pos[MAX_MEANINGS];
	int meaning[MAX_MEANINGS];
	int npos; /* number of possible parts of speech */
	struct snis_nl_number_data number;
	struct snis_nl_external_noun_data external_noun;
};

struct snis_nl_topic {
	int part_of_speech;
	int meaning;
	char *word;
	union snis_nl_extra_data extra_data;
};

struct snis_nl_context {
	int debuglevel;
	int ndictionary_entries;
	struct dictionary_entry dictionary[MAX_DICTIONARY_ENTRIES + 1];
	int nsynonyms;
	struct synonym_entry synonym[MAX_SYNONYMS + 1];
	struct snis_nl_topic current_topic;
	void *user_context;
	snis_nl_external_noun_lookup external_lookup;
	snis_nl_error_function error_function;
	snis_nl_multiword_preprocessor_fn multiword_preprocessor_fn;
};

struct snis_nl_context *snis_nl_context_create(void)
{
	struct snis_nl_context *ctx = malloc(sizeof(*ctx));
	memset(ctx, 0, sizeof(*ctx));
	ctx->current_topic.part_of_speech = -1;
	return ctx;
}

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
		/* If it is a dot, check if the preceding and following chars are digits */
		if (*src == '.') {
			if (src != s && isdigit(*(src + 1)) && isdigit(*(src - 1))) {
				/* preserve decimal point */
				*dest = *src;
				dest++;
				continue;
			}
		}
		*dest = ' '; dest++;
		*dest = *src; dest++;
		*dest = ' '; dest++;
	}
	*dest = '\0';
	return n;
}

/* Replace underscores with spaces */
static inline void multiword_processor_decode(char *w)
{
	char *x = w;
	for (x = w; *x; x++)
		if (*x == '_')
			*x = ' ';
}

static inline void encode_spaces(char *instance, int len)
{
	int i;

	for (i = 0; i < len; i++)
		if (instance[i] == ' ')
			instance[i] = '_';
}

/* For certain multiword phrases, replace spaces with underscores to make them
 * tokenize as a unit.  This gets undone immediately after tokenization in
 * tokenize().
 */
static void multiword_processor_encode(struct snis_nl_context *ctx, char *text)
{
	char *hit;
	int i;

	/* encode multiword synonyms */
	for (i = 0; ctx->synonym[i].syn != NULL; i++) {
		hit = strstr(text, ctx->synonym[i].syn);
		if (hit)
			encode_spaces(hit, strlen(ctx->synonym[i].syn));
	}

	/* encode multiword dictionary words */
	for (i = 0; ctx->dictionary[i].word != NULL; i++) {
		hit = strstr(text, ctx->dictionary[i].word);
		if (hit)
			encode_spaces(hit, strlen(ctx->dictionary[i].word));
	}

	/* encode specialized words */
	if (ctx->multiword_preprocessor_fn)
		ctx->multiword_preprocessor_fn(ctx, text, SNIS_NL_ENCODE);
}

static struct nl_token **tokenize(struct snis_nl_context *ctx, char *txt, int *nwords)
{
	char *s = fixup_punctuation(txt);
	char *t;
	char *w[100];
	int i, count;
	struct nl_token **word = NULL;
	char *saveptr = NULL;
	char *tmpstrptr = s;

	count = 0;
	do {
		t = strtok_r(tmpstrptr, " ", &saveptr);
		tmpstrptr = NULL;
		if (!t)
			break;
		w[count] = strdup(t);
		multiword_processor_decode(w[count]);
		if (ctx->multiword_preprocessor_fn)
			ctx->multiword_preprocessor_fn(ctx, txt, SNIS_NL_DECODE);
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

static void maybe_do_pronoun_substitution(struct snis_nl_context *ctx, struct nl_token *t)
{
	if (t->pos[t->npos] != POS_PRONOUN)
		return;
	if (strcasecmp(t->word, "it") != 0) /* only try to substitute for "it" */
		return;
	if (ctx->current_topic.part_of_speech == -1) /* No antecedent ready */
		return;
	/* Substitute the antecedent */
	t->pos[t->npos] = ctx->current_topic.part_of_speech;
	t->meaning[t->npos] = ctx->current_topic.meaning;
	if (ctx->current_topic.part_of_speech == POS_EXTERNAL_NOUN)
		t->external_noun.handle = ctx->current_topic.extra_data.external_noun.handle;
	else
		t->external_noun.handle = -1;
}

static void classify_token(struct snis_nl_context *ctx, struct nl_token *t)
{
	int i, j;
	float x, rc;
	char c;
	uint32_t handle;

	if (t->npos >= MAX_MEANINGS)
		return;

	/* Find all the dictionary meanings */
	for (i = 0; ctx->dictionary[i].word != NULL; i++) {
		if (strcmp(ctx->dictionary[i].word, t->word) != 0)
			continue;
		t->pos[t->npos] = ctx->dictionary[i].p_o_s;
		t->meaning[t->npos] = i;
		maybe_do_pronoun_substitution(ctx, t);
		t->npos++;
		if (t->npos >= MAX_MEANINGS)
			break;
	}
	if (t->npos >= MAX_MEANINGS)
		return;

	/* Find all synonym meanings */
	for (i = 0; ctx->synonym[i].syn != NULL; i++) {
		if (strcmp(ctx->synonym[i].syn, t->word) != 0)
			continue;
		for (j = 0; ctx->dictionary[j].word != NULL; j++) {
			if (strcmp(ctx->dictionary[j].word, ctx->synonym[i].canonical) != 0)
				continue;
			t->pos[t->npos] = ctx->dictionary[j].p_o_s;
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

	if (ctx->external_lookup) {
		handle = ctx->external_lookup(ctx, t->word);
		if (handle != 0xffffffff) {
			t->pos[t->npos] = POS_EXTERNAL_NOUN;
			t->meaning[t->npos] = -1;
			t->external_noun.handle = handle;
			t->npos++;
		}
		if (t->npos >= MAX_MEANINGS)
			return;
	}
}

static void classify_tokens(struct snis_nl_context *ctx, struct nl_token *t[], int ntokens)
{
	int i;

	for (i = 0; i < ntokens; i++)
		classify_token(ctx, t[i]);
}

static void remove_expletives(struct nl_token *t[], int *ntokens)
{
	int i, j;

	i = 0;
	while (i < *ntokens) {
		if (t[i]->npos == 1 && t[i]->pos[0] == POS_EXPLETIVE) {
			for (j = i; j < *ntokens - 1; j++)
				t[j] = t[j + 1];
			(*ntokens)--;
			continue;
		}
		i++;
	}
}

static void print_token_instance(struct snis_nl_context *ctx, struct nl_token *t, int i)
{
	printf("%s[%d]:", t->word, i);
	if (i < 0)
		return;
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
		printf("(%s %s (%s))\n", ctx->dictionary[t->meaning[i]].verb.syntax,
			part_of_speech[t->pos[i]],
			ctx->dictionary[t->meaning[i]].canonical_word);
		break;
	default:
		printf("(%s (%s))\n", part_of_speech[t->pos[i]],
			ctx->dictionary[t->meaning[i]].canonical_word);
		break;
	}
}

static void print_token(struct snis_nl_context *ctx, struct nl_token *t)
{
	int i;

	if (t->npos == 0)
		printf("%s:(unknown)\n", t->word);
	for (i = 0; i < t->npos; i++)
		print_token_instance(ctx, t, i);
}

static void __attribute__((unused)) print_tokens(struct snis_nl_context *ctx,
			struct nl_token *t[], int ntokens)
{
	int i;
	for (i = 0; i < ntokens; i++)
		print_token(ctx, t[i]);
}

#define MAX_MEANINGS 11
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
	char *syntax;				/* expected syntax for this machine, see struct snis_nl_verb_data, above
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
		/* clang scan-build seems to think "*list" can be a use-after-free bug,
		 * but I think it just does not understand the link rejiggering that
		 * happens after this "if" statement. -- smcameron 2019-07-11
		 */
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
	int syntax_length = 0;

	while (p) {
		if (!highest || highest_score < p->score) {
			highest = p;
			highest_score = p->score;
		}
		p = p->next;
	}

	if (!highest)
		return highest;

	/* Break tie scores by length of syntax, so that e.g. "an" beats "n" */
	p = *list;
	while (p) {
		if (p->score == highest_score) {
			int len = strlen(p->syntax);
			if (len > syntax_length) {
				syntax_length = len;
				highest = p;
			}
			p = p->next;
		}
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

static void nl_parse_machine_process_token(struct snis_nl_context *ctx,
				struct nl_parse_machine **list, struct nl_parse_machine *p,
				struct nl_token **token, int ntokens, int looking_for_pos)
{
	struct nl_parse_machine *new_parse_machine;
	int i, found = 0;
	int p_o_s;
	int topic_pos = ctx->current_topic.part_of_speech;

	for (i = 0; i < token[p->current_token]->npos; i++) {
		p_o_s = token[p->current_token]->pos[i]; /* part of speech */
		if (p_o_s == looking_for_pos ||
			(p_o_s == POS_EXTERNAL_NOUN && looking_for_pos == POS_NOUN) ||
			(p_o_s == POS_PRONOUN && looking_for_pos == POS_NOUN &&
				(topic_pos == POS_NOUN || topic_pos == POS_EXTERNAL_NOUN))) {
			p->meaning[p->current_token] = i;
			if (found == 0) {
				p->syntax_pos++;
				p->current_token++;
				if (p->current_token >= ntokens && p->syntax[p->syntax_pos] != '\0') {
					p->state = NL_STATE_FAILED;
					break;
				}
			} else {
				new_parse_machine = malloc(sizeof(*new_parse_machine));
				nl_parse_machine_init(new_parse_machine, p->syntax,
						p->syntax_pos + 1, p->current_token + 1, p->meaning);
				insert_parse_machine_before(list, new_parse_machine);
			}
			found++;
			break;
		} else if (looking_for_pos == POS_NOUN && (p_o_s == POS_ARTICLE || p_o_s == POS_ADJECTIVE)) {
			/* Don't advance syntax_pos, but advance to next token */
			p->meaning[p->current_token] = i;
			if (found == 0) {
				p->current_token++;
				if (p->current_token >= ntokens) {
					p->state = NL_STATE_FAILED;
					break;
				}
			} else {
				new_parse_machine = malloc(sizeof(*new_parse_machine));
				nl_parse_machine_init(new_parse_machine, p->syntax,
						p->syntax_pos, p->current_token + 1, p->meaning);
				insert_parse_machine_before(list, new_parse_machine);
			}
			found++;
		} else if (looking_for_pos == POS_ADJECTIVE && p_o_s == POS_ARTICLE) {
				/* Don't advance syntax_pos, but advance to next token */
				p->meaning[p->current_token] = i;
				if (found == 0) {
					p->current_token++;
					if (p->current_token >= ntokens) {
						p->state = NL_STATE_FAILED;
						break;
					}
				} else {
					new_parse_machine = malloc(sizeof(*new_parse_machine));
					nl_parse_machine_init(new_parse_machine, p->syntax,
							p->syntax_pos, p->current_token + 1, p->meaning);
					insert_parse_machine_before(list, new_parse_machine);
				}
				found++;
		}
	}
	if (!found) {
		/* didn't find a required meaning for this token, we're done */
		if (ctx->debuglevel > 0) {
			if (p->current_token >= 0 && p->current_token < ntokens)
				printf("   Failed to parse '%s'\n", token[p->current_token]->word);
			else
				printf("   Ran out of tokens\n");
			printf("   Looking for %s\n", part_of_speech[looking_for_pos]);
		}
		p->state = NL_STATE_FAILED;
	}
}

static void nl_parse_machine_step(struct snis_nl_context *ctx, struct nl_parse_machine **list,
			struct nl_parse_machine *p, struct nl_token **token, int ntokens)
{
	int i, de, p_o_s;
	int found;
	struct nl_parse_machine *new_parse_machine;

	if (p->state != NL_STATE_RUNNING)
		return;

	/* We ran out of syntax */
	if ((size_t) p->syntax_pos >= strlen(p->syntax)) {
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
				nl_parse_machine_init(new_parse_machine, ctx->dictionary[de].verb.syntax,
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
		nl_parse_machine_process_token(ctx, list, p, token, ntokens, POS_PREPOSITION);
		break;
	case 'n':
		nl_parse_machine_process_token(ctx, list, p, token, ntokens, POS_NOUN);
		break;
	case 'q':
		nl_parse_machine_process_token(ctx, list, p, token, ntokens, POS_NUMBER);
		break;
	case 'a':
		nl_parse_machine_process_token(ctx, list, p, token, ntokens, POS_ADJECTIVE);
		break;
	case 'P':
		nl_parse_machine_process_token(ctx, list, p, token, ntokens, POS_PRONOUN);
		break;
	case 'x':
		nl_parse_machine_process_token(ctx, list, p, token, ntokens, POS_AUXVERB);
		break;
	case 's':
		nl_parse_machine_process_token(ctx, list, p, token, ntokens, POS_SEPARATOR);
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

void snis_nl_clear_current_topic(struct snis_nl_context *ctx)
{
	ctx->current_topic.part_of_speech = -1;
	ctx->current_topic.meaning = -1;
	if (ctx->current_topic.word) {
		free(ctx->current_topic.word);
		ctx->current_topic.word = NULL;
	}
	memset(&ctx->current_topic.extra_data, 0, sizeof(ctx->current_topic.extra_data));
	ctx->current_topic.extra_data.external_noun.handle = -1;
}

void snis_nl_set_current_topic(struct snis_nl_context *ctx,
	int p_o_s, char *word, union snis_nl_extra_data extra_data)
{
	int i;

	snis_nl_clear_current_topic(ctx);
	if (p_o_s != POS_EXTERNAL_NOUN) {
		for (i = 0; ctx->dictionary[i].word != NULL; i++) {
			if (strcmp(ctx->dictionary[i].word, word) != 0)
				continue;
			if (p_o_s != ctx->dictionary[i].p_o_s)
				continue;
			break;
		}
		if (!ctx->dictionary[i].word) {
			snis_nl_clear_current_topic(ctx);
			return;
		}
	}
	/* TODO make synonyms work here? */
	ctx->current_topic.part_of_speech = p_o_s;
	ctx->current_topic.word = strdup(word);
	if (p_o_s == POS_EXTERNAL_NOUN)
		ctx->current_topic.meaning = -1;
	else
		ctx->current_topic.meaning = i;
	ctx->current_topic.extra_data = extra_data;
}

static void do_action(struct snis_nl_context *ctx,
	struct nl_parse_machine *p, struct nl_token **token, int ntokens)
{
	int argc;
	char *argv[MAX_WORDS];
	int pos[MAX_WORDS];
	union snis_nl_extra_data extra_data[MAX_WORDS] = { { { 0 } } };
	union snis_nl_extra_data antecedent_extra_data = { { 0 } };
	int antecedent_word = -1;
	int antecedent_noun_meaning = -1;
	int antecedent_pos = -1;
	int i, w = 0, de;
	snis_nl_verb_function vf = NULL;
	int limit = ntokens;

	if (p->current_token < ntokens)
		limit = p->current_token;

	argc = 0;
	for (i = 0; i < limit; i++) {
		struct nl_token *t = token[i];
		if (t->pos[p->meaning[i]] == POS_VERB) {
			if (vf != NULL) {
				vf(ctx, argc, argv, pos, extra_data);
				vf = NULL;
			}
			argc = 1;
			w = 0;
			de = t->meaning[p->meaning[i]];
			argv[w] = ctx->dictionary[de].canonical_word;
			pos[w] = POS_VERB;
			extra_data[w].number.value = 0.0;
			vf = ctx->dictionary[de].verb.fn;
			w++;
		} else {
			if (argc > 0) {
				de = t->meaning[p->meaning[i]];
				pos[w] = t->pos[p->meaning[i]];
				if (pos[w] != POS_NUMBER && pos[w] != POS_EXTERNAL_NOUN)
					argv[w] = ctx->dictionary[de].canonical_word;
				else
					argv[w] = t->word;
				if (pos[w] == POS_NUMBER) {
					extra_data[w].number.value = t->number.value;
				} else if (pos[w] == POS_EXTERNAL_NOUN) {
					extra_data[w].external_noun.handle = t->external_noun.handle;
					antecedent_extra_data = extra_data[w];
					antecedent_noun_meaning = -1;
					antecedent_pos = POS_EXTERNAL_NOUN;
					antecedent_word = w;
				} else if (pos[w] == POS_NOUN) {
					memset(&antecedent_extra_data, 0, sizeof(antecedent_extra_data));
					antecedent_noun_meaning = de;
					antecedent_word = w;
					antecedent_pos = POS_NOUN;
				} else {
					extra_data[w].number.value = 0.0;
				}
				w++;
				argc++;
			}
		}
	}

	if (vf != NULL) {
		if (antecedent_word != -1) {
			snis_nl_set_current_topic(ctx, antecedent_pos, argv[antecedent_word], antecedent_extra_data);
			ctx->current_topic.meaning = antecedent_noun_meaning;
		}
		vf(ctx, argc, argv, pos, extra_data);
		vf = NULL;
	}
}

static void nl_parse_machine_print(struct snis_nl_context *ctx,
		struct nl_parse_machine *p, struct nl_token **token, int ntokens, int number)
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
		print_token_instance(ctx, token[i], p->meaning[i]);
	}
}

static void nl_parse_machines_print(struct snis_nl_context *ctx,
		struct nl_parse_machine **list, struct nl_token **token, int ntokens)
{
	int x;
	struct nl_parse_machine *i;

	x = 0;
	for (i = *list; i != NULL; i = i->next)
		nl_parse_machine_print(ctx, i, token, ntokens, x++);
}

static void nl_parse_machines_run(struct snis_nl_context *ctx,
		struct nl_parse_machine **list, struct nl_token **tokens, int ntokens)
{
	int iteration = 0;
	struct nl_parse_machine *i;

	do {
		if (ctx->debuglevel > 0) {
			printf("\n--- iteration %d ---\n", iteration);
			nl_parse_machines_print(ctx, list, tokens, ntokens);
		}
		for (i = *list; i != NULL; i = i->next)
			nl_parse_machine_step(ctx, list, i, tokens, ntokens);
		iteration++;
		nl_parse_machine_list_prune(list);
	} while (nl_parse_machines_still_running(list));
	if (ctx->debuglevel > 0) {
		printf("--- iteration %d ---\n", iteration);
		nl_parse_machines_print(ctx, list, tokens, ntokens);
	}
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
	struct nl_parse_machine *p;

	for (p = *list; p; p = p->next)
		nl_parse_machine_score(p, ntokens);
}

static int extract_meaning(struct snis_nl_context *ctx,
	char *original_text, struct nl_token *token[], int ntokens, int test_only)
{
	struct nl_parse_machine *list, *p;
	int rc;

	list = NULL;
	p = malloc(sizeof(*p));
	nl_parse_machine_init(p, starting_syntax, 0, 0, NULL);
	insert_parse_machine_before(&list, p);
	nl_parse_machines_run(ctx, &list, token, ntokens);
	nl_parse_machine_list_prune(&list);
	nl_parse_machines_score(&list, ntokens);
	p = nl_parse_machines_find_highest_score(&list);
	if (p) {
		if (ctx->debuglevel > 0) {
			printf("-------- Final interpretation: ----------\n");
			nl_parse_machine_print(ctx, p, token, ntokens, 0);
		}
		rc = 0;
		if (!test_only)
			do_action(ctx, p, token, ntokens);
	} else {
		if (ctx->debuglevel > 0)
			printf("Failure to comprehend '%s'\n", original_text);
		if (ctx->error_function && !test_only)
			ctx->error_function(ctx);
		rc = -1;
	}
	parse_machine_free_list(p);
	return rc;
}

static int nl_parse_natural_language_request(struct snis_nl_context *ctx,
			char *original, int test_only)
{
	int ntokens;
	struct nl_token **token = NULL;
	char *copy = strdup(original);
	int rc;

	lowercase(copy);
	handle_spelled_numbers_in_place(copy);
	multiword_processor_encode(ctx, copy);
	token = tokenize(ctx, copy, &ntokens);
	classify_tokens(ctx, token, ntokens);
	remove_expletives(token, &ntokens);
	/* print_tokens(ctx, token, ntokens); */
	rc = extract_meaning(ctx, original, token, ntokens, test_only);
	free_tokens(token, ntokens);
	if (token)
		free(token);
	free(copy);
	return rc;
}

void snis_nl_parse_natural_language_request(struct snis_nl_context *ctx, char *txt)
{
	(void) nl_parse_natural_language_request(ctx, txt, 0);
}

int snis_nl_test_parse_natural_language_request(struct snis_nl_context *ctx, char *txt)
{
	if (ctx->debuglevel > 0)
		printf("Testing: %s\n", txt);
	return nl_parse_natural_language_request(ctx, txt, 1);
}

void snis_nl_add_synonym(struct snis_nl_context *ctx, char *synonym_word, char *canonical_word)
{
	if (ctx->nsynonyms >= MAX_SYNONYMS) {
		fprintf(stderr, "Too many synonyms, discarding '%s/%s'\n", synonym_word, canonical_word);
		return;
	}
	ctx->synonym[ctx->nsynonyms].syn = strdup(synonym_word);
	ctx->synonym[ctx->nsynonyms].canonical = strdup(canonical_word);
	ctx->nsynonyms++;
}

static void snis_nl_debuglevel(struct snis_nl_context *ctx,
				__attribute__((unused)) int argc,
				__attribute__((unused)) char *argv[],
				__attribute__((unused)) int pos[],
				__attribute__((unused)) union snis_nl_extra_data extra_data[])
{
	if (pos[1] != POS_NUMBER) {
		ctx->debuglevel = !!ctx->debuglevel;
		fprintf(stderr, "Toggling debug level to %d, '%s'\n", ctx->debuglevel, ctx->debuglevel ? "on" : "off");
		return;
	}
	ctx->debuglevel = (int) extra_data[1].number.value;
	fprintf(stderr, "Set debug level to %d, '%s'\n", ctx->debuglevel, ctx->debuglevel ? "on" : "off");
}

static void snis_nl_dumpvocab(struct snis_nl_context *ctx,
				__attribute__((unused)) int argc,
				__attribute__((unused)) char *argv[], __attribute__((unused)) int pos[],
				__attribute__((unused)) union snis_nl_extra_data extra_data[])
{
	int i;

	if (!ctx->debuglevel)
		return;
	/* Dump out all the words we know to stdout */

	for (i = 0; i < ctx->nsynonyms; i++)
		printf("%s\n", ctx->synonym[i].syn);
	for (i = 0; i < ctx->ndictionary_entries; i++)
		printf("%s\n", ctx->dictionary[i].word);
}

static void generic_add_dictionary_word(struct snis_nl_context *ctx,
						char *word, char *canonical_word, int p_o_s,
						char *syntax, snis_nl_verb_function action)
{
	struct dictionary_entry *de;

	/* If the dictionary is empty, first, jam in an entry to allow turning on debugging */
	if (ctx->ndictionary_entries == 0 && strcmp(word, "nldebuglevel") != 0) {
		/* TODO: doesn't this cause an infinite recursion? */
		generic_add_dictionary_word(ctx, "nldebuglevel", "nldebuglevel", POS_VERB, "q", snis_nl_debuglevel);
		generic_add_dictionary_word(ctx, "nldumpvocab", "nldumpvocab", POS_VERB, "", snis_nl_dumpvocab);
	}
	if (ctx->ndictionary_entries >= MAX_DICTIONARY_ENTRIES) {
		fprintf(stderr, "Too many dictionary entries, discarding '%s/%s'\n", word, canonical_word);
		return;
	}

	de = &ctx->dictionary[ctx->ndictionary_entries];
	memset(de, 0, sizeof(*de));
	de->word = strdup(word);
	de->canonical_word = strdup(canonical_word);
	de->p_o_s = p_o_s;
	if (de->p_o_s == POS_VERB) {
		de->verb.syntax = syntax;
		de->verb.fn = action;
	}
	ctx->ndictionary_entries++;
}

void snis_nl_add_dictionary_verb(struct snis_nl_context *ctx,
		char *word, char *canonical_word, char *syntax, snis_nl_verb_function action)
{
	generic_add_dictionary_word(ctx, word, canonical_word, POS_VERB, syntax, action);
}

void snis_nl_add_dictionary_word(struct snis_nl_context *ctx, char *word, char *canonical_word, int p_o_s)
{
	generic_add_dictionary_word(ctx, word, canonical_word, p_o_s, NULL, NULL);
}

void snis_nl_add_external_lookup(struct snis_nl_context *ctx, snis_nl_external_noun_lookup lookup)
{
	if (ctx->external_lookup) {
		fprintf(stderr, "Too many lookup functions added\n");
	}
	ctx->external_lookup = lookup;
}

void snis_nl_add_error_function(struct snis_nl_context *ctx, snis_nl_error_function error_func)
{
	if (ctx->error_function) {
		fprintf(stderr, "Too many error functions added\n");
	}
	ctx->error_function = error_func;
}

void snis_nl_add_multiword_preprocessor(struct snis_nl_context *ctx, snis_nl_multiword_preprocessor_fn preprocessor)
{
	if (ctx->multiword_preprocessor_fn) {
		fprintf(stderr, "Too many multiword preprocessor functions added\n");
	}
	ctx->multiword_preprocessor_fn = preprocessor;
}

void snis_nl_print_verbs_by_fn(struct snis_nl_context *ctx, const char *label, snis_nl_verb_function verb_function)
{
	int i, count;

	count = 0;
	for (i = 0; i < ctx->ndictionary_entries; i++) {
		if (ctx->dictionary[i].p_o_s != POS_VERB)
			continue;
		if (ctx->dictionary[i].verb.fn != verb_function)
			continue;
		printf("%s %30s %30s %10s\n", label, ctx->dictionary[i].word,
			ctx->dictionary[i].canonical_word, ctx->dictionary[i].verb.syntax);
		count++;
	}
	if (count > 0)
		printf("%s total count: %d\n", label, count);
}

void *snis_nl_get_user_context(struct snis_nl_context *ctx)
{
	return ctx->user_context;
}

void snis_nl_set_user_context(struct snis_nl_context *ctx, void *user_context)
{
	ctx->user_context = user_context;
}

void snis_nl_context_free(struct snis_nl_context *nl)
{
	free(nl);
}

#ifdef TEST_NL
static void init_synonyms(struct snis_nl_context *ctx)
{
	snis_nl_add_synonym(ctx, "cut", "lower");
	snis_nl_add_synonym(ctx, "decrease", "lower");
	snis_nl_add_synonym(ctx, "boost", "raise");
	snis_nl_add_synonym(ctx, "increase", "raise");
	snis_nl_add_synonym(ctx, "calculate", "compute");
	snis_nl_add_synonym(ctx, "figure", "compute");
	snis_nl_add_synonym(ctx, "activate", "engage");
	snis_nl_add_synonym(ctx, "actuate", "engage");
	snis_nl_add_synonym(ctx, "start", "engage");
	snis_nl_add_synonym(ctx, "energize", "engage");
	snis_nl_add_synonym(ctx, "deactivate", "disengage");
	snis_nl_add_synonym(ctx, "deenergize", "disengage");
	snis_nl_add_synonym(ctx, "shutdown", "disengage");
	snis_nl_add_synonym(ctx, "deploy", "launch");
}

static void generic_verb_action(struct snis_nl_context *ctx,
		int argc, char *argv[], int pos[],
		__attribute__((unused)) union snis_nl_extra_data extra_data[])
{
	int i;

	printf("generic_verb_action: ");
	for (i = 0; i < argc; i++) {
		printf("%s(%s) ", argv[i], part_of_speech[pos[i]]);
	}
	printf("\n");
}

static void init_dictionary(struct snis_nl_context *ctx)
{
	snis_nl_add_dictionary_verb(ctx, "describe",		"navigate",	"n", generic_verb_action);
	snis_nl_add_dictionary_verb(ctx, "describe",		"navigate",	"an", generic_verb_action);
	snis_nl_add_dictionary_verb(ctx, "navigate",		"navigate",	"pn", generic_verb_action);
	snis_nl_add_dictionary_verb(ctx, "navigate",		"navigate",	"pnq", generic_verb_action);
	snis_nl_add_dictionary_verb(ctx, "head",		"navigate",	"pn", generic_verb_action);
	snis_nl_add_dictionary_verb(ctx, "head",		"navigate",	"pnq", generic_verb_action);
	snis_nl_add_dictionary_verb(ctx, "set",		"set",		"npq", generic_verb_action);
	snis_nl_add_dictionary_verb(ctx, "set",		"set",		"npa", generic_verb_action);
	snis_nl_add_dictionary_verb(ctx, "set",		"set",		"npn", generic_verb_action);
	snis_nl_add_dictionary_verb(ctx, "set",		"set",		"npnq", generic_verb_action);
	snis_nl_add_dictionary_verb(ctx, "lower",		"lower",	"npq", generic_verb_action);
	snis_nl_add_dictionary_verb(ctx, "lower",		"lower",	"n", generic_verb_action);
	snis_nl_add_dictionary_verb(ctx, "raise",		"raise",	"nq", generic_verb_action);
	snis_nl_add_dictionary_verb(ctx, "raise",		"raise",	"npq", generic_verb_action);
	snis_nl_add_dictionary_verb(ctx, "raise",		"raise",	"n", generic_verb_action);
	snis_nl_add_dictionary_verb(ctx, "engage",		"engage",	"n", generic_verb_action);
	snis_nl_add_dictionary_verb(ctx, "disengage",	"disengage",	"n", generic_verb_action);
	snis_nl_add_dictionary_verb(ctx, "stop",		"disengage",	"n", generic_verb_action);
	snis_nl_add_dictionary_verb(ctx, "turn",		"turn",		"pn", generic_verb_action);
	snis_nl_add_dictionary_verb(ctx, "turn",		"turn",		"aqa", generic_verb_action);
	snis_nl_add_dictionary_verb(ctx, "turn",		"turn",		"qaa", generic_verb_action);
	snis_nl_add_dictionary_verb(ctx, "compute",		"compute",	"npn", generic_verb_action);
	snis_nl_add_dictionary_verb(ctx, "report",		"report",	"n", generic_verb_action);
	snis_nl_add_dictionary_verb(ctx, "yaw",		"yaw",		"aq", generic_verb_action);
	snis_nl_add_dictionary_verb(ctx, "pitch",		"pitch",	"aq", generic_verb_action);
	snis_nl_add_dictionary_verb(ctx, "roll",		"roll",		"aq", generic_verb_action);
	snis_nl_add_dictionary_verb(ctx, "zoom",		"zoom",		"p", generic_verb_action);
	snis_nl_add_dictionary_verb(ctx, "zoom",		"zoom",		"pq", generic_verb_action);
	snis_nl_add_dictionary_verb(ctx, "shut",		"shut",		"an", generic_verb_action);
	snis_nl_add_dictionary_verb(ctx, "shut",		"shut",		"na", generic_verb_action);
	snis_nl_add_dictionary_verb(ctx, "launch",		"launch",	"n", generic_verb_action);
	snis_nl_add_dictionary_verb(ctx, "eject",		"eject",	"n", generic_verb_action);
	snis_nl_add_dictionary_verb(ctx, "full",		"full",		"n", generic_verb_action);
	snis_nl_add_dictionary_verb(ctx, "how",		"how",		"anxPx", generic_verb_action);

	snis_nl_add_dictionary_word(ctx, "drive",		"drive",	POS_NOUN);
	snis_nl_add_dictionary_word(ctx, "system",		"system",	POS_NOUN);
	snis_nl_add_dictionary_word(ctx, "starbase",		"starbase",	POS_NOUN);
	snis_nl_add_dictionary_word(ctx, "base",		"starbase",	POS_NOUN);
	snis_nl_add_dictionary_word(ctx, "planet",		"planet",	POS_NOUN);
	snis_nl_add_dictionary_word(ctx, "ship",		"ship",		POS_NOUN);
	snis_nl_add_dictionary_word(ctx, "bot",		"bot",		POS_NOUN);
	snis_nl_add_dictionary_word(ctx, "shields",		"shields",	POS_NOUN);
	snis_nl_add_dictionary_word(ctx, "throttle",		"throttle",	POS_NOUN);
	snis_nl_add_dictionary_word(ctx, "factor",		"factor",	POS_NOUN);
	snis_nl_add_dictionary_word(ctx, "coolant",		"coolant",	POS_NOUN);
	snis_nl_add_dictionary_word(ctx, "level",		"level",	POS_NOUN);
	snis_nl_add_dictionary_word(ctx, "energy",		"energy",	POS_NOUN);
	snis_nl_add_dictionary_word(ctx, "power",		"energy",	POS_NOUN);
	snis_nl_add_dictionary_word(ctx, "stop",		"stop",		POS_NOUN); /* as in:full stop */
	snis_nl_add_dictionary_word(ctx, "speed",		"speed",	POS_NOUN);
	snis_nl_add_dictionary_word(ctx, "asteroid",		"asteroid",	POS_NOUN);
	snis_nl_add_dictionary_word(ctx, "nebula",		"nebula",	POS_NOUN);
	snis_nl_add_dictionary_word(ctx, "star",		"star",		POS_NOUN);
	snis_nl_add_dictionary_word(ctx, "range",		"range",	POS_NOUN);
	snis_nl_add_dictionary_word(ctx, "distance",		"range",	POS_NOUN);
	snis_nl_add_dictionary_word(ctx, "weapons",		"weapons",	POS_NOUN);
	snis_nl_add_dictionary_word(ctx, "screen",		"screen",	POS_NOUN);
	snis_nl_add_dictionary_word(ctx, "robot",		"robot",	POS_NOUN);
	snis_nl_add_dictionary_word(ctx, "torpedo",		"torpedo",	POS_NOUN);
	snis_nl_add_dictionary_word(ctx, "phasers",		"phasers",	POS_NOUN);
	snis_nl_add_dictionary_word(ctx, "maneuvering",	"maneuvering",	POS_NOUN);
	snis_nl_add_dictionary_word(ctx, "thruster",		"thrusters",	POS_NOUN);
	snis_nl_add_dictionary_word(ctx, "thrusters",	"thrusters",	POS_NOUN);
	snis_nl_add_dictionary_word(ctx, "sensor",		"sensors",	POS_NOUN);
	snis_nl_add_dictionary_word(ctx, "science",		"science",	POS_NOUN);
	snis_nl_add_dictionary_word(ctx, "comms",		"comms",	POS_NOUN);
	snis_nl_add_dictionary_word(ctx, "enemy",		"enemy",	POS_NOUN);
	snis_nl_add_dictionary_word(ctx, "derelict",		"derelict",	POS_NOUN);
	snis_nl_add_dictionary_word(ctx, "computer",		"computer",	POS_NOUN);
	snis_nl_add_dictionary_word(ctx, "fuel",		"fuel",		POS_NOUN);
	snis_nl_add_dictionary_word(ctx, "radiation",	"radiation",	POS_NOUN);
	snis_nl_add_dictionary_word(ctx, "wavelength",	"wavelength",	POS_NOUN);
	snis_nl_add_dictionary_word(ctx, "charge",		"charge",	POS_NOUN);
	snis_nl_add_dictionary_word(ctx, "magnets",		"magnets",	POS_NOUN);
	snis_nl_add_dictionary_word(ctx, "gate",		"gate",		POS_NOUN);
	snis_nl_add_dictionary_word(ctx, "percent",		"percent",	POS_NOUN);
	snis_nl_add_dictionary_word(ctx, "sequence",		"sequence",	POS_NOUN);
	snis_nl_add_dictionary_word(ctx, "core",		"core",		POS_NOUN);
	snis_nl_add_dictionary_word(ctx, "code",		"code",		POS_NOUN);
	snis_nl_add_dictionary_word(ctx, "hull",		"hull",		POS_NOUN);
	snis_nl_add_dictionary_word(ctx, "scanner",		"scanner",	POS_NOUN);
	snis_nl_add_dictionary_word(ctx, "scanners",		"scanners",	POS_NOUN);
	snis_nl_add_dictionary_word(ctx, "detail",		"details",	POS_NOUN);
	snis_nl_add_dictionary_word(ctx, "report",		"report",	POS_NOUN);
	snis_nl_add_dictionary_word(ctx, "damage",		"damage",	POS_NOUN);
	snis_nl_add_dictionary_word(ctx, "course",		"course",	POS_NOUN);
	snis_nl_add_dictionary_word(ctx, "earth",		"earth",	POS_NOUN);
	snis_nl_add_dictionary_word(ctx, "fuel",		"fuel",		POS_NOUN);


	snis_nl_add_dictionary_word(ctx, "a",		"a",		POS_ARTICLE);
	snis_nl_add_dictionary_word(ctx, "an",		"an",		POS_ARTICLE);
	snis_nl_add_dictionary_word(ctx, "the",		"the",		POS_ARTICLE);

	snis_nl_add_dictionary_word(ctx, "above",		"above",	POS_PREPOSITION);
	snis_nl_add_dictionary_word(ctx, "aboard",		"aboard",	POS_PREPOSITION);
	snis_nl_add_dictionary_word(ctx, "across",		"across",	POS_PREPOSITION);
	snis_nl_add_dictionary_word(ctx, "after",		"after",	POS_PREPOSITION);
	snis_nl_add_dictionary_word(ctx, "along",		"along",	POS_PREPOSITION);
	snis_nl_add_dictionary_word(ctx, "alongside",	"alongside",	POS_PREPOSITION);
	snis_nl_add_dictionary_word(ctx, "at",		"at",		POS_PREPOSITION);
	snis_nl_add_dictionary_word(ctx, "atop",		"atop",		POS_PREPOSITION);
	snis_nl_add_dictionary_word(ctx, "around",		"around",	POS_PREPOSITION);
	snis_nl_add_dictionary_word(ctx, "before",		"before",	POS_PREPOSITION);
	snis_nl_add_dictionary_word(ctx, "behind",		"behind",	POS_PREPOSITION);
	snis_nl_add_dictionary_word(ctx, "beneath",		"beneath",	POS_PREPOSITION);
	snis_nl_add_dictionary_word(ctx, "below",		"below",	POS_PREPOSITION);
	snis_nl_add_dictionary_word(ctx, "beside",		"beside",	POS_PREPOSITION);
	snis_nl_add_dictionary_word(ctx, "besides",		"besides",	POS_PREPOSITION);
	snis_nl_add_dictionary_word(ctx, "between",		"between",	POS_PREPOSITION);
	snis_nl_add_dictionary_word(ctx, "by",		"by",		POS_PREPOSITION);
	snis_nl_add_dictionary_word(ctx, "down",		"down",		POS_PREPOSITION);
	snis_nl_add_dictionary_word(ctx, "during",		"during",	POS_PREPOSITION);
	snis_nl_add_dictionary_word(ctx, "except",		"except",	POS_PREPOSITION);
	snis_nl_add_dictionary_word(ctx, "for",		"for",		POS_PREPOSITION);
	snis_nl_add_dictionary_word(ctx, "from",		"from",		POS_PREPOSITION);
	snis_nl_add_dictionary_word(ctx, "in",		"in",		POS_PREPOSITION);
	snis_nl_add_dictionary_word(ctx, "inside",		"inside",	POS_PREPOSITION);
	snis_nl_add_dictionary_word(ctx, "of",		"of",		POS_PREPOSITION);
	snis_nl_add_dictionary_word(ctx, "off",		"off",		POS_PREPOSITION);
	snis_nl_add_dictionary_word(ctx, "on",		"on",		POS_PREPOSITION);
	snis_nl_add_dictionary_word(ctx, "onto",		"onto",		POS_PREPOSITION);
	snis_nl_add_dictionary_word(ctx, "out",		"out",		POS_PREPOSITION);
	snis_nl_add_dictionary_word(ctx, "outside",		"outside",	POS_PREPOSITION);
	snis_nl_add_dictionary_word(ctx, "through",		"through",	POS_PREPOSITION);
	snis_nl_add_dictionary_word(ctx, "throughout",	"throughout",	POS_PREPOSITION);
	snis_nl_add_dictionary_word(ctx, "to",		"to",		POS_PREPOSITION);
	snis_nl_add_dictionary_word(ctx, "toward",		"toward",	POS_PREPOSITION);
	snis_nl_add_dictionary_word(ctx, "under",		"under",	POS_PREPOSITION);
	snis_nl_add_dictionary_word(ctx, "up",		"up",		POS_PREPOSITION);
	snis_nl_add_dictionary_word(ctx, "until",		"until",	POS_PREPOSITION);
	snis_nl_add_dictionary_word(ctx, "with",		"with",		POS_PREPOSITION);
	snis_nl_add_dictionary_word(ctx, "within",		"within",	POS_PREPOSITION);
	snis_nl_add_dictionary_word(ctx, "without",		"without",	POS_PREPOSITION);

	snis_nl_add_dictionary_word(ctx, "or",		"or",		POS_SEPARATOR);
	snis_nl_add_dictionary_word(ctx, "and",		"and",		POS_SEPARATOR);
	snis_nl_add_dictionary_word(ctx, "then",		"then",		POS_SEPARATOR);
	snis_nl_add_dictionary_word(ctx, ",",		",",		POS_SEPARATOR);
	snis_nl_add_dictionary_word(ctx, ".",		".",		POS_SEPARATOR);
	snis_nl_add_dictionary_word(ctx, ";",		";",		POS_SEPARATOR);
	snis_nl_add_dictionary_word(ctx, "!",		"!",		POS_SEPARATOR);
	snis_nl_add_dictionary_word(ctx, "?",		"?",		POS_SEPARATOR);

	snis_nl_add_dictionary_word(ctx, "damage",		"damage",	POS_ADJECTIVE);
	snis_nl_add_dictionary_word(ctx, "status",		"status",	POS_ADJECTIVE);
	snis_nl_add_dictionary_word(ctx, "warp",		"warp",		POS_ADJECTIVE);
	snis_nl_add_dictionary_word(ctx, "impulse",		"impulse",	POS_ADJECTIVE);
	snis_nl_add_dictionary_word(ctx, "docking",		"docking",	POS_ADJECTIVE);
	snis_nl_add_dictionary_word(ctx, "star",		"star",		POS_ADJECTIVE);
	snis_nl_add_dictionary_word(ctx, "space",		"space",	POS_ADJECTIVE);
	snis_nl_add_dictionary_word(ctx, "mining",		"mining",	POS_ADJECTIVE);
	snis_nl_add_dictionary_word(ctx, "energy",		"energy",	POS_ADJECTIVE);
	snis_nl_add_dictionary_word(ctx, "main",		"main",		POS_ADJECTIVE);
	snis_nl_add_dictionary_word(ctx, "navigation",	"navigation",	POS_ADJECTIVE);
	snis_nl_add_dictionary_word(ctx, "comms",		"comms",	POS_ADJECTIVE);
	snis_nl_add_dictionary_word(ctx, "engineering",	"engineering",	POS_ADJECTIVE);
	snis_nl_add_dictionary_word(ctx, "science",		"science",	POS_ADJECTIVE);
	snis_nl_add_dictionary_word(ctx, "enemy",		"enemy",	POS_ADJECTIVE);
	snis_nl_add_dictionary_word(ctx, "derelict",		"derelict",	POS_ADJECTIVE);
	snis_nl_add_dictionary_word(ctx, "solar",		"solar",	POS_ADJECTIVE);
	snis_nl_add_dictionary_word(ctx, "nearest",		"nearest",	POS_ADJECTIVE);
	snis_nl_add_dictionary_word(ctx, "closest",		"nearest",	POS_ADJECTIVE);
	snis_nl_add_dictionary_word(ctx, "nearby",		"nearest",	POS_ADJECTIVE);
	snis_nl_add_dictionary_word(ctx, "close",		"nearest",	POS_ADJECTIVE);
	snis_nl_add_dictionary_word(ctx, "up",		"up",		POS_ADJECTIVE);
	snis_nl_add_dictionary_word(ctx, "down",		"down",		POS_ADJECTIVE);
	snis_nl_add_dictionary_word(ctx, "left",		"left",		POS_ADJECTIVE);
	snis_nl_add_dictionary_word(ctx, "right",		"right",	POS_ADJECTIVE);
	snis_nl_add_dictionary_word(ctx, "self",		"self",		POS_ADJECTIVE);
	snis_nl_add_dictionary_word(ctx, "destruct",		"destruct",	POS_ADJECTIVE);
	snis_nl_add_dictionary_word(ctx, "self-destruct",	"self-destruct",	POS_ADJECTIVE);
	snis_nl_add_dictionary_word(ctx, "short",		"short",	POS_ADJECTIVE);
	snis_nl_add_dictionary_word(ctx, "long",		"long",		POS_ADJECTIVE);
	snis_nl_add_dictionary_word(ctx, "range",		"range",	POS_ADJECTIVE);
	snis_nl_add_dictionary_word(ctx, "full",		"maximum",	POS_ADJECTIVE);
	snis_nl_add_dictionary_word(ctx, "max",		"maximum",	POS_ADJECTIVE);
	snis_nl_add_dictionary_word(ctx, "maximum",		"maximum",	POS_ADJECTIVE);
	snis_nl_add_dictionary_word(ctx, "planet",		"planet",	POS_ADJECTIVE);
	snis_nl_add_dictionary_word(ctx, "degrees",		"degrees",	POS_ADJECTIVE);
	snis_nl_add_dictionary_word(ctx, "much",		"much",		POS_ADJECTIVE);
	snis_nl_add_dictionary_word(ctx, "far",		"far",		POS_ADJECTIVE);

	snis_nl_add_dictionary_word(ctx, "percent",		"percent",	POS_ADVERB);
	snis_nl_add_dictionary_word(ctx, "quickly",		"quickly",	POS_ADVERB);
	snis_nl_add_dictionary_word(ctx, "rapidly",		"quickly",	POS_ADVERB);
	snis_nl_add_dictionary_word(ctx, "swiftly",		"quickly",	POS_ADVERB);
	snis_nl_add_dictionary_word(ctx, "slowly",		"slowly",	POS_ADVERB);

	snis_nl_add_dictionary_word(ctx, "it",		"it",		POS_PRONOUN);
	snis_nl_add_dictionary_word(ctx, "me",		"me",		POS_PRONOUN);
	snis_nl_add_dictionary_word(ctx, "we",		"we",		POS_PRONOUN);
	snis_nl_add_dictionary_word(ctx, "them",		"them",		POS_PRONOUN);
	snis_nl_add_dictionary_word(ctx, "all",		"all",		POS_PRONOUN);
	snis_nl_add_dictionary_word(ctx, "everything",	"everything",	POS_PRONOUN);

	snis_nl_add_dictionary_word(ctx, "do",		"do",		POS_AUXVERB);
	snis_nl_add_dictionary_word(ctx, "be",		"be",		POS_AUXVERB);
	snis_nl_add_dictionary_word(ctx, "have",		"have",		POS_AUXVERB);
	snis_nl_add_dictionary_word(ctx, "will",		"will",		POS_AUXVERB);
	snis_nl_add_dictionary_word(ctx, "shall",		"shall",	POS_AUXVERB);
	snis_nl_add_dictionary_word(ctx, "would",		"would",	POS_AUXVERB);
	snis_nl_add_dictionary_word(ctx, "could",		"could",	POS_AUXVERB);
	snis_nl_add_dictionary_word(ctx, "should",		"should",	POS_AUXVERB);
	snis_nl_add_dictionary_word(ctx, "can",		"can",		POS_AUXVERB);
	snis_nl_add_dictionary_word(ctx, "may",		"may",		POS_AUXVERB);
	snis_nl_add_dictionary_word(ctx, "must",		"must",		POS_AUXVERB);
	snis_nl_add_dictionary_word(ctx, "ought",		"ought",	POS_AUXVERB);

}

int main(int argc, char *argv[])
{
	int i;
	char line[255];
	char *x;

	struct snis_nl_context *ctx = snis_nl_context_create();
	ctx->debuglevel = 1;

	init_synonyms(ctx);
	init_dictionary(ctx);

	for (i = 1; i < argc; i++)
		snis_nl_parse_natural_language_request(ctx, argv[i]);

	if (argc != 1)
		return 0;
	do {
		printf("Enter string to parse: ");
		fflush(stdout);
		x = fgets(line, sizeof(line), stdin);
		if (!x)
			break;

		/* cut off the newline that fgets leaves in. */
		i = strlen(x);
		if (i > 0)
			x[i - 1] = '\0';

		snis_nl_parse_natural_language_request(ctx, line);
	} while (1);
	snis_nl_context_free(ctx);
	return 0;
}

#endif

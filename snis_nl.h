#ifndef SNIS_NL_H__
#define SNIS_NL_H__
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

#include <stdint.h>

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
#define POS_EXTERNAL_NOUN	11
#define POS_AUXVERB		12
#define POS_EXPLETIVE		13

struct snis_nl_context;

union snis_nl_extra_data;
typedef void (*snis_nl_verb_function)(struct snis_nl_context *ctx,
				void *user_context, int argc, char *argv[], int part_of_speech[],
				union snis_nl_extra_data *extra_data);
typedef void (*snis_nl_error_function)(void *user_context);
typedef void (*snis_nl_multiword_preprocessor_fn)(struct snis_nl_context *ctx,
						char *word, int encode_or_decode);
#define SNIS_NL_ENCODE 1
#define SNIS_NL_DECODE 2

struct snis_nl_verb_data {
	snis_nl_verb_function fn;
	char *syntax;	/* Syntax of verb, denoted by characters.
			 * 'n' -- single noun (or pronoun)
			 * 'l' -- one or more nouns (or pronoun)
			 * 'p' -- preposition
			 * 'P' -- pronoun (unsubstitued, in most cases you probably want 'n' for noun.)
			 * 'q' -- quantity, that is to say, a number.
			 * 'a' -- adjective
			 * 'x' -- auxiliary verb (be, do, have, will, shall, would, should,
			 *	  can, could, may, might, must, ought, etc. )
			 *
			 * For example: "put", as in "put the butter on the bread with the knife"
			 * has a syntax of "npnpn", while "put" as in "put the coat on",
			 * has a syntax of "np".
			 */
};

struct snis_nl_noun_data {
	int nothing;
};

struct snis_nl_article_data {
	int definite;
};

struct snis_nl_preposition_data {
	int nothing;
};

struct snis_nl_separator_data {
	int nothing;
};

struct snis_nl_adjective_data {
	int nothing;
};

struct snis_nl_adverb_data {
	int nothing;
};

struct snis_nl_number_data {
	float value;
};

struct snis_nl_pronoun_data {
	int nothing;
};

struct snis_nl_external_noun_data {
	uint32_t handle;
};

union snis_nl_extra_data {
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

typedef uint32_t (*snis_nl_external_noun_lookup)(struct snis_nl_context *ctx,
			void *user_context, char *word);

struct snis_nl_context *snis_nl_context_create(void);
void snis_nl_context_free(struct snis_nl_context *ctx);
void snis_nl_add_synonym(struct snis_nl_context *ctx, char *synonym, char *canonical_word);
void snis_nl_add_dictionary_word(struct snis_nl_context *ctx, char *word, char *canonical_word, int part_of_speech);
void snis_nl_add_dictionary_verb(struct snis_nl_context *ctx,
		char *word, char *canonical_word, char *syntax, snis_nl_verb_function action);
void snis_nl_add_external_lookup(struct snis_nl_context *ctx, snis_nl_external_noun_lookup lookup);
void snis_nl_add_error_function(struct snis_nl_context *ctx, snis_nl_error_function error_func);
void snis_nl_add_multiword_preprocessor(struct snis_nl_context *ctx,
			snis_nl_multiword_preprocessor_fn multiword_processor);
void snis_nl_parse_natural_language_request(struct snis_nl_context *ctx, void *user_context, char *text);
int snis_nl_test_parse_natural_language_request(struct snis_nl_context *nl, void *user_context, char *text);
void snis_nl_print_verbs_by_fn(struct snis_nl_context *ctx, const char *label, snis_nl_verb_function verb_function);
void snis_nl_set_current_topic(struct snis_nl_context *ctx,
		int part_of_speech, char *word, union snis_nl_extra_data extra_data);
void snis_nl_clear_current_topic(struct snis_nl_context *ctx);

#endif

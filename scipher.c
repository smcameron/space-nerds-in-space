/*
	Copyright (C) 2019 Stephen M. Cameron
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
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include "mtwist.h"
#include "mathutils.h"
#define KEYSIZE 26

struct scipher_key {
	char key[KEYSIZE];
};

void scipher_reset_key(struct scipher_key *key)
{
	memset(key->key, '_', KEYSIZE);
}

void scipher_init_key(struct scipher_key *key, char *keyvalue)
{
	memcpy(key->key, keyvalue, 26);
}

/* Returns cipher key as a 26 character string, null terminated.
 * keystring should be capable of holding 27 chars
 */
void scipher_key_to_string(struct scipher_key *key, char *keystring)
{
	memcpy(keystring, key->key, 26);
	keystring[27] = '\0';
}

void scipher_modify_key(struct scipher_key *key, char from, char to)
{
	if (toupper(from) < 'A' || toupper(from) > 'Z')
		return;
	if ((toupper(to) < 'A' || toupper(to) > 'Z') && to != '_')
		return;
	key->key[(int) from] = to;
}

void scipher_key_free(struct scipher_key *key)
{
	free(key);
}

struct scipher_key *scipher_make_key(char *initial_key_value)
{
	int i, j, tmp;
	struct scipher_key *key;

	key = malloc(sizeof(*key));
	if (!key)
		return key;

	if (initial_key_value) {
		memcpy(key->key, initial_key_value, 26);
		return key;
	}

	/* Fill key with A - Z, in sequence. */
	for (i = 0; i < KEYSIZE; i++)
		key->key[i] = i + 'A';

	/* Swap each element of key with a randomly selected element of key */
	for (i = 0; i < KEYSIZE; i++) {
		j = snis_rand() % KEYSIZE;
		tmp = key->key[i];
		key->key[i] = key->key[j];
		key->key[j] = tmp;
	}
	return key;
}

void scipher_encipher(char *plaintext, char *ciphertext, int buffersize, struct scipher_key *key)
{
	int i;
	char c;

	for (i = 0; plaintext[i] != '\0' && i < buffersize - 1; i++) {
		c = toupper(plaintext[i]);
		if (c < 'A' || c > 'Z') {
			ciphertext[i] = c;
			continue;
		}
		ciphertext[i] = key->key[c - 'A'];
	}
	ciphertext[i] = '\0';
}

void scipher_decipher(char *ciphertext, char *plaintext, int buffersize, struct scipher_key *key)
{
	int i, j;
	char c;

	for (i = 0; ciphertext[i] != '\0' && i < buffersize - 1; i++) {
		c = ciphertext[i];
		if (c < 'A' || c > 'Z') {
			plaintext[i] = c;
			continue;
		}
		for (j = 0; j < KEYSIZE; j++) {
			if (key->key[j] == c) {
				plaintext[i] = j + 'A';
				break;
			}
		}
		if (j > KEYSIZE)
			plaintext[i] = '_';
	}
	plaintext[i] = '\0';
}

char scipher_encipher_char(char c, struct scipher_key *key)
{
	if (toupper(c) < 'A' || toupper(c) > 'Z')
		return c;

	c = key->key[c - 'A'];
	return c;
}

char scipher_decipher_char(char c, struct scipher_key *key)
{
	int i;

	if (toupper(c) < 'A' || toupper(c) > 'Z')
		return c;

	for (i = 0; i < 26; i++) {
		if (toupper(c) == key->key[i])
			return i + 'A';
	}
	return '_';
}

#ifdef TEST_SCIPHER

#include <stdio.h>

int main(int argc, char *argv[])
{
	struct scipher_key *key;
	char *plaintext = "The lazy brown dog jumps over the quick fox";
	char ciphertext[1000];
	char deciphertext[1000];
	char *sometext = "Four score and seven years ago our fathers brought forth on this "
			"continent, a new nation, conceived in Liberty, and dedicated to the "
			"proposition that all men are created equal.";

	key = scipher_make_key(NULL);
	scipher_encipher(plaintext, ciphertext, sizeof(ciphertext), key);
	scipher_decipher(ciphertext, deciphertext, sizeof(deciphertext), key);
	scipher_key_free(key);

	printf("Plaintext = '%s'\n", plaintext);
	printf("Ciphertext = '%s'\n", ciphertext);
	printf("Deciphered = '%s'\n", deciphertext);

	key = scipher_make_key(NULL);
	scipher_encipher(sometext, ciphertext, sizeof(ciphertext), key);
	scipher_decipher(ciphertext, deciphertext, sizeof(deciphertext), key);
	scipher_key_free(key);

	printf("Plaintext = '%s'\n", sometext);
	printf("Ciphertext = '%s'\n", ciphertext);
	printf("Deciphered = '%s'\n", deciphertext);

	return 0;
}
#endif


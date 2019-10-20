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

/* This implements a simple substitution cipher in which the letters a - z
 * are mapped to different letters from a - z.  Non letter characters are
 * not enciphered.
 */

struct scipher_key;

/* Clears the key (sets to all '_'). */
void scipher_reset_key(struct scipher_key *key);

/* Generates a new key with initial_key_value.
 * If initial_key_value is NULL, a random key is generated. */
struct scipher_key *scipher_make_key(char *initial_key_value);

/* Initialize a key to a specified value.  Keyvalue should be a string at least 26 characters
 * long and should map A-Z to some set of characters.  Unknown mappings can be represented by
 * the char '_';
 */
void scipher_init_key(struct scipher_key *key, char *keyvalue);

/* Returns cipher key as a 26 character string, null terminated.
 * keystring should be capable of holding 27 chars
 */
void scipher_key_to_string(struct scipher_key *key, char *keystring);

/* Modify a key by setting a particular position to a particular value.
 * E.g. if you want 'A' to mape to 'K', then:
 * scipher_modify_key(key, 'A', 'K');
 */
void scipher_modify_key(struct scipher_key *key, char from, char to);

/* Frees a previously generated scipher key */
void scipher_key_free(struct scipher_key *key);

/* enciphers plaintext using key. Output is in ciphertext */
void scipher_encipher(char *plaintext, char *ciphertext, int buffersize, struct scipher_key *key);

/* deciphers ciphertext using key. Output is in plaintext */
void scipher_decipher(char *ciphertext, char *plaintext, int buffersize, struct scipher_key *key);

char scipher_encipher_char(char c, struct scipher_key *key);

char scipher_decipher_char(char c, struct scipher_key *key);


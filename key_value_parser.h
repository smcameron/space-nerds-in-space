#ifndef KEY_VALUE_PARSER_H__
#define KEY_VALUE_PARSER_H__
/*
	Copyright (C) 2010 Stephen M. Cameron
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

/* Parses line according to key_value_specification into base_address + appropriate offset
 * line should be of the form "key: value\n"
 *
 * So, for example, suppose you have:
 * struct abc {
 *   int x;
 *   char y[10];
 *   uint16_t shortvalue;
 *} x;
 *
 * And so you want to be able to parse a file like:
 * ----------------
 * x: 10
 * y: abc
 * shortvalue: 20
 * ----------------
 *
 * So:
 *
 * struct key_value_specification mykvs[] = {
 *	{ "x", KVS_INTEGER, offsetof(abc, x), sizeof(int), },
 *	{ "y", KVS_STRING, offsetof(abc, y), sizeof(char) * 10 },
 *	{ "shortvalue", KVS_UNSIGNED_SHORT, offsetof(abc, shortvaluey), sizeof(uint16_t), },
 *	{ NULL, 0, 0, 0, 0, },
 * };
 *
 * struct abc myabc;
 * char *lines = "x: 10\n"
 *              "y: abc\n"
 *              "shortvalue: 20\n";
 *
 * int rc = key_value_parse_lines(mykvs, lines, &myabc);
 *
 */

struct key_value_specification {
	char *key;
	char type;
	int address_offset;
	int size;
};

#define KVS_STRING 's'
#define KVS_UINT64 'q'
#define KVS_UINT32 'w'
#define KVS_UINT16 'h'
#define KVS_UINT8 'b'
#define KVS_INT64 'Q'
#define KVS_INT32 'W'
#define KVS_INT16 'H'
#define KVS_INT8 'B'
#define KVS_DOUBLE 'd'
#define KVS_FLOAT 'f'

int key_value_parse_line(const struct key_value_specification *kvs, const char *line, void *base_address);
int key_value_parse_lines(const struct key_value_specification *kvs, const char *lines, void *base_address);

#endif

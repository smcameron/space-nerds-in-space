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

/* Parses line according to key_value_specification into base_address[address_index] + appropriate offset
 * line should be of the form "key: value\n"
 *
 * So, for example, suppose you have:
 * struct abc {
 *   int x;
 *   char y[10];
 *   uint16_t shortvalue;
 *} x;
 *
 * struct xyz {
 *   uint64_t j;
 *   uint32_t k;
 *   float f;
 * } y;
 *
 * And so you want to be able to parse a file like this to get the data into
 * those structs::
 * ----------------
 * x: 10
 * y: abc
 * shortvalue: 20
 * j: 7
 * k: 8
 * f: 3.1415
 * ----------------
 *
 * So:
 *
 * struct key_value_specification mykvs[] = {
 *	{ "x", KVS_INT32, 0, offsetof(struct abc, x), sizeof(int), },
 *	{ "y", KVS_STRING, 0, offsetof(struct abc, y), sizeof(char) * 10 },
 *	{ "shortvalue", KVS_UINT16, 0, offsetof(struct abc, shortvaluey), sizeof(uint16_t), },
 *      { "j", KVS_INT64, 1, offsetof(struct xyz, j), sizeof(uint64_t), },
 *      { "k", KVS_INT32, 1, offsetof(struct xyz, k), sizeof(uint32_t), },
 *      { "f", KVS_INT32, 1, offsetof(struct xyz, k), sizeof(uint32_t), },
 *	{ NULL, 0, 0, 0, 0, 0, },
 * };
 *
 * struct abc myabc;
 * struct xyz myxyz;
 * char *lines = "x: 10\n"
 *              "y: abc\n"
 *              "j: 27\n"
 *              "k: 28\n"
 *              "f: 3.1415927\n"
 *              "shortvalue: 20\n";
 * void *base_address[] = { &myabc, &myxyz };
 *
 * int rc = key_value_parse_lines(mykvs, lines, base_address[]);
 *
 */

struct key_value_specification {
	char *key;		/* name of the field */
	char type;		/* type (see below) */
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
	int address_index;	/* 0 ... n, index into base_address[] parameter */
	int address_offset;	/* offset of field from base address */
	int size;		/* size of field in bytes */
};

/* Parse a line of text into base_address[<appropriate_index>]-><appropriate_offset> based on kvs */
int key_value_parse_line(const struct key_value_specification *kvs, const char *line, void *base_address[]);
/* Parse a series of lines of text into base_address[<appropriate_indices>]-><appropriate_offsets> based on kvs */
int key_value_parse_lines(const struct key_value_specification *kvs, const char *lines, void *base_address[]);
/* Write a series of lines of text based on base_address[<appropriate_indices>]-><appropriate_offsets> based on kvs */
int key_value_write_lines(FILE *f, struct key_value_specification *kvs, void *base_address[]);

/* key_value_get_value() Looks up key in kvs, extracts corresponding value from base_address[],
 * and copies the data to output_buffer.  Returns number of bytes copied, or -1 if the key
 * was not found.
 */
int key_value_get_value(struct key_value_specification *kvs, const char *key, void *base_address[],
			void *output_buffer, int output_buffersize);
struct key_value_specification *lookup_key_entry(struct key_value_specification *kvs, const char *key);
void key_value_specification_print(struct key_value_specification *kvs);

#endif

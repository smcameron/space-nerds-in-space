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

/*
struct key_value_specification {
	char *key;
	char type;
	int address_offset;
	int size;
};
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>
#include <inttypes.h>

#include "key_value_parser.h"

static char *strdup_to_newline(const char *input)
{
	int i;
	char *copy;

	for (i = 0; input[i] != '\n' && input[i] != '\0';)
		i++;
	if (i == 0)
		return NULL;
	copy = malloc(i + 1);
	memcpy(copy, input, i);
	copy[i] = '\0';
	return copy;
}

static int parse_string(const char *strvalue, struct key_value_specification *k, void *base_address)
{
	int i, len;
	for (i = 0; strvalue[i] == ' ' && strvalue[i] != '\n' && strvalue[i] != '\0';)
		i++;
	len = strlen(&strvalue[i]);
	if (len > k->size - 1)
		len = k->size - 1;
	char *str = (char *) base_address + k->address_offset;
	memset(str, 0, k->size);
	memcpy(str, &strvalue[i], len);
	return 0;
}

#define PARSE_INT(sign, size, format) \
static int parse_##sign##size(const char *strvalue, struct key_value_specification *k, void *base_address) \
{ \
	int rc; \
	sign ## size ## _t *ptr = (sign ## size ## _t *) ((unsigned char *) base_address + \
					k->address_offset); \
	rc = sscanf(strvalue, format, ptr); \
	return (rc != 1); \
}

PARSE_INT(uint, 8, "%*[ ]%hhu")	/* parse_uint8 defined here */
PARSE_INT(uint, 16, "%*[ ]%hu")	/* parse_uint16 defined here */
PARSE_INT(uint, 32, "%*[ ]%u")	/* parse_uint32 defined here */
PARSE_INT(int, 8, "%*[ ]%hhd")	/* parse_int8 defined here */
PARSE_INT(int, 16, "%*[ ]%hd")	/* parse_int16 defined here */
PARSE_INT(int, 32, "%*[ ]%d")	/* parse_int32 defined here */

/* Can't use the macros for 64 bit versions because "lld" and "llu" complain about (u)int64_t
 * and I couldn't figure out how to get ("%" PRId64) and ("%" PRIu64) to go through the macro.
 * No worries, they're tiny anyhow.
 */
static int parse_uint64(const char *strvalue, struct key_value_specification *k, void *base_address)
{
	int rc;
	uint64_t *ptr = (uint64_t *) ((unsigned char *) base_address + k->address_offset);
	rc = sscanf(strvalue, "%*[ ]%llu", (unsigned long long *) ptr);
	return (rc != 1);
}

static int parse_int64(const char *strvalue, struct key_value_specification *k, void *base_address)
{
	int rc;
	int64_t *ptr = (int64_t *) ((unsigned char *) base_address + k->address_offset);
	rc = sscanf(strvalue, "%*[ ]%lld", (signed long long *) ptr);
	return (rc != 1);
}

static int parse_double(const char *strvalue, struct key_value_specification *k, void *base_address)
{
	int rc;
	double *ptr = (double *) ((unsigned char *) base_address + k->address_offset);
	rc = sscanf(strvalue, "%*[ ]%lf", ptr);
	return (rc != 1);
}

static int parse_float(const char *strvalue, struct key_value_specification *k, void *base_address)
{
	int rc;
	float *ptr = (float *) ((unsigned char *) base_address + k->address_offset);
	rc = sscanf(strvalue, "%*[ ]%f", ptr);
	return (rc != 1);
}

static int key_value_parse_line_with_key(struct key_value_specification *k, char *line, void *base_address)
{
	char *valuestr;
	int rc, len, klen;

	klen = strlen(k->key);
	len = strlen(line);
	if (len < klen + 2)
		return 1;
	if (strncmp(line, k->key, klen) != 0)
		return 1;
	if (line[klen] != ':')
		return 1;

	/* At this point, we have a match on the key */
	valuestr = &line[klen + 1];
	switch (k->type) {
	case KVS_STRING:
		rc = parse_string(valuestr, k, base_address);
		break;
	case KVS_INT64:
		rc = parse_int64(valuestr, k, base_address);
		break;
	case KVS_INT32:
		rc = parse_int32(valuestr, k, base_address);
		break;
	case KVS_INT16:
		rc = parse_int16(valuestr, k, base_address);
		break;
	case KVS_INT8:
		rc = parse_int8(valuestr, k, base_address);
		break;
	case KVS_UINT64:
		rc = parse_uint64(valuestr, k, base_address);
		break;
	case KVS_UINT32:
		rc = parse_uint32(valuestr, k, base_address);
		break;
	case KVS_UINT16:
		rc = parse_uint16(valuestr, k, base_address);
		break;
	case KVS_UINT8:
		rc = parse_uint8(valuestr, k, base_address);
		break;
	case KVS_DOUBLE:
		rc = parse_double(valuestr, k, base_address);
		break;
	case KVS_FLOAT:
		rc = parse_float(valuestr, k, base_address);
		break;
	default:
		fprintf(stderr, "%s:%d: unknown key type '%c' for key '%s'\n",
			__func__, __LINE__, k->type, k->key);
		rc = -1;
		break;
	}
	return rc;
}

int key_value_parse_line(const struct key_value_specification *kvs, const char *line, void *base_address)
{
	struct key_value_specification *k;
	int rc;

	char *tmpline = strdup_to_newline(line);
	for (k = (struct key_value_specification *) kvs; k->key != NULL; k++) {
		rc = key_value_parse_line_with_key(k, tmpline, base_address);
		if (rc > 0) /* key does not match */
			continue;
		if (rc < 0) /* key matches, but could not parse */
			fprintf(stderr, "Failed to parse '%s'.\n", tmpline);
		free(tmpline);
		return rc;
	}
	fprintf(stderr, "Unknown key '%s', ignoring.\n", tmpline);
	free(tmpline);
	return 0;
}

int key_value_parse_lines(const struct key_value_specification *kvs, const char *lines, void *base_address)
{
	char *p = (char *) lines;
	int rc;

	while (*p) {
		rc = key_value_parse_line(kvs, p, base_address);
		if (rc)
			return rc;
		while (*p != '\n' && *p != '\0')
			p++;
		while (*p == '\n')
			p++;
	}
	return 0;
}

#ifdef TEST_KEY_VALUE_PARSER
#include <math.h>

struct test_struct {
	uint8_t u8;
	uint16_t u16;
	uint32_t u32;
	uint16_t u64;
	int8_t i8;
	int16_t i16;
	int32_t i32;
	int16_t i64;
	char some_data[64];
	double doubledata;
	float floatdata;
};

#define KVS_U8FIELD(x) { #x, KVS_UINT8, offsetof(struct test_struct, x), sizeof(((struct test_struct *)0)->x), }
#define KVS_U16FIELD(x) { #x, KVS_UINT16, offsetof(struct test_struct, x), sizeof(((struct test_struct *)0)->x), }
#define KVS_U32FIELD(x) { #x, KVS_UINT32, offsetof(struct test_struct, x), sizeof(((struct test_struct *)0)->x), }
#define KVS_U64FIELD(x) { #x, KVS_UINT32, offsetof(struct test_struct, x), sizeof(((struct test_struct *)0)->x), }
#define KVS_I8FIELD(x) { #x, KVS_INT8, offsetof(struct test_struct, x), sizeof(((struct test_struct *)0)->x), }
#define KVS_I16FIELD(x) { #x, KVS_INT16, offsetof(struct test_struct, x), sizeof(((struct test_struct *)0)->x), }
#define KVS_I32FIELD(x) { #x, KVS_INT32, offsetof(struct test_struct, x), sizeof(((struct test_struct *)0)->x), }
#define KVS_I64FIELD(x) { #x, KVS_INT32, offsetof(struct test_struct, x), sizeof(((struct test_struct *)0)->x), }
#define KVS_FLOATFIELD(x) { #x, KVS_FLOAT, offsetof(struct test_struct, x), sizeof(((struct test_struct *)0)->x), }
#define KVS_DOUBLEFIELD(x) { #x, KVS_DOUBLE, offsetof(struct test_struct, x), sizeof(((struct test_struct *)0)->x), }
#define KVS_STRINGFIELD(x) { #x, KVS_STRING, offsetof(struct test_struct, x), sizeof(((struct test_struct *)0)->x), }

struct key_value_specification test_kvs[] = {
	KVS_U8FIELD(u8),
	KVS_U16FIELD(u16),
	KVS_U32FIELD(u32),
	KVS_U64FIELD(u64),
	KVS_I8FIELD(i8),
	KVS_I16FIELD(i16),
	KVS_I32FIELD(i32),
	KVS_I64FIELD(i64),
	KVS_FLOATFIELD(floatdata),
	KVS_DOUBLEFIELD(doubledata),
	KVS_STRINGFIELD(some_data),
	{ 0, 0, 0, 0 },
};

#define CHECK(condition) \
{ \
	if (!(condition)) { \
		fprintf(stderr, "FAILED: %s:%d %s is false\n", __func__, __LINE__, #condition); \
		return -1; \
	} \
}

int main(int argc, char *argv[])
{
	int rc;
	struct test_struct data;
	const char *lines =
		"u8: 5\n"
		"u16: 10\n"
		"u32: 15\n"
		"u64: 20\n"
		"i8: 45\n"
		"i16: 410\n"
		"i32: 415\n"
		"i64: 420\n"
		"some_data: this is a test\n"
		"doubledata: 3.1415927\n"
		"floatdata: 6.66666";

	memset(&data, 0, sizeof(data));
	rc = key_value_parse_lines(test_kvs, lines, &data);
	CHECK(data.u8 == 5);
	CHECK(data.u16 == 10);
	CHECK(data.u32 == 15);
	CHECK(data.u64 == 20);
	CHECK(data.i8 == 45);
	CHECK(data.i16 == 410);
	CHECK(data.i32 == 415);
	CHECK(data.i64 == 420);
	CHECK(fabs(data.floatdata - 6.66666) < 0.0001);
	CHECK(abs(data.doubledata - 3.1415927) < 0.0001);
	CHECK(strcmp("this is a test", data.some_data) == 0);
	CHECK(rc == 0);
	printf("test_key_value_parser: all tests pass.\n");
	return 0;
}
#endif


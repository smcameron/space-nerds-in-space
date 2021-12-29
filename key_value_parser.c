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
	int address_index;
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

static int parse_string(const char *strvalue, struct key_value_specification *k, void *base_address[])
{
	int i, len;
	for (i = 0; strvalue[i] == ' ' && strvalue[i] != '\n' && strvalue[i] != '\0';)
		i++;
	len = strlen(&strvalue[i]);
	if (len > k->size - 1)
		len = k->size - 1;
	char *str = (char *) base_address[k->address_index] + k->address_offset;
	memset(str, 0, k->size);
	memcpy(str, &strvalue[i], len);
	return 0;
}

#define WRITE_INT(sign, size, format) \
static int write_##sign##size(FILE *f, struct key_value_specification *k, void *base_address[]) \
{ \
	sign ## size ## _t *ptr = (sign ## size ## _t *) ((unsigned char *) base_address[k->address_index] + \
					k->address_offset); \
	fprintf(f, "%s:" format "\n", k->key, *ptr); \
	return 0; \
}

WRITE_INT(uint, 8, "%hhu") /* write_uint8 defined here */
WRITE_INT(uint, 16, "%hu") /* write_uint16 defined here */
WRITE_INT(uint, 32, "%u") /* write_uint32 defined here */
WRITE_INT(int, 8, "%hhd") /* write_uint8 defined here */
WRITE_INT(int, 16, "%hd") /* write_uint16 defined here */
WRITE_INT(int, 32, "%d") /* write_uint32 defined here */

static int write_uint64(FILE *f, struct key_value_specification *k, void *base_address[])
{
	uint64_t *ptr = (uint64_t *) ((unsigned char *) base_address[k->address_index] +
					k->address_offset);
	fprintf(f, "%s:%llu\n", k->key, (unsigned long long) *ptr);
	return 0;
}

static int write_int64(FILE *f, struct key_value_specification *k, void *base_address[])
{
	int64_t *ptr = (int64_t *) ((unsigned char *) base_address[k->address_index] +
					k->address_offset);
	fprintf(f, "%s:%lld\n", k->key, (long long) *ptr);
	return 0;
}

static int write_float(FILE *f, struct key_value_specification *k, void *base_address[])
{
	float *ptr = (float *) ((unsigned char *) base_address[k->address_index] +
					k->address_offset);
	fprintf(f, "%s:%f\n", k->key, *ptr);
	return 0;
}

static int write_double(FILE *f, struct key_value_specification *k, void *base_address[])
{
	double *ptr = (double *) ((unsigned char *) base_address[k->address_index] +
					k->address_offset);
	fprintf(f, "%s:%lf\n", k->key, *ptr);
	return 0;
}

static int write_string(FILE *f, struct key_value_specification *k, void *base_address[])
{
	char tmpstring[1024];
	char *x = &tmpstring[0];
	char *ptr = (char *) ((unsigned char *) base_address[k->address_index] +
					k->address_offset);
	if (k->size >= 1024) {
		x = malloc(k->size + 1);
	}
	memcpy(x, ptr, k->size);
	x[k->size] = '\0';
	fprintf(f, "%s:%s\n", k->key, x);
	if (k->size >= 1024)
		free(x);
	return 0;
}

#define PARSE_INT(sign, size, format) \
static int parse_##sign##size(const char *strvalue, struct key_value_specification *k, void *base_address[]) \
{ \
	int rc; \
	sign ## size ## _t *ptr = (sign ## size ## _t *) ((unsigned char *) base_address[k->address_index] + \
					k->address_offset); \
	rc = sscanf(strvalue, format, ptr); \
	return (rc != 1); \
}

PARSE_INT(uint, 8, "%hhu")	/* parse_uint8 defined here */
PARSE_INT(uint, 16, "%hu")	/* parse_uint16 defined here */
PARSE_INT(uint, 32, "%u")	/* parse_uint32 defined here */
PARSE_INT(int, 8, "%hhd")	/* parse_int8 defined here */
PARSE_INT(int, 16, "%hd")	/* parse_int16 defined here */
PARSE_INT(int, 32, "%d")	/* parse_int32 defined here */

/* Can't use the macros for 64 bit versions because "lld" and "llu" complain about (u)int64_t
 * and I couldn't figure out how to get ("%" PRId64) and ("%" PRIu64) to go through the macro.
 * No worries, they're tiny anyhow.
 */
static int parse_uint64(const char *strvalue, struct key_value_specification *k, void *base_address[])
{
	int rc;
	uint64_t *ptr = (uint64_t *) ((unsigned char *) base_address[k->address_index] + k->address_offset);
	rc = sscanf(strvalue, "%llu", (unsigned long long *) ptr);
	return (rc != 1);
}

static int parse_int64(const char *strvalue, struct key_value_specification *k, void *base_address[])
{
	int rc;
	int64_t *ptr = (int64_t *) ((unsigned char *) base_address[k->address_index] + k->address_offset);
	rc = sscanf(strvalue, "%lld", (signed long long *) ptr);
	return (rc != 1);
}

static int parse_double(const char *strvalue, struct key_value_specification *k, void *base_address[])
{
	int rc;
	double *ptr = (double *) ((unsigned char *) base_address[k->address_index] + k->address_offset);
	rc = sscanf(strvalue, "%lf", ptr);
	return (rc != 1);
}

static int parse_float(const char *strvalue, struct key_value_specification *k, void *base_address[])
{
	int rc;
	float *ptr = (float *) ((unsigned char *) base_address[k->address_index] + k->address_offset);
	rc = sscanf(strvalue, "%f", ptr);
	return (rc != 1);
}

static int key_value_parse_line_with_key(struct key_value_specification *k, char *line, void *base_address[])
{
	char *valuestr;
	int rc, len, klen;

	klen = strlen(k->key);
	len = strlen(line);
	if (len < klen + 1)
		return 1;
	if (strncmp(line, k->key, klen) != 0)
		return 1;
	if (line[klen] != ':')
		return 1;

	/* Skip leading spaces */
	klen = klen + 1;
	while (line[klen] == ' ')
		klen++;
	/* At this point, we have a match on the key */
	valuestr = &line[klen];
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

int key_value_parse_line(const struct key_value_specification *kvs, const char *line, void *base_address[])
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

int key_value_parse_lines(const struct key_value_specification *kvs, const char *lines, void *base_address[])
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

int key_value_write_lines(FILE *f, struct key_value_specification *kvs, void *base_address[])
{
	struct key_value_specification *k;
	int rc;
	int errs = 0;

	for (k = kvs; k->key; k++) {
		switch (k->type) {
		case KVS_STRING:
			rc = write_string(f, k, base_address);
			break;
		case KVS_INT64:
			rc = write_int64(f, k, base_address);
			break;
		case KVS_INT32:
			rc = write_int32(f, k, base_address);
			break;
		case KVS_INT16:
			rc = write_int16(f, k, base_address);
			break;
		case KVS_INT8:
			rc = write_int8(f, k, base_address);
			break;
		case KVS_UINT64:
			rc = write_uint64(f, k, base_address);
			break;
		case KVS_UINT32:
			rc = write_uint32(f, k, base_address);
			break;
		case KVS_UINT16:
			rc = write_uint16(f, k, base_address);
			break;
		case KVS_UINT8:
			rc = write_uint8(f, k, base_address);
			break;
		case KVS_DOUBLE:
			rc = write_double(f, k, base_address);
			break;
		case KVS_FLOAT:
			rc = write_float(f, k, base_address);
			break;
		default:
			fprintf(stderr, "%s:%d: unknown key type '%c' for key '%s'\n",
				__func__, __LINE__, k->type, k->key);
			rc = -1;
			break;
		}
		if (rc)
			errs++;
	}
	return errs;
}

int key_value_get_value(struct key_value_specification *kvs, const char *key, void *base_address[],
			void *output_buffer, int output_buffersize)
{
	struct key_value_specification *k;
	unsigned char *data;
	int amount;

	for (k = kvs; k->key; k++) {
		if (strcmp(key, k->key) != 0)
			continue;
		data = (unsigned char *) base_address[k->address_index] + k->address_offset;
		amount = output_buffersize;
		if (k->size < amount)
			amount = k->size;
		memcpy(output_buffer, data, amount);
		return amount;
	}
	return -1;
}

void key_value_specification_print(struct key_value_specification *kvs)
{
	struct key_value_specification *k;

	printf("%40s %4s %10s %10s %10s\n", "Key", "Type", "offset", "size", "index");
	for (k = kvs; k->key; k++) {
		printf("%40s %4c %10d %10d %10d\n",
			k->key, k->type, k->address_offset, k->size, k->address_index);
	}
}

struct key_value_specification *lookup_key_entry(struct key_value_specification *kvs, const char *key)
{
	struct key_value_specification *k;

	for (k = kvs; k->key; k++) {
		if (strcmp(key, k->key) != 0)
			continue;
		return k;
	}
	return NULL;
}

#ifdef TEST_KEY_VALUE_PARSER
#include <math.h>

struct test_struct {
	uint8_t u8;
	uint16_t u16;
	uint32_t u32;
	uint64_t u64;
	int8_t i8;
	int16_t i16;
	int32_t i32;
	int64_t i64;
	char some_data[64];
	double doubledata;
	float floatdata;
};

struct test_struct2 {
	float xyz;
	double abc[2];
	char blah[100];
	char blah2[100];
};

#define KVS_U8FIELD(x, i, s) { #x, KVS_UINT8, i, offsetof(struct s, x), sizeof(((struct s *)0)->x), }
#define KVS_U16FIELD(x, i, s) { #x, KVS_UINT16, i, offsetof(struct s, x), sizeof(((struct s *)0)->x), }
#define KVS_U32FIELD(x, i, s) { #x, KVS_UINT32, i, offsetof(struct s, x), sizeof(((struct s *)0)->x), }
#define KVS_U64FIELD(x, i, s) { #x, KVS_UINT64, i, offsetof(struct s, x), sizeof(((struct s *)0)->x), }
#define KVS_I8FIELD(x, i, s) { #x, KVS_INT8, i, offsetof(struct s, x), sizeof(((struct s *)0)->x), }
#define KVS_I16FIELD(x, i, s) { #x, KVS_INT16, i, offsetof(struct s, x), sizeof(((struct s *)0)->x), }
#define KVS_I32FIELD(x, i, s) { #x, KVS_INT32, i, offsetof(struct s, x), sizeof(((struct s *)0)->x), }
#define KVS_I64FIELD(x, i, s) { #x, KVS_INT64, i, offsetof(struct s, x), sizeof(((struct s *)0)->x), }
#define KVS_FLOATFIELD(x, i, s) { #x, KVS_FLOAT, i, offsetof(struct s, x), sizeof(((struct s *)0)->x), }
#define KVS_DOUBLEFIELD(x, i, s) { #x, KVS_DOUBLE, i, offsetof(struct s, x), sizeof(((struct s *)0)->x), }
#define KVS_STRINGFIELD(x, i, s) { #x, KVS_STRING, i, offsetof(struct s, x), sizeof(((struct s *)0)->x), }

struct key_value_specification test_kvs[] = {
	KVS_U8FIELD(u8, 0, test_struct),
	KVS_U16FIELD(u16, 0, test_struct),
	KVS_U32FIELD(u32, 0, test_struct),
	KVS_U64FIELD(u64, 0, test_struct),
	KVS_I8FIELD(i8, 0, test_struct),
	KVS_I16FIELD(i16, 0, test_struct),
	KVS_I32FIELD(i32, 0, test_struct),
	KVS_I64FIELD(i64, 0, test_struct),
	KVS_FLOATFIELD(floatdata, 0, test_struct),
	KVS_DOUBLEFIELD(doubledata, 0, test_struct),
	KVS_STRINGFIELD(some_data, 0, test_struct),
	KVS_FLOATFIELD(floatdata, 0, test_struct),
	KVS_FLOATFIELD(xyz, 1, test_struct2),
	KVS_DOUBLEFIELD(abc[0], 1, test_struct2),
	KVS_DOUBLEFIELD(abc[1], 1, test_struct2),
	KVS_STRINGFIELD(blah, 1, test_struct2),
	KVS_STRINGFIELD(blah2, 1, test_struct2),
	{ 0 },
};

#define CHECK(condition) \
{ \
	if (!(condition)) { \
		fprintf(stderr, "FAILED: %s:%d %s is false\n", __func__, __LINE__, #condition); \
		return -1; \
	} \
}

int main(__attribute__((unused)) int argc, __attribute__((unused)) char *argv[])
{
	int rc;
	struct test_struct data;
	struct test_struct2 data2;
	const char *lines =
		"u8: 5\n"
		"u16: 10\n"
		"u32: 15\n"
		"u64: 20\n"
		"i8: 45\n"
		"i16: 410\n"
		"xyz: 1.2345\n"
		"abc[0]: 7.8910\n"
		"abc[1]: 2.222\n"
		"blah: blah blah blah\n"
		"blah2:\n"
		"i32: 415\n"
		"i64: 420\n"
		"some_data: this is a test\n"
		"doubledata: 3.1415927\n"
		"floatdata: 6.66666";

	void *base_address[] = { &data, &data2 };

	memset(&data, 0, sizeof(data));
	rc = key_value_parse_lines(test_kvs, lines, base_address);
	CHECK(data.u8 == 5);
	CHECK(data.u16 == 10);
	CHECK(data.u32 == 15);
	CHECK(data.u64 == 20);
	CHECK(data.i8 == 45);
	CHECK(data.i16 == 410);
	CHECK(data.i32 == 415);
	CHECK(data.i64 == 420);
	CHECK(fabs(data.floatdata - 6.66666) < 0.0001);
	CHECK(fabs(data.doubledata - 3.1415927) < 0.0001);
	CHECK(fabs(data2.xyz - 1.2345) < 0.0001);
	CHECK(fabs(data2.abc[0] - 7.8910) < 0.0001);
	CHECK(fabs(data2.abc[1] - 2.222) < 0.0001);
	CHECK(strcmp("this is a test", data.some_data) == 0);
	CHECK(strcmp("blah blah blah", data2.blah) == 0);
	CHECK(strcmp("", data2.blah2) == 0);
	CHECK(rc == 0);

	printf("---- Begin testing key_value_write_lines\n");
	key_value_write_lines(stdout, test_kvs, base_address);
	printf("---- End testing key_value_write_lines\n");

	printf("test_key_value_parser: all tests pass.\n");
	return 0;
}
#endif


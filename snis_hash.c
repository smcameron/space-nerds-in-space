/*
	Copyright (C) 2015 Stephen M. Cameron
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <assert.h>

#ifndef __APPLE__
#define _GNU_SOURCE
#define __USE_GNU
#include <crypt.h>
#endif

void snis_format_hash(unsigned char *hash, int hashlen, unsigned char *buffer, int len)
{
	int i;
	const char hexdigit[] = "0123456789abcdef";

	for (i = 0; i < hashlen && i * 2 < len; i++) {
		buffer[i * 2] = hexdigit[((hash[i] & 0xf0) >> 4)];
		buffer[i * 2 + 1] = hexdigit[(hash[i] & 0x0f)];
	}
	buffer[i * 2] = '\0';
}

void snis_scan_hash(char *hexhash, int hexhashlen, unsigned char *hash, int hashlen)
{
	int i, j, digit1, digit2;
	const char hexdigit[] = "0123456789abcdef";
	char *p;

	assert(hexhashlen == hashlen * 2);
	assert((hexhashlen % 2) == 0);

	j = 0;
	for (i = 0; i < hexhashlen; i += 2)  {
		p = strchr(hexdigit, hexhash[i]);
		digit1 = (p - hexdigit) * 16;
		p = strchr(hexdigit, hexhash[i + 1]);
		digit2 = (p - hexdigit);
		assert(j < hashlen);
		hash[j] = (unsigned char) (digit1 + digit2);
		j++;
	}
}

/* get_salt() reads saltsize bytes from /dev/random into salt.
 * Returns 0 on success, -1 on failure with errno set.
 */
static int get_salt(unsigned char *salt, int saltsize)
{
	int f, i, rc, bytesleft, offset;
	unsigned char *buffer;
	/* note: we remove . and / from seedchar because we use some of them as dir paths (fixme: should not do that) */
	const char *const seedchar = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
	const int nseedchars = strlen(seedchar);

	f = open("/dev/random", O_RDONLY);
	if (f < 0)
		return f;
	buffer = malloc(saltsize);
	if (!buffer)
		return -1;
	memset(buffer, 0, saltsize);
	bytesleft = saltsize;
	offset = 0;
	do {
		rc = read(f, &buffer[offset], bytesleft);
		if (rc < 0) {
			if (errno == EINTR)
				continue;
			free(buffer);
			return rc;
		}
		bytesleft -= rc;
		offset += rc;
	} while (bytesleft > 0);
	for (i = 0; i < saltsize; i++)
		buffer[i] = seedchar[buffer[i] % nseedchars];
	memcpy(salt, buffer, saltsize);
	free(buffer);
	return 0;
}

/* Note this is not thread safe on __APPLE__, but it is thread safe on linux */
int snis_crypt(unsigned char *shipname, unsigned char *password,
		unsigned char *crypted, int cryptsize, char *insalt, int insaltsize)
{
	unsigned char salt[] = "$1$........";
	unsigned char buffer[50];
#ifndef __APPLE__
	struct crypt_data data;
#endif
	int rc;

	if (insalt) {
		if (insaltsize != 11)
			return -1;
		memcpy(&salt[3], &insalt[3], 8);
	} else {
		rc = get_salt(&salt[3], 8);
		if (rc < 0)
			return rc;
	}

	memset(buffer, 'x', sizeof(buffer));
	if (strlen((char *) password) > 20)
		return -1;
	if (strlen((char *) shipname) > 20)
		return -1;
	buffer[49] = '\0';
	memcpy(buffer, shipname, strlen((char *) shipname));
	memcpy(buffer + 20, password, strlen((char *) password));

#ifndef __APPLE__
	data.initialized = 0;
	password = (unsigned char *) crypt_r((char *) buffer, (char *) salt, &data);
#else
	/* Apple doesn't have crypt_r(), use crypt() instead. */
	password = (unsigned char *) crypt((char *) buffer, (char *) salt);
#endif
	/* We actually want strncpy not strlcpy here */
	strncpy((char *) crypted, (char *) password, cryptsize);
	return 0;
}

#ifdef TEST_SNIS_CRYPT
int main(int argc, char *argv)
{

	char *shipname = "enterprise";
	char *password = "khaaan";
	char crypted[1000], recrypted[1000], formatted[1000];
	int cryptsize = sizeof(crypted);
	int rc;
	char salt[100];

	rc = snis_crypt(shipname, password, crypted, cryptsize, NULL, 0);
	printf("rc = %d\n", rc);
	printf("crypted = '%s' (len = %lu)\n", crypted, strlen(crypted));
	memset(salt, 0, sizeof(salt));
	memcpy(salt, crypted, 11);
	cryptsize = sizeof(recrypted);
	rc = snis_crypt(shipname, password, recrypted, cryptsize, salt, 11);
	printf("rc = %d\n", rc);
	printf("recrypted = '%s' (len = %lu)\n", recrypted, strlen(recrypted));
	if (strcmp(crypted, recrypted) != 0)
		printf("FAIL: encryption came out different with same salt!\n");
	snis_format_hash(crypted, strlen(crypted), formatted, sizeof(formatted));
	printf("formatted = '%s', len = %ld\n", formatted, strlen(formatted));

	return 0;
}
#endif


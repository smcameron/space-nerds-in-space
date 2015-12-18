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
#include <string.h>
#include <openssl/md5.h>
#include <openssl/sha.h>
#include <assert.h>

int snis_md5_hash(unsigned char *shipname, unsigned char *password, unsigned char *md5)
{
	unsigned char *md5tmp, md5buffer[20];

	unsigned char buffer[50];
	memset(buffer, 'x', sizeof(buffer));
	if (strlen((char *) password) > 20)
		return -1;
	if (strlen((char *) shipname) > 20)
		return -1;
	buffer[49] = '\0';
	memcpy(buffer, shipname, strlen((char *) shipname));
	memcpy(buffer + 20, password, strlen((char *) password));

	memset(md5buffer, 0, sizeof(md5buffer));
	md5tmp = MD5(buffer, 50,  md5buffer);

	memcpy(md5, md5tmp, 16);
	return 0;
}

void snis_format_md5_hash(unsigned char *md5, unsigned char *buffer, int len)
{
	snprintf((char *) buffer, len,
		"%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
		md5[0], md5[1], md5[2], md5[3], md5[4], md5[5], md5[6], md5[7], md5[8],
		md5[9], md5[10], md5[11], md5[12], md5[13], md5[14], md5[15]);
}

int snis_sha1_hash(unsigned char *shipname, unsigned char *password, unsigned char *sha1)
{
	unsigned char *sha1tmp, sha1buffer[20];

	unsigned char buffer[50];
	memset(buffer, 'x', sizeof(buffer));
	if (strlen((char *) password) > 20)
		return -1;
	if (strlen((char *) shipname) > 20)
		return -1;
	buffer[49] = '\0';
	memcpy(buffer, shipname, strlen((char *) shipname));
	memcpy(buffer + 20, password, strlen((char *) password));

	memset(sha1buffer, 0, sizeof(sha1buffer));
	sha1tmp = SHA1(buffer, 50,  sha1buffer);

	memcpy(sha1, sha1tmp, 20);
	return 0;
}

void snis_format_sha1_hash(unsigned char *sha1, unsigned char *buffer, int len)
{
	snprintf((char *) buffer, len,
		"%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
		sha1[0], sha1[1], sha1[2], sha1[3], sha1[4], sha1[5], sha1[6], sha1[7], sha1[8],
		sha1[9], sha1[10], sha1[11], sha1[12], sha1[13], sha1[14], sha1[15],
		sha1[16], sha1[17], sha1[18], sha1[19]);
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


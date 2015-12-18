#ifndef SNIS_MD5_HASH_H__
#define SNIS_MD5_HASH_H__
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

void snis_md5_hash(unsigned char *shipname, unsigned char *password, unsigned char *md5);
void snis_format_md5_hash(unsigned char *md5, unsigned char *buffer, int len);

void snis_sha1_hash(unsigned char *shipname, unsigned char *password, unsigned char *sha1);
void snis_format_sha1_hash(unsigned char *md5, unsigned char *buffer, int len);

void snis_scan_hash(char *hexhash, int hexhashlen, unsigned char *hash, int hashlen);

#endif

/*
	Copyright (C) 2025 Stephen M. Cameron
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

/* This program is a supplement to util/snis_update_assets.sh.
 * It does the same thing, but without requiring bash, md5sum, wget, etc.
 * but instead requires libcurl, libcrypt, glibc
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <limits.h>
#include <unistd.h>
#include <curl/curl.h>
#include <openssl/md5.h>

#define SNIS_ASSET_URL "https://spacenerdsinspace.com/snis-assets/"
#define MANIFEST_URL SNIS_ASSET_URL "manifest.txt"
#define P "snis_update_assets"

static int updated_files = 0;
static int new_files = 0;

/* Callback function to write the received data to a file */
static size_t write_data(void *ptr, size_t size, size_t nmemb, FILE *stream)
{
	return fwrite(ptr, size, nmemb, stream);
}

/* Function to fetch a file using libcurl */
static int fetch_file(CURL *curl, const char *url, const char *filename)
{
	CURLcode res;
	FILE *fp;

	printf("Fetching %s\n", url);
	curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
	fp = fopen(filename, "wb");
	if (!fp) {
		fprintf(stderr, "%s: Error opening file %s\n", P, filename);
		return -1;
	}
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
	res = curl_easy_perform(curl);
	if (res != CURLE_OK) {
		fprintf(stderr, "%s: curl_easy_perform() failed: %s\n", P, curl_easy_strerror(res));
	}
	fclose(fp);
	return !(res == CURLE_OK);
}

static char *compute_md5_sum(const char *filename)
{
	FILE *file = fopen(filename, "rb");
	if (!file) {
		return NULL;
	}

	unsigned char buffer[1024];
	unsigned char md5_digest[MD5_DIGEST_LENGTH];
	MD5_CTX context;
	MD5_Init(&context);

	size_t bytes_read;
	while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0) {
		MD5_Update(&context, buffer, bytes_read);
	}

	MD5_Final(md5_digest, &context);
	fclose(file);

	char *hash_str = malloc(MD5_DIGEST_LENGTH * 2 + 1); /* 2 hex digits per byte + null terminator */
	for (int i = 0; i < MD5_DIGEST_LENGTH; i++) {
		snprintf(hash_str + i * 2, 3, "%02x", md5_digest[i]);
	}
	return hash_str;
}

static int fetch_manifest(CURL *curl, char *manifest_url, char **manifest_filename)
{
	/* Generate a temporary filename */
	*manifest_filename = malloc(1024);
	snprintf(*manifest_filename, 1024, "/tmp/snis_tmp_manifest.txt");
	return fetch_file(curl, manifest_url, *manifest_filename);
}

static int make_parent_directories(char *asset_filename)
{
	char *x = strdup(asset_filename);
	int l = strlen(x) + 1;
	char *t, *saveptr;
	char path[PATH_MAX];

	if (strchr(x, '/') == NULL) {
		free(x);
		return 0;
	}
	t = x + l - 1;
	while (*t != '/') {
		*t = '\0';
		t--;
	}
	*t = '\0';
	t = strtok_r(x, "/", &saveptr);
	snprintf(path, PATH_MAX, ".");
	do {
		if (t) {
			strcat(path, "/");
			strcat(path, t);
			/* printf("%s: Creating directory %s\n", P, path); */
			errno = 0;
			int rc = mkdir(path, 0775);
			if (rc && errno == EEXIST)
				continue;
			if (!rc && errno == 0)
				continue;
			fprintf(stderr, "%s: failed to create directory %s: %s\n", P, path, strerror(errno)); {
				free(x);
				return -1;
			}
		}
	} while ((t = strtok_r(NULL, "/", &saveptr)));
	free(x);
	return 0;
}

static int fetch_asset(CURL *curl, char *asset_filename)
{
	char url[PATH_MAX + 1000];
	int rc = make_parent_directories(asset_filename);
	if (rc != 0)
		return rc;
	snprintf(url, sizeof(url), "%s%s", SNIS_ASSET_URL, asset_filename);
	return fetch_file(curl, url, asset_filename);
}

static int process_manifest(CURL *curl, char *manifest_filename)
{
	FILE *f;
	char line[2048];
	int linecount = 0;
	int rc = 0;

	f = fopen(manifest_filename, "r");
	if (!f) {
		fprintf(stderr, "%s: Failed to open %s: %s\n", P, manifest_filename, strerror(errno));
	}

	do {
		char *s = fgets(line, sizeof(line) - 1, f);
		if (!s)
			break;
		linecount++;
		/* if there's a trailing newline, cut it off */
		int l = strlen(s);
		if (s > 0 && s[l - 1] == '\n')
			s[l - 1] = '\0';

		char *md5sum = s;
		char *asset_filename = NULL;
		int found_space = 0;
		for (int i = 0;; i++) {
			if (md5sum[i] == '\0') {
				fprintf(stderr, "%s: bad data at line %d in %s\n", P, linecount, manifest_filename);
				rc = 1;
				goto out;
			}
			if ((md5sum[i] == ' ' || md5sum[i] == '\t') && !found_space) {
				md5sum[i] = '\0';
				found_space = 1;
				continue;
			}
			if (found_space && md5sum[i] != ' ' && md5sum[i] != '\t') {
				asset_filename = &md5sum[i];
				break;
			}
		}
		if (asset_filename) {
			/* See if the file already exists */
			struct stat statbuf;
			int rc = stat(asset_filename, &statbuf);

			if (rc == 0) { /* file exists? */
				char *computed_md5sum = compute_md5_sum(asset_filename);
				if (strcmp(computed_md5sum, md5sum) != 0) {
					if (fetch_asset(curl, asset_filename) == 0)
						updated_files++;
				}
				free(computed_md5sum);
			} else if (errno == ENOENT) { /* file does not exist, fetch it */
				if (fetch_asset(curl, asset_filename) == 0)
					new_files++;
			} else {
				fprintf(stderr, "%s: cannot stat %s: %s\n", P, asset_filename, strerror(errno));
			}
		}
	} while (1);
out:
	fclose(f);
	return rc;
}

int main(int argc, char *argv[])
{
	int rc = 0;
	char *manifest_url = MANIFEST_URL;
	char *manifest_filename;
	CURL *curl;
	char answer[100];

	printf("WARNING!  This program is experimental!  Are you sure you wish to proceeed (y/n)? ");
	memset(answer, 0, sizeof(answer));
	char *a = fgets(answer, sizeof(answer), stdin);
	if (strncmp(answer, "y\n", 3) != 0)
		exit(1);

	/* Set up curl */
	curl_global_init(CURL_GLOBAL_ALL);
	curl = curl_easy_init();
	if (!curl) {
		fprintf(stderr, "%s: curl_easy_init failed.\n", P);
		goto out;
	}

	rc = fetch_manifest(curl, manifest_url, &manifest_filename);
	if (rc) {
		fprintf(stderr, "%s: Failed to fetch manifest from %s\n", P, manifest_url);
		return -1;
	}
	process_manifest(curl, manifest_filename);
	free(manifest_filename);
	curl_easy_cleanup(curl);
out:
	curl_global_cleanup();
	printf("Updated files: %d\nNew files: %d\n", updated_files, new_files);
	return rc;
}

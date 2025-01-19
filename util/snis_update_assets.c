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
#include <fcntl.h>
#include <limits.h>
#include <unistd.h>
#include <curl/curl.h>
#include <openssl/md5.h>
#include <getopt.h>
#include <dirent.h>

#include "../string-utils.h"

#define SNIS_ASSET_URL "https://spacenerdsinspace.com/snis-assets/"
#define MANIFEST_URL SNIS_ASSET_URL "manifest.txt"
#define P "snis_update_assets"

static struct option long_options[] = {
	{"dry-run", no_argument, 0, 'd' },
	{"destdir", required_argument, 0, 'D' },
	{"srcdir", required_argument, 0, 's' },
	{"force", no_argument, 0, 'f' },
	{0, 0, 0, 0 },
};

static int dry_run = 0;
static char *destdir = NULL;
static char *srcdir = NULL;
static char orig_cwd[PATH_MAX * 2] = { 0 };
static int force_option = 0;

static int updated_files = 0;
static int new_files = 0;

static struct directory_list {
	char **name;
	int nnames;
	int nslots;
} dir_list = { NULL, 0, 0 };

static int lookup_dir_name(struct directory_list *dir_list, char *name)
{
	for (int i = 0; i < dir_list->nnames; i++)
		if (strcmp(dir_list->name[i], name) == 0)
			return i;
	return -1;
}

static int remember_dir_name(struct directory_list *dir_list, char *name)
{
	/* Check if we already know it so we don't store it again. */
	int rc = lookup_dir_name(dir_list, name);
	if (rc >= 0)
		return 0;

	/* Make space for the new name if necessary */
	if (dir_list->nnames >= dir_list->nslots) {
		int new_nslots;
		if (dir_list->nslots == 0)
			new_nslots = 10;
		else
			new_nslots = dir_list->nslots * 2;
		char **new_name = realloc(dir_list->name, new_nslots * sizeof(*new_name));
		if (new_name) {
			dir_list->name = new_name;
			dir_list->nslots = new_nslots;
		} else {
			return -1;
		}
	}
	dir_list->name[dir_list->nnames] = strdup(name);
	dir_list->nnames++;
	return 0;
}

static void free_directory_list(struct directory_list *dir_list)
{
	if (!dir_list->name)
		return;
	for (int i = 0; i < dir_list->nnames; i++) {
		free(dir_list->name[i]);
		dir_list->name[i] = NULL;
	}
	free(dir_list->name);
	dir_list->name = NULL;
	dir_list->nslots = 0;
	dir_list->nnames = 0;
}

/* Callback function to write the received data to a file */
static size_t write_data(void *ptr, size_t size, size_t nmemb, FILE *stream)
{
	return fwrite(ptr, size, nmemb, stream);
}

/* Function to fetch a file using libcurl */
static int fetch_file_helper(CURL *curl, const char *url, const char *filename, int honor_dry_run)
{
	CURLcode res;
	FILE *fp;

	if (dry_run && honor_dry_run) {
		printf("Would fetch %s\n", url);
		return 0;
	}
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

static int fetch_file(CURL *curl, const char *url, const char *filename)
{
	fetch_file_helper(curl, url, filename, 1);
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
	*manifest_filename = malloc(PATH_MAX);
	snprintf(*manifest_filename, PATH_MAX, "/tmp/snis_tmp_manifest.txt");

	/* Actually fetch the manifest ignoring --dry-run flag. */
	int rc = fetch_file_helper(curl, manifest_url, *manifest_filename, 0);
	if (rc)
		free(*manifest_filename);
	return rc;
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

			/* Check if we already created this directory */
			int dirnum = lookup_dir_name(&dir_list, path);
			if (dirnum >= 0)
				continue; /* we did */

			if (dry_run) {
				printf("%s: Would create directory %s\n", P, path);
				(void) remember_dir_name(&dir_list, path); /* so we don't keep trying to create it */
				continue;
			}
			errno = 0;
			int rc = mkdir(path, 0775);
			if (rc && errno == EEXIST) {
				(void) remember_dir_name(&dir_list, path); /* so we don't keep trying to create it */
				continue;
			}
			if (!rc && errno == 0) {
				printf("%s: Created directory %s\n", P, path);
				(void) remember_dir_name(&dir_list, path); /* so we don't keep trying to create it */
				continue;
			}
			fprintf(stderr, "%s: failed to create directory %s: %s\n", P, path, strerror(errno)); {
				free(x);
				return -1;
			}
		}
	} while ((t = strtok_r(NULL, "/", &saveptr)));
	free(x);
	return 0;
}

static int copy_file(char *src, char *dest)
{
	int fsize;

	printf("Copying %s to %s\n", src, dest);
	if (dry_run)
		return 0;
	char *buffer = slurp_file(src, &fsize);
	if (!buffer) {
		fprintf(stderr, "%s: failed to read file %s\n", P, src);
		return -1;
	}

	int bytes_to_write = fsize;
	int bytes_written = 0;

	int fd = open(dest, O_TRUNC | O_WRONLY | O_CREAT, 0644);
	do {

		int rc = write(fd, &buffer[bytes_written], bytes_to_write);
		if (rc >= 0) {
			bytes_written += rc;
			bytes_to_write -= rc;
			if (bytes_to_write == 0)
				break;
		}
		if (rc < 0) {
			fprintf(stderr, "%s: failed to write to %s: %s\n", P, dest, strerror(errno));
			close(fd);
			return -1;
		}
	} while (1);
	close(fd);
	return 0;
}

static int fetch_asset(CURL *curl, char *asset_filename)
{
	char url[PATH_MAX + 1000];
	int rc = make_parent_directories(asset_filename);
	if (rc != 0)
		return rc;
	if (!srcdir) {
		snprintf(url, sizeof(url), "%s%s", SNIS_ASSET_URL, asset_filename);
		return fetch_file(curl, url, asset_filename);
	} else {
		/* fprintf(stderr, "Copy file from %s/%s to %s\n", orig_cwd, asset_filename, asset_filename); */
		int srclen = strlen(orig_cwd) + 1 + strlen(asset_filename) + 1;
		char *src = malloc(srclen);
		snprintf(src, srclen, "%s/%s", orig_cwd, asset_filename);
		rc = copy_file(src, asset_filename);
		free(src);
		return rc;
	}
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
		return -1;
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

static void usage(void)
{
	fprintf(stderr, "\n%s: usage:\n", P);
	fprintf(stderr, "%s [ --force ] [ --dry-run ] [ --srcdir dir ] --destdir dir\n\n", P);
	exit(1);
}

static void process_cmdline_options(int argc, char *argv[])
{
	int c, option_index;
	char cwd[PATH_MAX];

	while (1) {
		option_index = 0;

		c = getopt_long(argc, argv, "D:f", long_options, &option_index);
		if (c == -1)
			break;
		switch (c) {
		case 'd': /* dry run */
			dry_run = 1;
			break;
		case 'D':
			destdir = strdup(optarg);
			break;
		case 's':
			srcdir = strdup(optarg);
			if (!getcwd(cwd, PATH_MAX)) {
				fprintf(stderr, "%s: Cannot get current directory: %s\n", P, strerror(errno));
				exit(1);
			}
			snprintf(orig_cwd, PATH_MAX, "%s", cwd);
			break;
		case 'f':
			force_option = 1;
			break;
		default:
			break;
		}
	}
	if (!destdir) {
		fprintf(stderr, "%s: required destination directory not specified.\n", P);
		usage();
	}

}

static char *filenames_to_ignore[] = {
	".",
	"..",
	".gitignore",
	".git"
};

static int file_should_be_ignored(char *filename)
{
	int n = sizeof(filenames_to_ignore) / sizeof(filenames_to_ignore[0]);
	for (int i = 0; i < n; i++) {
		if (strcmp(filename, filenames_to_ignore[i]) == 0)
			return 1;
	}
	return 0;
}

static int recursively_build_local_manifest(FILE *f, char *srcdir)
{
	char dir[PATH_MAX];
	struct dirent **namelist;
	int rc, n;

	rc = 0;
	/* fprintf(stderr, "Executing scandir on %s\n", srcdir); */
	errno = 0;
	n = scandir(srcdir, &namelist, NULL, alphasort);
	if (n == -1) {
		fprintf(stderr, "%s: scandir %s failed: %s\n", P, srcdir, strerror(errno));
		return -1;
	}

	for (int i = 0; i < n; i++) {
		struct stat statbuf;
		char path[PATH_MAX];
		if (file_should_be_ignored(namelist[i]->d_name))
			continue;
		snprintf(path, PATH_MAX, "%s/%s", srcdir, namelist[i]->d_name);
		errno = 0;
		rc = stat(path, &statbuf);
		if (rc != 0) {
			fprintf(stderr, "%s: failed to stat %s: %s\n", P, namelist[i]->d_name, strerror(errno));
			goto out;
		}

		if ((statbuf.st_mode & S_IFMT) == S_IFREG) { /* Regular file? */
			snprintf(dir, PATH_MAX, "%s/%s", srcdir, namelist[i]->d_name);
			char *md5sum = compute_md5_sum(dir);
			/* fprintf(stderr, "%s  %s\n", md5sum, dir); */
			fprintf(f, "%s  %s\n", md5sum, dir);
		} else if ((statbuf.st_mode & S_IFMT) == S_IFDIR) { /* Directory? */
			snprintf(dir, PATH_MAX, "%s/%s", srcdir, namelist[i]->d_name);
			rc = recursively_build_local_manifest(f, dir);
			if (rc)
				goto out;
		} /* Ignore everything except directories and regular files */
	}

out:
	for (int i = 0; i < n; i++) {
		free(namelist[i]);
	}
	free(namelist);
	return rc;
}

static int build_local_manifest(char *srcdir, char *manifest_filename)
{
	FILE *f = fopen(manifest_filename, "w+");
	if (!f) {
		fprintf(stderr, "%s: Failed to open %s for write\n", P, manifest_filename);
		return -1;
	}

	int rc = recursively_build_local_manifest(f, srcdir);
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

	process_cmdline_options(argc, argv);

	if (!force_option) {
		printf("Asset directory is %s\n", destdir);
		printf("Are you sure you wish to proceeed with setting up assets (y/n)? ");
		memset(answer, 0, sizeof(answer));
		char *a = fgets(answer, sizeof(answer), stdin);
		if (!a || strncmp(answer, "y\n", 3) != 0)
			exit(1);
	}

	/* Set up curl */
	curl_global_init(CURL_GLOBAL_ALL);
	curl = curl_easy_init();
	if (!curl) {
		fprintf(stderr, "%s: curl_easy_init failed.\n", P);
		goto out;
	}

	if (!srcdir) {
		rc = fetch_manifest(curl, manifest_url, &manifest_filename);
		if (rc) {
			fprintf(stderr, "%s: Failed to fetch manifest from %s\n", P, manifest_url);
			goto out1;
		}
	} else {
		manifest_filename = strdup("/tmp/snis-asset-local-manifest.txt");
		rc = build_local_manifest(srcdir, manifest_filename);
	}

	if (destdir) {
		errno = 0;
		rc = chdir(destdir);
		if (rc != 0) {
			fprintf(stderr, "%s: failed to chdir to %s\n", P, destdir);
			exit(1);
		}
		fprintf(stderr, "Changed current directory to %s\n", destdir);
	}

	process_manifest(curl, manifest_filename);
	free(manifest_filename);
out1:
	curl_easy_cleanup(curl);
out:
	curl_global_cleanup();
	printf("%s files: %d\n%s files: %d\n",
			dry_run ? "Would have updated" : "Updated", updated_files,
			dry_run ? "Would have created new" : "New", new_files);
	free_directory_list(&dir_list);
	return rc;
}

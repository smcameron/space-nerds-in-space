#define _GNU_SOURCE
#include <sys/mman.h>
#include <assert.h>

#define FUZZ_TESTING 1
#include "util/snis_update_assets.c"

CURLcode curl_easy_setopt(CURL *curl, CURLoption option, ...) { return 0; }
CURLcode curl_easy_perform(CURL *curl) { return 0; }

__AFL_FUZZ_INIT();


int main(int argc, char *argv[])
{
	dry_run = 1;

	__AFL_INIT();
	int fd = memfd_create("fuzz", 0);
	assert(fd == 3);
	unsigned char *buf = __AFL_FUZZ_TESTCASE_BUF;
	while (__AFL_LOOP(10000)) {
		int len = __AFL_FUZZ_TESTCASE_LEN;
		ftruncate(fd, 0);
		pwrite(fd, buf, len, 0);
		process_manifest(NULL, "/proc/self/fd/3");
	}
}

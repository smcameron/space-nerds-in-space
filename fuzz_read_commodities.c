
#define _GNU_SOURCE         /* See feature_test_macros(7) */
#include <sys/mman.h>
#include <assert.h>

#include "string-utils.c"
#include "commodities.c"

#define ARRAYSIZE(x) (sizeof(x) / sizeof((x)[0]))

__AFL_FUZZ_INIT();

int main(int argc, char *argv[])
{
	struct commodity *c = NULL;
	int ncommodities = 0;
	/* __AFL_INIT(); */
	int fd = memfd_create("fuzz", 0);
	assert(fd == 3);
	unsigned char *buf = __AFL_FUZZ_TESTCASE_BUF;
	while (__AFL_LOOP(10000)) {
		int len = __AFL_FUZZ_TESTCASE_LEN;
		int count;
		ftruncate(fd, 0);
		pwrite(fd, buf, len, 0);
		c = read_commodities("/proc/self/fd/3", &ncommodities);
		free(c);
		c = NULL;
	}
}

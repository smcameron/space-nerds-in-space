
#define _GNU_SOURCE         /* See feature_test_macros(7) */
#include <sys/mman.h>
#include <assert.h>

#include "string-utils.c"
#include "mtwist.c"
#include "mathutils.c"
#include "quat.c"
#include "docking_port.c"

#define ARRAYSIZE(x) (sizeof(x) / sizeof((x)[0]))

__AFL_FUZZ_INIT();

int main(int argc, char *argv[])
{
	struct docking_port_attachment_point *dpap = NULL;
	/* __AFL_INIT(); */
	int fd = memfd_create("fuzz", 0);
	assert(fd == 3);
	unsigned char *buf = __AFL_FUZZ_TESTCASE_BUF;
	while (__AFL_LOOP(10000)) {
		int len = __AFL_FUZZ_TESTCASE_LEN;
		int count;
		ftruncate(fd, 0);
		pwrite(fd, buf, len, 0);
		dpap = read_docking_port_attachments("/proc/self/fd/3", 1.0);
		free(dpap);
		dpap = NULL;
	}
}

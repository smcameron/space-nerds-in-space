
#define _GNU_SOURCE         /* See feature_test_macros(7) */
#include <sys/mman.h>
#include <assert.h>

#include "string-utils.c"
#include "starbase_metadata.c"

struct docking_port_attachment_point;

struct docking_port_attachment_point *read_docking_port_attachments(
			__attribute__((unused)) char *filename,
			__attribute__((unused)) float scaling_factor)
{
	return NULL;
}


#define ARRAYSIZE(x) (sizeof(x) / sizeof((x)[0]))

__AFL_FUZZ_INIT();

int main(int argc, char *argv[])
{
	struct starbase_file_metadata *starbase_metadata = NULL;
	int nstarbase_models = 0;
	/* __AFL_INIT(); */
	int fd = memfd_create("fuzz", 0);
	assert(fd == 3);
	unsigned char *buf = __AFL_FUZZ_TESTCASE_BUF;
	while (__AFL_LOOP(10000)) {
		int len = __AFL_FUZZ_TESTCASE_LEN;
		int count;
		ftruncate(fd, 0);
		pwrite(fd, buf, len, 0);

		int rc = read_starbase_model_metadata("/proc/self/fd", "3", &nstarbase_models, &starbase_metadata);
	}
}

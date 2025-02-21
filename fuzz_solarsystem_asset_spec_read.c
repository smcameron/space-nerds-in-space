#define _GNU_SOURCE
#define dist3dsqrd _dist3dsqrd
#  include "mathutils.c"
#undef  dist3dsqrd
#include "solarsystem_config.c"
#include "matrix.c"
#include "mesh.c"
#include "mtwist.c"
#include "quat.c"
#include "string-utils.c"
#include <assert.h>
#include <sys/mman.h>
#include <unistd.h>

tbool genTangSpaceDefault(const SMikkTSpaceContext *x) { assert(0); }
OSNFLOAT open_simplex_noise3(const struct osn_context *x, OSNFLOAT y, OSNFLOAT z, OSNFLOAT a) { assert(0); }
void stacktrace(char *x) {}
void mesh_graph_dev_init(struct mesh *x) {}
void mesh_graph_dev_cleanup(struct mesh *x) {}
unsigned graph_dev_load_texture(const char *x, int y) { return 1; }
void material_init_texture_mapped(struct material *x) {}

__AFL_FUZZ_INIT();


int main(int argc, char *argv[])
{
	__AFL_INIT();
	int fd = memfd_create("fuzz", 0);
	assert(fd == 3);
	unsigned char *buf = __AFL_FUZZ_TESTCASE_BUF;
	while (__AFL_LOOP(10000)) {
		int len = __AFL_FUZZ_TESTCASE_LEN;
		ftruncate(fd, 0);
		pwrite(fd, buf, len, 0);
		solarsystem_asset_spec_read("/proc/self/fd/3");
	}
}

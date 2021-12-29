#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "mtwist.h"
#include "quat.h"
#include "matrix.h"
#include "mathutils.h"
#include "mesh.h"
#include "stl_parser.h"

unsigned int graph_dev_load_texture(const char *filename)
{
	return 0;
}

void material_init_texture_mapped(struct material *m)
{
}

void mesh_graph_dev_init(struct mesh *m)
{
}

void mesh_graph_dev_cleanup(struct mesh *m)
{
}

static void usage(void)
{
	fprintf(stderr, "usage: test-obj-parser somefile.obj\n");
	exit(1);
}

int main(int argc, char *argv[])
{
	struct mesh *m;

	if (argc < 2)
		usage();


	m = read_obj_file(argv[1]);
	printf("m = %p\n", m);
	if (!m) {
		fprintf(stderr, "errno = %d: %s\n", errno, strerror(errno));
	}

	return 0;
}


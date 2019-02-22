#include <stdio.h>

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

int main(int argc, char *argv[])
{
	struct mesh *m;

	m = read_obj_file(argv[1]);
	printf("m = %p\n", m);

	return 0;
}


#ifndef DOCKING_PORT_H__
#define DOCKING_PORT_H__

#include "quat.h"

#define MAX_DOCKING_PORTS 4

struct docking_port_attachment_point {
	int nports;
	int docking_port_model;
#define DOCKING_PORT_INVISIBLE_MODEL 2
	struct docking_port {
		float scale;
		union vec3 pos;
		union quat orientation;
	} port[MAX_DOCKING_PORTS];
};

struct docking_port_attachment_point *read_docking_port_attachments(char *filename, float scaling_factor);

#endif

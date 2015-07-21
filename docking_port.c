#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#include "docking_port.h"

struct docking_port_attachment_point *read_docking_port_attachments(char *filename,
				float scaling_factor)
{
	FILE *f;
	struct docking_port_attachment_point *answer = NULL;
	int rc, n = 0;
	int line = 1;
	int model;
	float scale, x, y, z, rx, ry, rz, ra;
	char buffer[256];

	f = fopen(filename, "r");
	if (!f) {
		fprintf(stderr, "Cannot read file %s: %s\n", filename, strerror(errno));
		return NULL;
	}
	answer = malloc(sizeof(*answer));
	if (!answer)
		goto error_out;
	memset(answer, 0, sizeof(*answer));

	/* FIXME: This parser is not lenient enough in what it accepts */

	if (!fgets(buffer, sizeof(buffer) - 1, f)) {
		fprintf(stderr, "Error reading docking port attachment file %s: %s\n",
				filename, strerror(errno));
		fprintf(stderr, "Last line successfully read was %d\n", line);
		free(answer);
		answer = NULL;
		goto error_out;
	}

	rc = sscanf(buffer, "%*[ {	]%d", &n);
	if (rc != 1) {
		fprintf(stderr, "Error reading docking port attachment file %s, line %d\n",
				filename, line);
		goto error_out;
	}
	if (n < 0 || n > MAX_DOCKING_PORTS) {
		fprintf(stderr, "Invalid number of ports in docking port attachment file %s:%d %d (0-5)\n",
				filename, line, n);
		goto error_out;
	}
	for (int i = 0; i < n; i++) {
		if (!fgets(buffer, sizeof(buffer) - 1, f)) {
			fprintf(stderr, "Error reading docking port attachment file %s: %s\n",
					filename, strerror(errno));
			fprintf(stderr, "Last line successfully read was %d\n", line);
			goto error_out;
		}
		line++;
		rc = sscanf(buffer,
"%*[ {	]%d%*[, ]%f%*[, {]%f%*[, ]%f%*[, ]%f%*[} ,{]%f%*[, ]%f%*[ ,]%f%*[, ]%f",
			&model, &scale, &x, &y, &z, &rx, &ry, &rz, &ra);
		if (rc != 9) {
			fprintf(stderr,
				"Invalid model, scale, x, y, z, rx, ry, rz, ra at %s:%d, %d items read successfully.\n",
				filename, line, rc);
			fprintf(stderr, "line was: '%s'\n", buffer);
			fprintf(stderr, "(sorry this file format is so persnickety)\n");
		}
		answer->docking_port_model = model;
		answer->port[i].scale = scale * scaling_factor;
		vec3_init(&answer->port[i].pos, x, y, z);
		vec3_mul_self(&answer->port[i].pos, scaling_factor);
		quat_init_axis(&answer->port[i].orientation, rx, ry, rz, ra * M_PI / 180.0);
	}
	answer->nports = n;
	return answer;

error_out:
	if (answer)
		free(answer);
	fclose(f);
	return NULL;
}


#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "snis_dmx.h"

void usage(void)
{
	fprintf(stderr, "usage: test_snis_dmx <device>\n");
	fprintf(stderr, "       Where device is a serial port connected to a DMX device chain.\n");
	fprintf(stderr, "       If everything works correctly, all the lights in the chain should\n");
	fprintf(stderr, "       blink once every two seconds twenty times.\n");
}

int main(int argc, char *argv[])
{
	int i, j, dmx;

	if (argc != 2) {
		usage();
		return -1;
	}
	dmx = snis_dmx_start_main_thread(argv[1]);
	if (dmx < 0) {
		fprintf(stderr, "Failed to start DMX thread on device '%s', %s\n",
			argv[1], errno ? strerror(errno) : "Unknown error");
		return 1;
	}
	for (i = 0; i < 512; i++)
		snis_dmx_add_light(dmx, 1);

	for (i = 0; i < 20; i++) {
		/* Turn on every possible light */
		for (j = 0; j < 512; j++)
			snis_dmx_set_u8_level(dmx, j, 255);
		sleep(1);
		/* Turn off every possible light */
		for (j = 0; j < 512; j++)
			snis_dmx_set_u8_level(dmx, j, 0);
		sleep(1);
	}
	snis_dmx_close_device(dmx);
	return 0;
}

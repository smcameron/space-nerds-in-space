#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>

#include "wwviaudio.h"

int main(int argc, char *argv[])
{
	int rc;
	char *sound_device_str;
	int sound_device = -1;
	char *sound_file = "share/snis/sounds/red-alert.ogg";

	printf("Space Nerds In Space audio test program.\n");
	if (argc > 1) {
		sound_device_str = argv[1];
		rc = sscanf(sound_device_str, "%d", &sound_device);
		if (rc != 1) {
			sound_device = -1;
		}
	}

	if (sound_device >= 0) {
		printf("Manually setting sound device to %d\n", sound_device);
		wwviaudio_set_sound_device(sound_device);
	}
	rc = wwviaudio_initialize_portaudio(10, 10);
	printf("wwviaudio_initialize_portaudio returned %d\n", rc);
	if (rc != 0) {
		return rc;
	}

	rc = wwviaudio_read_ogg_clip(0, sound_file);
	if (rc != 0) {
		printf("Failed to read %s\n", sound_file);
		exit(1);
	}

	wwviaudio_list_devices();

	printf("Attempting to play sound... ");
	wwviaudio_add_sound(0);
	sleep(1);
	printf("...finished attempting to play sound\n");
	return 0;
}


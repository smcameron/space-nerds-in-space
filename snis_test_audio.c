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
	printf("How to use this program:\n");
	printf("Type the following:\n");
	printf("  make snis_test_audio\n");
	printf("  ./snis_test_audio\n\n");
	printf("Or, if you want to try a sound device other than the default, such as device 6:\n\n");
	printf("  ./snis_test_audio 6\n\n");
	printf("The device numbers should be listed below.\n\n\n");

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
	sound_device = wwviaudio_get_sound_device();

	rc = wwviaudio_read_ogg_clip(0, sound_file);
	if (rc != 0) {
		printf("Failed to read %s\n", sound_file);
		exit(1);
	}

	wwviaudio_list_devices();

	printf("Attempting to play sound '%s' to device %d... ", sound_file, sound_device);
	fflush(stdout);
	wwviaudio_add_sound(0);
	sleep(1);
	printf("...finished attempting to play sound\n");
	return 0;
}


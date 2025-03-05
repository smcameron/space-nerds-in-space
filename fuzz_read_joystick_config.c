
#define _GNU_SOURCE         /* See feature_test_macros(7) */
#include <sys/mman.h>
#include <assert.h>

#define DO_AFL_TEST 1

#include "string-utils.c"
#include "joystick_config.c"

#define ARRAYSIZE(x) (sizeof(x) / sizeof((x)[0]))

static void dummy_button_fn(__attribute__((unused)) void *x) { }
static void dummy_axis_fn(__attribute__((unused)) void *x, int v) { }

static char *joysticks[] = {
	"device:usb-©Microsoft_Corporation_Controller_05EB4AD-joystick",
	"device:usb-Thrustmaster_TWCS_Throttle-joystick",
	"device:usb-Thrustmaster_T.16000M-joystick",
	"device:usb-Thrustmaster_T.Flight_Stick_X-joystick",
	"device:usb-Thrustmaster_Throttle_-_HOTAS_Warthog-joystick",
	"device:usb-Thustmaster_Joystick_-_HOTAS_Warthog-joystick",
	"device:usb-Logitech_Logitech_RumblePad_2_USB-joystick",
	"device:usb-Saitek_Cyborg_X-joystick",
	"device:usb-0079_Generic_USB_Joystick-joystick",
	"device:usb-Saitek_Saitek_X52_Flight_Control_System-joystick",
	"device:usb-Logitech_Logitech_Attack_3-joystick",
};

#ifdef DO_AFL_TEST
__AFL_FUZZ_INIT();
#endif

static void setup_for_parsing(struct joystick_config *joystick_cfg)
{
	set_joystick_axis_fn(joystick_cfg, "yaw", dummy_axis_fn);
	set_joystick_axis_fn(joystick_cfg, "roll", dummy_axis_fn);
	set_joystick_axis_fn(joystick_cfg, "pitch", dummy_axis_fn);
	set_joystick_button_fn(joystick_cfg, "phaser", dummy_button_fn);
	set_joystick_button_fn(joystick_cfg, "torpedo", dummy_button_fn);
	set_joystick_button_fn(joystick_cfg, "missile", dummy_button_fn);
	set_joystick_axis_fn(joystick_cfg, "weapons-yaw", dummy_axis_fn);
	set_joystick_axis_fn(joystick_cfg, "weapons-pitch", dummy_axis_fn);
	set_joystick_axis_fn(joystick_cfg, "damcon-pitch", dummy_axis_fn);
	set_joystick_axis_fn(joystick_cfg, "damcon-roll", dummy_axis_fn);
	set_joystick_axis_fn(joystick_cfg, "throttle", dummy_axis_fn);
	set_joystick_axis_fn(joystick_cfg, "warp", dummy_axis_fn);
	set_joystick_axis_fn(joystick_cfg, "weapons-wavelength", dummy_axis_fn);
	set_joystick_button_fn(joystick_cfg, "damcon-gripper", dummy_button_fn);
	set_joystick_button_fn(joystick_cfg, "nav-engage-warp", dummy_button_fn);
	set_joystick_button_fn(joystick_cfg, "nav-standard-orbit", dummy_button_fn);
	set_joystick_button_fn(joystick_cfg, "nav-docking-magnets", dummy_button_fn);
	set_joystick_button_fn(joystick_cfg, "nav-attitude-indicator-abs-rel", dummy_button_fn);
	set_joystick_button_fn(joystick_cfg, "nav-starmap", dummy_button_fn);
	set_joystick_button_fn(joystick_cfg, "nav-reverse", dummy_button_fn);
	set_joystick_button_fn(joystick_cfg, "nav-lights", dummy_button_fn);
	set_joystick_button_fn(joystick_cfg, "nav-nudge-warp-up", dummy_button_fn);
	set_joystick_button_fn(joystick_cfg, "nav-nudge-warp-down", dummy_button_fn);
	set_joystick_button_fn(joystick_cfg, "nav-nudge-zoom-up", dummy_button_fn);
	set_joystick_button_fn(joystick_cfg, "nav-nudge-zoom-down", dummy_button_fn);
	set_joystick_button_fn(joystick_cfg, "weapons-wavelength-up", dummy_button_fn);
	set_joystick_button_fn(joystick_cfg, "weapons-wavelength-down", dummy_button_fn);
	set_joystick_button_fn(joystick_cfg, "nav-change-pov", dummy_button_fn);
	set_joystick_button_fn(joystick_cfg, "eng-preset-1", dummy_button_fn);
	set_joystick_button_fn(joystick_cfg, "eng-preset-2", dummy_button_fn);
	set_joystick_button_fn(joystick_cfg, "eng-preset-3", dummy_button_fn);
	set_joystick_button_fn(joystick_cfg, "eng-preset-4", dummy_button_fn);
	set_joystick_button_fn(joystick_cfg, "eng-preset-5", dummy_button_fn);
	set_joystick_button_fn(joystick_cfg, "eng-preset-6", dummy_button_fn);
	set_joystick_button_fn(joystick_cfg, "eng-preset-7", dummy_button_fn);
	set_joystick_button_fn(joystick_cfg, "eng-preset-8", dummy_button_fn);
	set_joystick_button_fn(joystick_cfg, "eng-preset-9", dummy_button_fn);
	set_joystick_button_fn(joystick_cfg, "eng-preset-10", dummy_button_fn);
	set_joystick_button_fn(joystick_cfg, "eng-shield-toggle", dummy_button_fn);
	set_joystick_button_fn(joystick_cfg, "left-arrow", dummy_button_fn);
	set_joystick_button_fn(joystick_cfg, "right-arrow", dummy_button_fn);
	set_joystick_button_fn(joystick_cfg, "up-arrow", dummy_button_fn);
	set_joystick_button_fn(joystick_cfg, "down-arrow", dummy_button_fn);
}

int main(int argc, char *argv[])
{
	struct joystick_config *joystick_cfg;

#ifdef DO_AFL_TEST
	/* __AFL_INIT(); */
	int fd = memfd_create("fuzz", 0);
	assert(fd == 3);
	unsigned char *buf = __AFL_FUZZ_TESTCASE_BUF;
	while (__AFL_LOOP(10000)) {
		int len = __AFL_FUZZ_TESTCASE_LEN;
		int count;
		ftruncate(fd, 0);
		pwrite(fd, buf, len, 0);
		joystick_cfg = new_joystick_config();
		setup_for_parsing(joystick_cfg);
		read_joystick_config(joystick_cfg, "/proc/self/fd/3", joysticks, 1);
					/* ARRAYSIZE(joysticks)); TODO why this breaks? */
		free_joystick_config(joystick_cfg);
	}
#else
		joystick_cfg = new_joystick_config();
		setup_for_parsing(joystick_cfg);
		read_joystick_config(joystick_cfg, "fuzztests/read_joystick_config/joystick_config.txt",
					joysticks, ARRAYSIZE(joysticks));
		free_joystick_config(joystick_cfg);
#endif

}

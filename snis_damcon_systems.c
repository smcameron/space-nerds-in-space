#include <stdint.h>

#define DEFINE_SNIS_DAMCON_SYSTEMS_GLOBALS
#include "snis_damcon_systems.h"
#undef DEFINE_SNIS_DAMCON_SYSTEMS_GLOBALS

#define ARRAYSIZE(x) (sizeof(x) / sizeof(x[0]))

static char *damcon_system_names[] = {
	"SHIELD SYSTEM",
	"IMPULSE DRIVE",
	"WARP DRIVE",
	"MANEUVERING",
	"PHASER BANKS",
	"SENSORS",
	"COMMUNICATIONS",
	"TRACTOR BEAM",
	"REPAIR STATION",
};

static char *damcon_part_names[][PARTS_PER_DAMCON_SYSTEM] = {
	{	/* shield system parts */
		"Von Kurnatowski field generator",
		"furion reflector matrix",
		"photonic charge stabilizer",
		"phase conjugate baryon reflector",
		"Tachyonic energy absorbtion matrix", 
	},
	{	/* impulse drive parts */
		"positron emitter tube",
		"plasma current regulator",
		"Wilhelm junction",
		"antimatter backflash suppressor",
		"positronic charging coil",
	},
	{	/* warp drive parts */
		"relativistic torque amplifier",
		"transient wormhole suppressor",
		"plasma wastegate controller",
		"Hibbert space multiplexer",
		"Sagan conundrum resolver",
	},
	{	/* maneuvering parts */
		"inertial navigation module",
		"thruster control module",
		"Hibbert space transfer computer",
		"star tracking module",
		"furion spin gyroscope",
	},
	{	/* Phaser bank parts */
		"phased enluxinator",
		"phase transfer matrix",
		"annular phase conjugator",
		"photonic phase tuner",
		"six channel Rose frequency modulator",
	},
	{	/* sensor system parts */
		"furion detector array",
		"harmonic damper circuit",
		"harmonic differential amplifier",
		"subharmonic omni-stabilizer",
		"furion-photon transformer ",
	},
	{	/* communication system parts */
		"radionic detector coil",
		"radionic field modulator",
		"Antenna control module",
		"Hibbert space furionic amplifier",
		"furionic field multiplexer",
	},
	{	/* tractor beam system parts */
		"Van Grinsven field modulator",
		"Dual stage ion brake",
		"Graviton induction unit",
		"Black hole suppressor",
		"Turbo-gravitaser collimator",
	},
	{	/* repair station parts... special case, no parts. */
		"RS BUG", /* if you see these in the game, it's a bug. */
		"RS BUG",
		"RS BUG",
		"RS BUG",
		"RS BUG",
	},
};

static char *damcon_tools[] = {
	"Magneto-forceps",
	"photon wrench",
	"tuning knife",
	"furionic multimeter",
	"furionic calibrator",
	"Hibbert entangler",
};

static char *damcon_damages[] = {
	"is shorted out",
	"is blown",
	"has melted down",
	"is burned out",
	"has vaporized",
	"is destroyed",
	"has malfunctioned",
	"has faulted",
	"is malfunctioning",
	"is unresponsive",
	"needs to be replaced",
	"is not functional",
	"has ceased to be",
	"is pining for the fjords",
};

char *damcon_part_name(uint8_t system, uint8_t part)
{
	if (system < 0 || system >= ARRAYSIZE(damcon_part_names))
		return "UNKNOWN";
	if (part < 0 || part >= PARTS_PER_DAMCON_SYSTEM)
		return "UNKNOWN";
	return damcon_part_names[system][part];
}

char *damcon_system_name(uint8_t system)
{
	if (system < 0 || system >= ARRAYSIZE(damcon_system_names))
		return "UNKNOWN";
	return damcon_system_names[system];
}

char *damcon_tool_name(uint8_t tool)
{
	if (tool < 0 || tool >= ARRAYSIZE(damcon_tools))
		return "UNKNOWN";
	return damcon_tools[tool];
}

char *damcon_damage_name(uint8_t damage)
{
	if (damage < 0 || damage >= ARRAYSIZE(damcon_damages))
		return "UNKNOWN";
	return damcon_damages[damage];
}



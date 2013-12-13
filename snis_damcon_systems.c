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

static char *damcon_part_names[][DAMCON_PARTS_PER_SYSTEM] = {
	{	/* shield system parts */
		"VON KURNATOWSKI FIELD GENERATOR",
		"FURION REFLECTOR MATRIX",
		"PHOTONIC CHARGE STABILIZER",
		/* "PHASE CONJUGATE BARYON REFLECTOR",
		"TACHYONIC ENERGY ABSORPTION MATRIX",  */
	},
	{	/* impulse drive parts */
		"POSITRON EMITTER TUBE",
		/* "PLASMA CURRENT REGULATOR",
		"WILHELM JUNCTION", */
		"ANTIMATTER BACKFLASH SUPPRESSOR",
		"POSITRONIC CHARGING COIL",
	},
	{	/* warp drive parts */
		/* "RELATIVISTIC TORQUE AMPLIFIER", */
		"TRANSIENT WORMHOLE SUPPRESSOR",
		/* "PLASMA WASTEGATE CONTROLLER", */
		"HIBBERT SPACE MULTIPLEXER",
		"SAGAN CONUNDRUM RESOLVER",
	},
	{	/* maneuvering parts */
		"INERTIAL NAVIGATION MODULE",
		/* "THRUSTER CONTROL MODULE", */
		"HIBBERT SPACE TRANSFER COMPUTER",
		/* "STAR TRACKING MODULE", */
		"FURION SPIN GYROSCOPE",
	},
	{	/* Phaser bank parts */
		"PHASED ENLUXINATOR",
		/* "PHASE TRANSFER MATRIX", */
		"ANNULAR PHASE CONJUGATOR",
		/* "PHOTONIC PHASE TUNER", */
		"SIX CHANNEL ROSE FREQUENCY MODULATOR",
	},
	{	/* sensor system parts */
		"FURION DETECTOR ARRAY",
		/* "HARMONIC DAMPER CIRCUIT", */
		/* "HARMONIC DIFFERENTIAL AMPLIFIER", */
		"SUBHARMONIC OMNI-STABILIZER",
		"FURION-PHOTON TRANSFORMER ",
	},
	{	/* communication system parts */
		"RADIONIC DETECTOR COIL",
		/* "RADIONIC FIELD MODULATOR", */
		/* "ANTENNA CONTROL MODULE", */
		"HIBBERT SPACE FURIONIC AMPLIFIER",
		"FURIONIC FIELD MULTIPLEXER",
	},
	{	/* tractor beam system parts */
		"VAN GRINSVEN FIELD MODULATOR",
		"DUAL STAGE ION BRAKE",
		"GRAVITON INDUCTION UNIT",
		/* "BLACK HOLE SUPPRESSOR",
		"TURBO-GRAVITASER COLLIMATOR", */
	},
	{	/* repair station parts... special case, no parts. */
		"RS BUG", /* if you see these in the game, it's a bug. */
		"RS BUG",
		"RS BUG",
		/* "RS BUG",
		"RS BUG", */
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
	if (part < 0 || part >= DAMCON_PARTS_PER_SYSTEM)
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



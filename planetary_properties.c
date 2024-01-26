#include "planetary_properties.h"
#include "infinite-taunt.h" /* for enum planet_type */
#include "mathutils.h"
#include "arraysize.h"
#include <stdio.h>

/* See http://phl.upr.edu/library/notes/standardmass-radiusrelationforexoplanets */

static const double planet_mass[] =	{ 0.0, 0.060, 0.105, 1.0, 4.5, 10.0, 40.0, 200.0, };
static const double planet_radius[] =	{ 0.0, 0.400, 0.550, 1.0, 2.0, 4.00, 6.00, 15.00, };

#define GAS_GIANT_MASS_MULTIPLIER 300.0
#define GAS_GIANT_RADIUS_MULTIPLIER 12.0

double planetary_mass(double radius, enum planet_type t)
{
	double eradius = (radius / 6000.0);
	double gas_giant_multiplier = (t == planet_type_gas_giant) ? GAS_GIANT_MASS_MULTIPLIER : 1.0;

	if (eradius < 0)
		eradius = 0;
	if (eradius > 200.0)
		eradius = 200.0;

	return gas_giant_multiplier * table_interp(eradius, planet_radius, planet_mass, ARRAYSIZE(planet_radius));
}

/* Returns "diameter" of planet as a multiple of earth's diameter */
double planetary_diameter(double radius, enum planet_type t)
{
	/* We lie about the gas giant diameter to make them seem bigger. */
	double gas_giant_multiplier = (t == planet_type_gas_giant) ? GAS_GIANT_RADIUS_MULTIPLIER : 1.0;
	return gas_giant_multiplier * radius / 6000.0;
}

double planetary_gravity(double radius, enum planet_type t)
{
	const double radius_of_earth_meters = 6378100.0;
	const double G = 6.6743e-11; /* gravitational constant */
	const double earth_mass_kg = 5.972e24;
	double mass = planetary_mass(radius, t) * earth_mass_kg;

	double r = planetary_diameter(radius, t) * radius_of_earth_meters;
	return G * (mass / (r * r));
}

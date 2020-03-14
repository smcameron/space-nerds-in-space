#include "planetary_properties.h"
#include "infinite-taunt.h" /* for enum planet_type */
#include "mathutils.h"
#include "arraysize.h"
#include <stdio.h>

/* See http://phl.upr.edu/library/notes/standardmass-radiusrelationforexoplanets */

static const double planet_mass[] =	{ 0.0, 0.060, 0.105, 1.0, 4.5, 10.0, 40.0, 200.0, };
static const double planet_radius[] =	{ 0.0, 0.400, 0.550, 1.0, 2.0, 4.00, 6.00, 15.00, };

double planetary_mass(double radius, enum planet_type t)
{
	double eradius = (radius / 6000.0);
	double gas_giant_multiplier = (t == planet_type_gas_giant) ? 12.0 : 1.0;

	if (eradius < 0)
		eradius = 0;
	if (eradius > 200.0)
		eradius = 200.0;

	return gas_giant_multiplier * table_interp(eradius, planet_radius, planet_mass, ARRAYSIZE(planet_radius));
}

double planetary_diameter(double radius, enum planet_type t)
{
	/* We lie about the gas giant diameter to make them seem bigger. */
	double gas_giant_multiplier = (t == planet_type_gas_giant) ? 12.0 : 1.0;
	return gas_giant_multiplier * radius / 6000.0;
}


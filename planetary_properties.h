#ifndef PLANETARY_PROPERTIES_H__
#define PLANETARY_PROPERTIES_H__
#include "infinite-taunt.h" /* for enum planet_type */

double planetary_mass(double radius, enum planet_type t);
double planetary_diameter(double radius, enum planet_type t);
double planetary_gravity(double radisu, enum planet_type t);

#endif


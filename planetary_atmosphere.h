#ifndef PLANETARY_ATMOSPHERE_H__
#define PLANETARY_ATMOSPHERE_H__
#include <stdint.h>

#include "mtwist.h"

enum planetary_atmosphere_type {
	earthlike_atmosphere_type = 0,
	venuslike_atmosphere_type = 1,
	marslike_atmosphere_type = 2,
	gas_giant_atmosphere_type = 3,
	titanlike_atmosphere_type = 4,
	ice_giant_atmosphere_type = 5,
};

extern struct atmospheric_compound_entry {
	char *name;
	char *symbol;
} atmospheric_compound[];

struct planetary_atmosphere_profile {
	int index;
	enum planetary_atmosphere_type type;
	char *name;
	float pressure;
	float temperature;
	int nmajor, nminor; /* number of major and minor compounds */
	int *major_compound;
	int *minor_compound;
	float *major_fraction;
	float *minor_ppm;
};

extern struct planetary_atmosphere_profile *venus_atmosphere_model;
extern struct planetary_atmosphere_profile *earth_atmosphere_model;
extern struct planetary_atmosphere_profile *mars_atmosphere_model;
extern struct planetary_atmosphere_profile *jupiter_atmosphere_model;
extern struct planetary_atmosphere_profile *saturn_atmosphere_model;
extern struct planetary_atmosphere_profile *uranus_atmosphere_model;
extern struct planetary_atmosphere_profile *neptune_atmosphere_model;
extern struct planetary_atmosphere_profile *titan_atmosphere_model;

/* Create a new planetary atmosphere model with the specified name, temp and pressure.
 * If model is not null, the values of the model will be used as a basis
 */
struct planetary_atmosphere_profile *planetary_atmosphere_profile_new(char *name, enum planetary_atmosphere_type,
			float temperature, float pressure, struct planetary_atmosphere_profile *model);
/* Add a major component compound to an atmosphere at the specified fraction */
void planetary_atmosphere_profile_add_major(struct planetary_atmosphere_profile *profile,
						char *compound, float fraction);
/* Add a minor component compound to the atmosphere at the specified parts per million */
void planetary_atmosphere_profile_add_minor(struct planetary_atmosphere_profile *profile,
						char *compound, float ppm);
/* Lookup a planetary atmosphere by name */
struct planetary_atmosphere_profile *planetary_atmosphere_profile_lookup(char *name);
/* Print out a planetary atmosphere */
void planetary_atmosphere_profile_print(struct planetary_atmosphere_profile *p);
/* Set up the canonical model atmospheres */
void planetary_atmosphere_model_init_models(uint32_t seed, int num_atmospheres_per_type);

struct planetary_atmosphere_profile *planetary_atmosphere_by_index(int index);
int planetary_atmosphere_profile_index(struct planetary_atmosphere_profile *p);
int random_planetary_atmosphere_by_type(struct mtwist_state *mt, enum planetary_atmosphere_type t, int n);

#endif

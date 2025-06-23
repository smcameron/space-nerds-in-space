#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "arraysize.h"
#include "planetary_atmosphere.h"
#include "mtwist.h"

static int num_atmos_per_type = 100;

static const char * const atmosphere_type_name[] = {
	"EARTH-CLASS",
	"VENUS-CLASS",
	"MARS-CLASS",
	"GAS-GIANT",
	"TITAN-CLASS",
	"ICE-GIANT",
};

static int first_random_atmosphere = 0;

struct atmospheric_compound_entry atmospheric_compound[] = {
	{ "CARBON DIOXIDE", "CO2", },
	{ "NITROGEN", "N2", },
	{ "OXYGEN", "O2", },
	{ "HYDROGEN", "H2", },
	{ "OZONE", "O3", },
	{ "SULFUR DIOXIDE", "SO2", },
	{ "ARGON", "AR", },
	{ "WATER VAPOR", "H2O", },
	{ "CARBON MONOXIDE", "CO", },
	{ "HELIUM", "HE", },
	{ "NEON", "NE", },
	{ "HYDROGEN CHLORIDE", "HCL", },
	{ "HYDROGEN FLOURIDE", "HFL", },
	{ "KRYPTON", "KR", },
	{ "METHANE", "CH4", },
	{ "XENON", "XE", },
	{ "PHOSPHINE", "PH3", },
	{ "AMMONIA", "NH3", },
	{ "HYDROGEN SULFIDE", "H2S", },
	{ "ARSINE", "AsH3", },
	{ "GERMANE", "GeH4", },
	{ "ETHANE", "C2H6", },
	{ "ETHYLENE", "C2H4", },
	{ "ACETYLENE", "C2H2", },
	{ "DIACETYLENE", "(C2H2)2", },
	{ "HYDROGEN CYANIDE", "HCN", },
	{ "PROPANE", "C3H8", },
	{ "CYANOACETYLENE", "C3HN", },
	{ "CYANOGEN", "(CN)2", },
	{ "SODIUM", "Na", },
	{ "POTASSIUM", "K", },
	{ "MAGNESIUM", "Mg", },
	{ "SILICON", "Si", },
	{ "HYDROGEN DEUTERIDE", "HD", },
	{ "HEAVY WATER", "HDO", },
	{ "NITROGEN OXIDE", "NO", },
};

static int natmos_profiles = 0;
static struct planetary_atmosphere_profile **atmos_profile = NULL;

struct planetary_atmosphere_profile *venus_atmosphere_model = NULL;
struct planetary_atmosphere_profile *earth_atmosphere_model = NULL;
struct planetary_atmosphere_profile *mars_atmosphere_model = NULL;
struct planetary_atmosphere_profile *jupiter_atmosphere_model = NULL;
struct planetary_atmosphere_profile *saturn_atmosphere_model = NULL;
struct planetary_atmosphere_profile *uranus_atmosphere_model = NULL;
struct planetary_atmosphere_profile *neptune_atmosphere_model = NULL;
struct planetary_atmosphere_profile *titan_atmosphere_model = NULL;

static void free_profile(struct planetary_atmosphere_profile *p)
{
	if (!p)
		return;
	if (p->name) {
		free(p->name);
		p->name = NULL;
	}
	if (p->major_compound) {
		free(p->major_compound);
		p->major_compound = NULL;
	};
	if (p->minor_compound) {
		free(p->minor_compound);
		p->minor_compound = NULL;
	};
	if (p->major_fraction) {
		free(p->major_fraction);
		p->major_fraction = NULL;
	}
	if (p->minor_ppm) {
		free(p->minor_ppm);
		p->minor_ppm = NULL;
	};
	p->nmajor = 0;
	p->nminor = 0;
	free(p);
}

static int lookup_compound(char *name)
{
	int i;

	for (i = 0; (size_t) i < ARRAYSIZE(atmospheric_compound); i++) {
		if (strcmp(atmospheric_compound[i].name, name) == 0)
			return i;
	}
	return -1;
}

static int lookup_profile(char *name)
{
	int i;

	for (i = 0; i < natmos_profiles; i++) {
		if (strcmp(atmos_profile[i]->name, name) == 0)
			return i;
	}
	return -1;
}

static float perturb_float(struct mtwist_state *mt, float value, float fraction)
{
	float amount = (mtwist_float(mt) * 2.0 - 1.0) * fraction * value;
	return amount;
}

static void planetary_atmosphere_profile_perturb(struct mtwist_state *mt, struct planetary_atmosphere_profile *p)
{
	int i;
	int loops = 0;

	/* perturb temperature and pressure */
	p->temperature += perturb_float(mt, p->temperature, 0.35);
	p->pressure += perturb_float(mt, p->pressure, 0.35);

	/* Perturb major compounds */
	for (i = 0; i < p->nmajor - 1; i++) {
		float amount, a, b;

		do {
			amount = perturb_float(mt, p->major_fraction[i], 0.37);
			a = p->major_fraction[i] + amount;
			b = p->major_fraction[i + 1] - amount;
			loops++;
		} while ((a < 0.0 || a > 100.0 || b < 0.0 || b > 100.0) && loops < 1000);
		if (loops > 1000) {
			printf("loops = %d, i = %d\n", loops, i);
			planetary_atmosphere_profile_print(p);
			exit(1);
		}
		p->major_fraction[i] += amount;
		p->major_fraction[i + 1] -= amount;
	}

	/* Perturb minor compounds */
	for (i = 0; i < p->nminor; i++)
		p->minor_ppm[i] += perturb_float(mt, p->minor_ppm[i], 0.25);
}

struct planetary_atmosphere_profile *planetary_atmosphere_profile_new(char *name, enum planetary_atmosphere_type t,
		float temperature, float pressure, struct planetary_atmosphere_profile *model)
{
	int i;

	struct planetary_atmosphere_profile *p = malloc(sizeof(*p));

	i = lookup_profile(name);
	if (i >= 0) {
		free_profile(atmos_profile[i]);
		atmos_profile[i] = p;
	}
	p->name = strdup(name);
	if (!model) {
		p->temperature = temperature;
		p->pressure = pressure;
		p->nminor = 0;
		p->nmajor = 0;
		p->minor_compound = NULL;
		p->major_compound = NULL;
		p->minor_ppm = NULL;
		p->major_fraction = NULL;
		p->type = t;
	} else {
		p->temperature = model->temperature;
		p->pressure = model->pressure;
		p->type = model->type;
		p->nminor = model->nminor;
		p->nmajor = model->nmajor;
		p->major_compound = malloc(p->nmajor * sizeof(*p->major_compound));
		p->minor_compound = malloc(p->nminor * sizeof(*p->minor_compound));
		p->minor_ppm = malloc(p->nminor * sizeof(*p->minor_ppm));
		p->major_fraction = malloc(p->nmajor * sizeof(*p->major_fraction));
		memcpy(p->major_compound, model->major_compound, p->nmajor * sizeof(*p->major_compound));
		memcpy(p->minor_compound, model->minor_compound, p->nminor * sizeof(*p->minor_compound));
		memcpy(p->minor_ppm, model->minor_ppm, p->nminor * sizeof(*p->minor_ppm));
		memcpy(p->major_fraction, model->major_fraction, p->nmajor * sizeof(*p->major_fraction));
	}
	if (i < 0) {
		struct planetary_atmosphere_profile **new_atmos_profile;
		new_atmos_profile = realloc(atmos_profile, sizeof(*atmos_profile) * (natmos_profiles + 1));
		if (new_atmos_profile) {
			atmos_profile = new_atmos_profile;
		} else {
			free_profile(p);
			return NULL;
		}
		atmos_profile[natmos_profiles] = p;
		atmos_profile[natmos_profiles]->index = natmos_profiles;
		natmos_profiles++;
	}
	return atmos_profile[natmos_profiles - 1];
}

void planetary_atmosphere_profile_add_major(struct planetary_atmosphere_profile *profile,
						char *compound, float fraction)
{
	int i = lookup_compound(compound);
	if (i < 0) {
		printf("Failed to lookup compound %s\n", compound);
		return;
	}
	profile->major_compound = realloc(profile->major_compound,
						(profile->nmajor + 1) * sizeof(*profile->major_compound));
	profile->major_compound[profile->nmajor] = i;
	profile->major_fraction = realloc(profile->major_fraction,
						(profile->nmajor + 1) * sizeof(*profile->major_fraction));
	profile->major_fraction[profile->nmajor] = fraction;
	profile->nmajor++;
}

void planetary_atmosphere_profile_add_minor(struct planetary_atmosphere_profile *profile,
						char *compound, float ppm)
{
	int i = lookup_compound(compound);
	if (i < 0) {
		printf("Failed to lookup compound %s\n", compound);
		return;
	}
	profile->minor_compound = realloc(profile->minor_compound,
					(profile->nminor + 1) * sizeof(*profile->minor_compound));
	profile->minor_compound[profile->nminor] = i;
	profile->minor_ppm = realloc(profile->minor_ppm, (profile->nminor + 1) * sizeof(*profile->minor_ppm));
	profile->minor_ppm[profile->nminor] = ppm;
	profile->nminor++;
}

struct planetary_atmosphere_profile *planetary_atmosphere_profile_lookup(char *name)
{
	int i = lookup_profile(name);
	if (i < 0)
		return NULL;
	return atmos_profile[i];
}

static void add_random_atmospheres(struct mtwist_state *mt, enum planetary_atmosphere_type t,
				struct planetary_atmosphere_profile *model, int count, int offset)
{
	int i;
	struct planetary_atmosphere_profile *p;
	char name[30];

	for (i = 0; i < count; i++) {
		sprintf(name, "%s-%d", atmosphere_type_name[t], i + offset);
		p = planetary_atmosphere_profile_new(name, t, 0.0, 0.0, model);
		planetary_atmosphere_profile_perturb(mt,  p);
	}
}

void planetary_atmosphere_model_init_models(uint32_t seed, int num_atmospheres_per_type)
{
	struct mtwist_state *mt;

	num_atmos_per_type = num_atmospheres_per_type;

	mt = mtwist_init(seed);

	/* Warning: do not suppose these values to be scientifically accurate. */
	venus_atmosphere_model = planetary_atmosphere_profile_new("venuslike",
					venuslike_atmosphere_type, 737, 9.2e6, NULL);
	planetary_atmosphere_profile_add_major(venus_atmosphere_model, "CARBON DIOXIDE", 0.965);
	planetary_atmosphere_profile_add_major(venus_atmosphere_model, "NITROGEN", 0.035);
	planetary_atmosphere_profile_add_minor(venus_atmosphere_model, "SULFUR DIOXIDE", 150.0);
	planetary_atmosphere_profile_add_minor(venus_atmosphere_model, "ARGON", 70.0);
	planetary_atmosphere_profile_add_minor(venus_atmosphere_model, "WATER VAPOR", 20.0);
	planetary_atmosphere_profile_add_minor(venus_atmosphere_model, "CARBON MONOXIDE", 17.0);
	planetary_atmosphere_profile_add_minor(venus_atmosphere_model, "HELIUM", 12.0);
	planetary_atmosphere_profile_add_minor(venus_atmosphere_model, "NEON", 7.0);
	planetary_atmosphere_profile_add_minor(venus_atmosphere_model, "HYDROGEN CHLORIDE", 0.3);
	planetary_atmosphere_profile_add_minor(venus_atmosphere_model, "HYDROGEN FLOURIDE", 0.003);

	earth_atmosphere_model = planetary_atmosphere_profile_new("earthlike",
					earthlike_atmosphere_type, 294.0, 101325, NULL);
	planetary_atmosphere_profile_add_major(earth_atmosphere_model, "NITROGEN", 0.78084);
	planetary_atmosphere_profile_add_major(earth_atmosphere_model, "OXYGEN", 0.20946);
	planetary_atmosphere_profile_add_major(earth_atmosphere_model, "WATER VAPOR", 0.02);
	planetary_atmosphere_profile_add_major(earth_atmosphere_model, "ARGON", 0.00934);
	planetary_atmosphere_profile_add_minor(earth_atmosphere_model, "CARBON DIOXIDE", 400.0);
	planetary_atmosphere_profile_add_minor(earth_atmosphere_model, "NEON", 18.18);
	planetary_atmosphere_profile_add_minor(earth_atmosphere_model, "HELIUM", 5.24);
	planetary_atmosphere_profile_add_minor(earth_atmosphere_model, "METHANE", 1.79);

	mars_atmosphere_model = planetary_atmosphere_profile_new("marslike",
					marslike_atmosphere_type, 210.0, 600.0, NULL);
	planetary_atmosphere_profile_add_major(mars_atmosphere_model, "CARBON DIOXIDE", 0.9532);
	planetary_atmosphere_profile_add_major(mars_atmosphere_model, "NITROGEN", 0.027);
	planetary_atmosphere_profile_add_major(mars_atmosphere_model, "ARGON", 0.016);
	planetary_atmosphere_profile_add_major(mars_atmosphere_model, "OXYGEN", 0.0013);
	planetary_atmosphere_profile_add_major(mars_atmosphere_model, "CARBON MONOXIDE", 0.0008);
	planetary_atmosphere_profile_add_minor(mars_atmosphere_model, "WATER VAPOR", 210);
	planetary_atmosphere_profile_add_minor(mars_atmosphere_model, "NITROGEN OXIDE", 100);
	planetary_atmosphere_profile_add_minor(mars_atmosphere_model, "NEON", 2.5);
	planetary_atmosphere_profile_add_minor(mars_atmosphere_model, "HEAVY WATER", 0.85);
	planetary_atmosphere_profile_add_minor(mars_atmosphere_model, "KRYPTON", 0.3);
	planetary_atmosphere_profile_add_minor(mars_atmosphere_model, "XENON", 0.08);

	jupiter_atmosphere_model = planetary_atmosphere_profile_new("jupiterlike",
					gas_giant_atmosphere_type, 165.0, 2e6, NULL);
	planetary_atmosphere_profile_add_major(jupiter_atmosphere_model, "HYDROGEN", 0.898);
	planetary_atmosphere_profile_add_major(jupiter_atmosphere_model, "HELIUM", 0.102);
	planetary_atmosphere_profile_add_minor(jupiter_atmosphere_model, "METHANE", 3000);
	planetary_atmosphere_profile_add_minor(jupiter_atmosphere_model, "AMMONIA", 260);
	planetary_atmosphere_profile_add_minor(jupiter_atmosphere_model, "HYDROGEN DEUTERIDE", 28);
	planetary_atmosphere_profile_add_minor(jupiter_atmosphere_model, "ETHANE", 5.8);
	planetary_atmosphere_profile_add_minor(jupiter_atmosphere_model, "WATER VAPOR", 4);

	saturn_atmosphere_model = planetary_atmosphere_profile_new("saturnlike",
					gas_giant_atmosphere_type, 134.0, 2e6, NULL);
	planetary_atmosphere_profile_add_major(saturn_atmosphere_model, "HYDROGEN", 0.96);
	planetary_atmosphere_profile_add_major(saturn_atmosphere_model, "HELIUM", 0.0325);
	planetary_atmosphere_profile_add_minor(saturn_atmosphere_model, "METHANE", 4500);
	planetary_atmosphere_profile_add_minor(saturn_atmosphere_model, "AMMONIA", 125);
	planetary_atmosphere_profile_add_minor(saturn_atmosphere_model, "HYDROGEN DEUTERIDE", 110);
	planetary_atmosphere_profile_add_minor(saturn_atmosphere_model, "ETHANE", 7);

	uranus_atmosphere_model = planetary_atmosphere_profile_new("uranuslike",
					ice_giant_atmosphere_type, 49.0, 9e5, NULL);
	planetary_atmosphere_profile_add_major(uranus_atmosphere_model, "HYDROGEN", 0.965);
	planetary_atmosphere_profile_add_major(uranus_atmosphere_model, "HELIUM", 0.027);
	planetary_atmosphere_profile_add_major(uranus_atmosphere_model, "METHANE", 0.023);
	planetary_atmosphere_profile_add_minor(uranus_atmosphere_model, "AMMONIA", 105);
	planetary_atmosphere_profile_add_minor(uranus_atmosphere_model, "HYDROGEN SULFIDE", 42);
	planetary_atmosphere_profile_add_minor(uranus_atmosphere_model, "ETHANE", 35);
	planetary_atmosphere_profile_add_minor(uranus_atmosphere_model, "WATER VAPOR", 20);
	planetary_atmosphere_profile_add_minor(uranus_atmosphere_model, "ACETYLENE", 15);
	planetary_atmosphere_profile_add_minor(uranus_atmosphere_model, "CARBON DIOXIDE", 15);
	planetary_atmosphere_profile_add_minor(uranus_atmosphere_model, "DIACETYLENE", 13);
	planetary_atmosphere_profile_add_minor(uranus_atmosphere_model, "CARBON MONOXIDE", 12);

	neptune_atmosphere_model = planetary_atmosphere_profile_new("neptunelike",
					ice_giant_atmosphere_type, 72.0, 1e7, NULL);
	planetary_atmosphere_profile_add_major(neptune_atmosphere_model, "HYDROGEN", 0.8);
	planetary_atmosphere_profile_add_major(neptune_atmosphere_model, "HELIUM", 0.19);
	planetary_atmosphere_profile_add_major(neptune_atmosphere_model, "METHANE", 0.015);
	planetary_atmosphere_profile_add_minor(neptune_atmosphere_model, "HYDROGEN DEUTERIDE", 192);
	planetary_atmosphere_profile_add_minor(neptune_atmosphere_model, "HYDROGEN SULFIDE", 82);
	planetary_atmosphere_profile_add_minor(neptune_atmosphere_model, "WATER VAPOR", 70);
	planetary_atmosphere_profile_add_minor(neptune_atmosphere_model, "ETHANE", 1.5);
	planetary_atmosphere_profile_add_minor(neptune_atmosphere_model, "CARBON MONOXIDE", 30);
	planetary_atmosphere_profile_add_minor(neptune_atmosphere_model, "HYDROGEN CYANIDE", 16);

	titan_atmosphere_model = planetary_atmosphere_profile_new("titanlike",
					titanlike_atmosphere_type, 94.0, 149288, NULL);
	planetary_atmosphere_profile_add_major(titan_atmosphere_model, "NITROGEN", 0.984);
	planetary_atmosphere_profile_add_major(titan_atmosphere_model, "METHANE", 0.014);
	planetary_atmosphere_profile_add_major(titan_atmosphere_model, "HYDROGEN", 0.0015);
	planetary_atmosphere_profile_add_minor(titan_atmosphere_model, "ETHANE", 115);
	planetary_atmosphere_profile_add_minor(titan_atmosphere_model, "DIACETYLENE", 75);
	planetary_atmosphere_profile_add_minor(titan_atmosphere_model, "ACETYLENE", 43);
	planetary_atmosphere_profile_add_minor(titan_atmosphere_model, "PROPANE", 24);
	planetary_atmosphere_profile_add_minor(titan_atmosphere_model, "CYANOACETYLENE", 21);
	planetary_atmosphere_profile_add_minor(titan_atmosphere_model, "HYDROGEN CYANIDE", 20);
	planetary_atmosphere_profile_add_minor(titan_atmosphere_model, "CARBON DIOXIDE", 17);
	planetary_atmosphere_profile_add_minor(titan_atmosphere_model, "CARBON MONOXIDE", 9);
	planetary_atmosphere_profile_add_minor(titan_atmosphere_model, "CYANOGEN", 9);
	planetary_atmosphere_profile_add_minor(titan_atmosphere_model, "ARGON", 7);
	planetary_atmosphere_profile_add_minor(titan_atmosphere_model, "HELIUM", 2);

	first_random_atmosphere = planetary_atmosphere_profile_index(titan_atmosphere_model) + 1;

	add_random_atmospheres(mt, earthlike_atmosphere_type, earth_atmosphere_model, num_atmos_per_type, 0);
	add_random_atmospheres(mt, venuslike_atmosphere_type, venus_atmosphere_model, num_atmos_per_type, 0);
	add_random_atmospheres(mt, marslike_atmosphere_type, mars_atmosphere_model, num_atmos_per_type, 0);
	add_random_atmospheres(mt, gas_giant_atmosphere_type, jupiter_atmosphere_model, num_atmos_per_type / 2, 0);
	add_random_atmospheres(mt, gas_giant_atmosphere_type, saturn_atmosphere_model,
				num_atmos_per_type / 2, num_atmos_per_type / 2);
	add_random_atmospheres(mt, ice_giant_atmosphere_type, neptune_atmosphere_model, num_atmos_per_type / 2, 0);
	add_random_atmospheres(mt, ice_giant_atmosphere_type, uranus_atmosphere_model,
				num_atmos_per_type / 2, num_atmos_per_type / 2);
	add_random_atmospheres(mt, titanlike_atmosphere_type, titan_atmosphere_model, num_atmos_per_type, 0);
}

void planetary_atmosphere_profile_print(struct planetary_atmosphere_profile *p)
{
	int i;
	char compound_name[100];

	printf("profile: %s (%d)\n", p->name, p->index);
	printf("type: %s\n", atmosphere_type_name[p->type]);
	printf("Pressure: %f Pascal (%.1fxEarth)\n", p->pressure, p->pressure / 101325.0);
	printf("Temperature: %.1fK/%.1fC/%.1fF\n", p->temperature,
			p->temperature - 273.15, p->temperature * 9.0 / 5.0 - 459.67);

	for (i = 0; i < p->nmajor; i++) {
		int compound = p->major_compound[i];
		char *name = atmospheric_compound[compound].name;
		char *symbol = atmospheric_compound[compound].symbol;
		sprintf(compound_name, "%s (%s)", name, symbol);
		printf("%30s: %3.2f%%\n", compound_name, p->major_fraction[i] * 100.0);
	}
	for (i = 0; i < p->nminor; i++) {
		int compound = p->minor_compound[i];
		char *name = atmospheric_compound[compound].name;
		char *symbol = atmospheric_compound[compound].symbol;
		sprintf(compound_name, "%s (%s)", name, symbol);
		printf("%30s: %4.2f ppm\n", compound_name, p->minor_ppm[i]);
	}
	printf("\n");
}

struct planetary_atmosphere_profile *planetary_atmosphere_by_index(int index)
{
	if (index < 0 || index >= natmos_profiles)
		return NULL;
	return atmos_profile[index];
}

int planetary_atmosphere_profile_index(struct planetary_atmosphere_profile *p)
{
	return p->index;
}

int random_planetary_atmosphere_by_type(struct mtwist_state *mt, enum planetary_atmosphere_type t, int n)
{
	int offset = first_random_atmosphere + num_atmos_per_type * (int) t;
	if (n < 0 && mt)
		return offset + mtwist_int(mt, num_atmos_per_type);
	else
		return offset + n;
}

#ifdef TEST_PLANETARY_ATMOSPHERE_PROFILE

int main(int argc, char *argv[])
{
	int i;

	planetary_atmosphere_model_init_models(3124152, 10);
	for (i = 0; i < natmos_profiles; i++)
		planetary_atmosphere_profile_print(atmos_profile[i]);
	return 0;
}
#endif

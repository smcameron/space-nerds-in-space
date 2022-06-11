#include <stdlib.h>
#include <string.h>

#include "mtwist.h"
#include "power-model.h"
#include "mathutils.h"

struct power_model;

struct power_device {
	struct power_model *pm;
	resistor_sample_fn r1, r2, r3;
	float or1, or2, or3; /* previous values of resistors */
	float damage;
	void *cookie;
	float i;
};

#define MAX_DEVICES_PER_MODEL 9
struct power_model {
	struct power_device *d[MAX_DEVICES_PER_MODEL];
	int ndevices;
	float max_current;
	float nominal_voltage;
	float actual_voltage;
	float internal_resistance;
	float actual_current;
	int enabled;
};

struct power_device *new_power_device(void * cookie, resistor_sample_fn r1,
			resistor_sample_fn r2, resistor_sample_fn r3)
{
	struct power_device *d = malloc(sizeof(*d));

	d->r1 = r1;
	d->r2 = r2;
	d->r3 = r3;
	d->or1 = 0;
	d->or2 = 0;
	d->or3 = 0;
	d->i = 0;
	d->cookie = cookie;
	d->damage = 0.0;
	return d;
}

struct power_model *new_power_model(float max_current, float voltage,
					float internal_resistance)
{
	struct power_model *m = malloc(sizeof(*m));

	memset(m, 0, sizeof(*m));
	m->nominal_voltage = voltage;
	m->max_current = max_current;
	m->internal_resistance = internal_resistance;
	m->enabled = 1;
	return m;
}

void power_model_add_device(struct power_model *m, struct power_device *device)
{
	int n = m->ndevices;

	if (n >= MAX_DEVICES_PER_MODEL)
		return;
	m->d[n] = device;
	m->ndevices++;
	device->pm = m;
}

static void update_resistance(float *r, float new_r)
{
	*r = *r + ((new_r - *r) / 4.0);
}

static void power_model_update_resistances(struct power_model *m)
{
	float nr1, nr2, nr3;
	int i;

	for (i = 0; i < m->ndevices; i++) {
		struct power_device *d = m->d[i];
		void *cookie = d->cookie;

		nr1 = d->r1(cookie);
		nr2 = d->r2(cookie);
		if (nr1 < nr2)
			nr1 = nr2;
		nr3 = d->r3(cookie);

		update_resistance(&d->or1, nr1);
		update_resistance(&d->or2, nr2);
		update_resistance(&d->or3, nr3);
	}
}

void power_model_compute(struct power_model *m)
{
	int i;
	float total_resistance = m->internal_resistance;
	float conductance = 0.0;

	power_model_update_resistances(m);
	for (i = 0; i < m->ndevices; i++) {
		struct power_device *d = m->d[i];
		float r;

		r = d->or1 + d->or3; 
		conductance += 1.0 / r; 
	}
	total_resistance += 1.0 / conductance;
	m->actual_current = (float) m->enabled * m->nominal_voltage / total_resistance;

	if (m->actual_current > m->max_current) {
		m->actual_voltage = m->max_current * total_resistance;
		m->actual_current = m->max_current;
	} else {
		m->actual_voltage = m->actual_current * total_resistance; 
	}

	for (i = 0; i < m->ndevices; i++) {
		struct power_device *d = m->d[i];
		float r = d->or1 + d->or3;

		d->i = m->actual_voltage / r;
		if (d->i < (m->max_current / 256.0))
			d->i = 0.0;
	}
}

float device_current(struct power_device *d)
{
	float current = d->i * (1.0 - d->damage) -
		(d->damage * snis_randn(256) / 256.0f) * d->i / 4.0f;
	if (current < 0.0f)
		current = 0.0f;
	return current;
}

float device_max_current(struct power_device *d)
{
	return d->pm->nominal_voltage / d->r3(d->cookie);
}

float power_model_total_current(struct power_model *m)
{
	return m->actual_current;
}

struct power_device *power_model_get_device(struct power_model *m, int i)
{
	if (i < 0 || i >= m->ndevices)
		return NULL; 
	return m->d[i];
}

void free_power_model(struct power_model *m)
{
	int i;

	for (i = 0; i < m->ndevices; i++)
		free(m->d[i]);
	free(m);
}

float power_model_nominal_voltage(struct power_model *m)
{
	return m->nominal_voltage;
}

float power_model_actual_voltage(struct power_model *m)
{
	return m->actual_voltage;
}
	
void power_model_enable(struct power_model *m)
{
	m->enabled = 1;
}

void power_model_disable(struct power_model *m)
{
	m->enabled = 0;
}

void power_device_set_damage(struct power_device *d, float damage)
{
	d->damage = damage;
}

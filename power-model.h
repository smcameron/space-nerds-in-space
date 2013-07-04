#ifndef POWERMODEL_H__
#define POWERMODEL_H__

/*********************************************************************

  This module defines a crude power model based (loosely) on the behavior
   of an electric circuit.
  
   The circuit looks like this:

             R01        R02        R03
 +---------/\/\/-------------------/\/\/--------+ S0
 |                                              |
 |           R11        R12         R13         |
 +---------/\/\/-------------------/\/\/--------+ S1
 |                                              |
 |           R21        R22         R23         |
 +---------/\/\/-------------------/\/\/--------+ S2
 |                                              |
 |           R31        R32         R33         |
 +---------/\/\/-------------------/\/\/--------+ S3
 |                                              |
 |           R41        R42         R43         |
 +---------/\/\/-------------------/\/\/--------+ S4
 |                                              |
 |           R51        R52         R53         |
 +---------/\/\/-------------------/\/\/--------+ S5
 |                                              |
 |           R61        R62         R63         |
 +---------/\/\/-------------------/\/\/--------+ S6
 |                                              |
 |                                              |
 |      RS        | | | |                       |
 +----/\/\/------||||||||-----------------------+
                  | | | |
		    V0, Imax

The power supply is a constant voltage supply with a limited
maximum current.   Each row of three resistors represents a
system.  R*1 is a variable resistor controlled by crewmembers
and specific to a device, e.g. Warp power setting, or phaser
power setting.   R*2 is a limit on R*1 controlled by engineering
to control power consumption of a system, R*1 is not allowed to
be a value below R*2, so engineering can limit the current consumed
by a device.   R*3 is the innate resistance of the device.
Some devices may have a constant R*1 (not user controllable), and
then it is only engineering which controls the power consumption
of the device.  r*3 is typically a constant.


**************************************************************/

struct power_model;

struct power_device;

typedef float (*resistor_sample_fn)(void * cookie);

struct power_device *new_power_device(void *cookie, resistor_sample_fn r1, resistor_sample_fn r2, resistor_sample_fn r3);
struct power_model *new_power_model(float max_current, float voltage,
					float internal_resistance);
void power_model_add_device(struct power_model *m, struct power_device *device);
void power_model_compute(struct power_model *m);
float device_current(struct power_device *d);
float device_max_current(struct power_device *d);
float power_model_total_current(struct power_model *m);
struct power_device *power_model_get_device(struct power_model *m, int i);
void free_power_model(struct power_model *m);
float power_model_nominal_voltage(struct power_model *m);
float power_model_actual_voltage(struct power_model *m);

#endif

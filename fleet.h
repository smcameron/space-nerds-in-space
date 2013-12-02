#ifndef FLEET_H__
#define FLEET_H__

#define FLEET_LINE 0
#define FLEET_TRIANGLE 1
#define FLEET_SQUARE 2

struct fleet;

union vec3 fleet_position(int fleet_number, int position, union quat *fleet_orientation);

int fleet_new(int fleet_shape, int32_t leader);

#endif


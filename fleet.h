#ifndef FLEET_H__
#define FLEET_H__

#define FLEET_LINE 0
#define FLEET_TRIANGLE 1
#define FLEET_SQUARE 2

union vec3 fleet_position(int fleet_shape, int position, union quat *fleet_orientation);

#endif


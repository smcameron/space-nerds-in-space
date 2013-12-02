#ifndef FLEET_H__
#define FLEET_H__

#define FLEET_LINE 0
#define FLEET_TRIANGLE 1
#define FLEET_SQUARE 2

struct fleet;

union vec3 fleet_position(int fleet_number, int position, union quat *fleet_orientation);

int fleet_new(int fleet_shape, int32_t leader);

void fleet_leave(int32_t id);

int fleet_count(void);

uint32_t fleet_member_get_id(int fleet_number, int position);

int fleet_join(int fleet_number, int32_t id);

int fleet_position_number(int fleet_number, int32_t id);

int32_t fleet_get_leader_id(int fleet_number);
#endif


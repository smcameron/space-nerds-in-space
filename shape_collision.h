#ifndef SHAPE_COLLISION_H__
#define SHAPE_COLLISION_H__

#include "quat.h"
#include "oriented_bounding_box.h"
#include "math.h"

struct shape {
	__extension__ union {
		struct shape_sphere {
			float radius;
		} sphere;
		struct shape_capsule_data {
			float radius;
			float length;
		} capsule;
		struct shape_cuboid_data {
			double sx, sy, sz; /* scale in x, y, z */
			struct oriented_bounding_box obb;
		} cuboid;
	};
	float overall_radius;
	uint8_t type;
#define SHAPE_CUBOID 0
#define SHAPE_SPHERE 1
#define SHAPE_CAPSULE 2
};
		
/* Given point point, and a shape s at position shape_position, and orientation
 * orientation, find the
 * closest point on the surface of shape to point, (returned in *closest_point)
 * and the square of the distance is returned. For capsules and spheres the
 * normal_vector is filled in. TODO: Fill in the normal vector for cuboids too.
 * Note: for cuboids, orientation is ignored, and instead orientation is instead
 * contained in shape->cuboid.obb.  For spheres orientation is of course irrelevant. 
 */
float shape_closest_point(union vec3 *point, union vec3 *shape_position,
					union quat *orientation,
					struct shape *s,
					union vec3 *closest_point,
					union vec3 *normal_vector);

void shape_init_sphere(struct shape *s, double radius);
void shape_init_capsule(struct shape *s, double length, double radius);
void shape_init_cuboid(struct shape *s, double sx, double sy, double sz);

#endif

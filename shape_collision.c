#include "shape_collision.h"

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
					union vec3 *normal_vector)
{
	union vec3 heretothere, p1, p2;
	float dist;

	switch (s->type) {
	case SHAPE_SPHERE:
		/* Here we have simple sphere collision detection, because in add_block_object(),
		 * we forced axial scaling factors to be equal, (i.e. we forced
		 * (tsd.block.sx == tsd.block.sy == tsd.block.sz) to be true.)
		 */
		vec3_sub(&heretothere, point, shape_position);
		vec3_normalize_self(&heretothere);
		*normal_vector = heretothere;
		vec3_mul_self(&heretothere, s->sphere.radius);
		vec3_add(closest_point, &heretothere, shape_position);
		vec3_sub(&heretothere, closest_point, point);
		return vec3_magnitude2(&heretothere);
	case SHAPE_CUBOID:
		/* TODO: fill in the normal_vector */
		oriented_bounding_box_closest_point(point, &s->cuboid.obb, closest_point);
		vec3_sub(&heretothere, closest_point, point);
		return vec3_magnitude2(&heretothere);
	case SHAPE_CAPSULE:
		p1.v.x = 0.5 * s->capsule.length;
		p1.v.y = 0.0;
		p1.v.z = 0.0;
		p2.v.x = -0.5 * s->capsule.length;
		p2.v.y = 0;
		p2.v.z = 0;
		quat_rot_vec_self(&p1, orientation);
		quat_rot_vec_self(&p2, orientation);
		vec3_add_self(&p1, shape_position);
		vec3_add_self(&p2, shape_position);
		dist = dist2_from_point_to_line_segment(point, &p1, &p2, closest_point);
		p2 = *closest_point;
		vec3_sub(&p1, point, closest_point);
		vec3_normalize_self(&p1);
		vec3_mul_self(&p1, s->capsule.radius);
		vec3_add_self(closest_point, &p1);
		vec3_sub(normal_vector, closest_point, &p2);
		vec3_normalize_self(normal_vector);
		return dist;
		break;
	default:
		break;
	}
	return 0.0;
}


/*
   quaternion library - interface

   Copyright (C) 2013 Tobias Simon
   most of the code was stolen from the Internet

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
*/

#ifndef __QUAT_H__
#define __QUAT_H__

#include "mtwist.h"

/* generic 2d vector */
union vec2 {
	struct {
		float x;
		float y;
	} v;
	float vec[2];
};
#define VEC2_INITIALIZER { { 0.0, 0.0 } }

/* generic 3d vector */
union vec3 {
	struct {
		float x;
		float y;
		float z;
	} v;
	float vec[3];
};
#define VEC3_INITIALIZER { { 0.0, 0.0, 0.0 } }

/* quaternion */
union quat {
	struct {
		float q0;
		float q1;
		float q2;
		float q3;
	} q;
	struct {
		float w;
		float x;
		float y;
		float z;
	} v;
	float vec[4];
};
#define IDENTITY_QUAT_INITIALIZER { { 1.0, 0.0, 0.0, 0.0 } }
extern const union quat identity_quat;

/* euler angle */
union euler {
	struct {
		float yaw;
		float pitch;
		float roll;
	} a;
	float vec[3];
};

/* initialize with (x,y,z) */
void vec3_init(union vec3 *vo, float x, float y, float z);

/* copy vector vi to vo */
union vec3 *vec3_copy(union vec3 *restrict vo, const union vec3 *restrict vi);

float vec3_cwise_min(const union vec3 *v);

float vec3_cwise_max(const union vec3 *v);

/* vo = v1 + v2, return vo */
union vec3 *vec3_add(union vec3 *vo, const union vec3 *v1, const union vec3 *v2);

/* v1 = v1 + v2, return v1 */
union vec3 *vec3_add_self(union vec3 *v1, const union vec3 *restrict v2);

/* v1 = v1 + (x,y,z), return v1 */
union vec3 *vec3_add_c_self(union vec3 *v1, float x, float y, float z);

/* vo = v1 - v2, return vo */
union vec3 *vec3_sub(union vec3 *vo, const union vec3 *v1, const union vec3 *v2);

/* v1 = v1 - v2, return v1 */
union vec3 *vec3_sub_self(union vec3 *v1, const union vec3 *v2);
        
/* v1 = v1 - (x,y,z), return v1 */
union vec3 *vec3_sub_c_self(union vec3 *v1, float x, float y, float z);

/* Hadamard or component wise product of two vectors, vo = v1 * v2 */
union vec3 *vec3_cwise_product(union vec3 *vo, const union vec3 *v1, const union vec3 *v2);

/* Hadamard or component wise product of two vectors, vo *= vi */
union vec3 *vec3_cwise_product_self(union vec3 *vo, const union vec3 *vi);

/* vo = vi * scalar, return vo */
union vec3 *vec3_mul(union vec3 *vo, const union vec3 *vi, float scalar);

/* vi *= scalar, return vi */
union vec3 *vec3_mul_self(union vec3 *vi, float scalar);

/* vo = vi / scalar, return vo */
union vec3 *vec3_div(union vec3 *vo, const union vec3 *vi, float scalar);

/* vi /= scalar, return vi */
union vec3 *vec3_div_self(union vec3 *vi, float scalar);

/* dot product */
float vec3_dot(const union vec3 *v1, const union vec3 *v2);

/* cross product vo = v1 X v2, return vo */
union vec3 *vec3_cross(union vec3 *vo, const union vec3 *v1, const union vec3 *v2);

union vec3 *vec3_normalize(union vec3 *vo, const union vec3 *vi);

union vec3 *vec3_normalize_self(union vec3 *vo);

/* vec3 rotate by axis and angle */
union vec3 *vec3_rot_axis(union vec3 *vo, union vec3 *vi, float x, float y, float z, float angle);

/* vec3 rotate self by axis and angle */
union vec3 *vec3_rot_axis_self(union vec3 *vo, float x, float y, float z, float angle);

double vec3_dist(const union vec3 *v1, const union vec3 *v2);
double vec3_dist_c(const union vec3 *v1, float x, float y, float z);

float vec3_dist_sqrd(const union vec3 *v1, const union vec3 *v2);

void vec3_print(const char* prefix, const union vec3 *v);

/* init orientation quaternion from measurements */
void quaternion_init(union quat *quat, const union vec3 *acc, const union vec3 *mag);

/* initialize quaternion from axis angle using floats */
void quat_init_axis(union quat *q, float x, float y, float z, float a);

/* initialize quaternion from axis angle using a vector */
void quat_init_axis_v(union quat *q, const union vec3 *v, float a);

/* decompose a quaternion into an axis angle using floats */
void quat_to_axis(const union quat *restrict q,
		float *restrict x, float *restrict y, float *restrict z, float *restrict a);

/* decompose a quaternion into an axis angle using vector */
void quat_to_axis_v(const union quat *restrict q, union vec3 *restrict v, float *restrict a);

/* quaternion dot product q1 . q2 */
float quat_dot(const union quat *q1, const union quat *q2);

/* rotate vector vi via unit quaternion q and put result into vector vo */
void quat_rot_vec(union vec3 *restrict vo, const union vec3 *restrict vi, const union quat *restrict q);

/* rotate vector v_in in-place via unit quaternion quat */
void quat_rot_vec_self(union vec3 *restrict v, const union quat *restrict q);

/* returns len of quaternion */
float quat_len(const union quat *q);

/* copy quaternion qi to qo */
void quat_copy(union quat *restrict qo, const union quat *restrict qi);

/* qo = qi * f */
void quat_scale(union quat *qo, const union quat *qi, float f);

/* qo *= f */
void quat_scale_self(union quat *q, float f);

/* Change a quaternion's coordinate system */
union quat *quat_conjugate(union quat *restrict qo, union quat *restrict rotation,
				union quat *restrict new_coord_system);

/* Compute the inverse of a unit quaternion */
void quat_inverse(union quat *restrict qo, const union quat *restrict qi);

/* o = q1 + q2 */
void quat_add(union quat *restrict qo, const union quat *restrict q1, const union quat *restrict q2);

/* o += q */
void quat_add_self(union quat *restrict o, const union quat *restrict q);

/* o = q1 * q2 */
void quat_mul(union quat *o, const union quat *q1, const union quat *q2);

/* q = q * qi */
void quat_mul_self(union quat *q, const union quat *qi);

/* q = qi * q */
void quat_mul_self_right(const union quat *qi, union quat *q);

/* normalizes quaternion q and puts result into o */
void quat_normalize(union quat *qo, const union quat *qi);

/* normalize q in-place */
void quat_normalize_self(union quat *q);

/* convert quaternion to euler angles */
void quat_to_euler(union euler *e, const union quat *q);

/* return angles
   heading as angle around y axis with zero at {1,0,0), positive toward -z, 0 to 2pi
   mark as angle from xz plane with zero at xz plane, positive toward +y, pi/2 to -pi/2 */
union vec3* heading_mark_to_vec3(float r, double heading, double mark, union vec3 *dir);
void vec3_to_heading_mark(const union vec3 *restrict dir,
		double *restrict r, double *restrict heading, double *restrict mark);
void quat_to_heading_mark(const union quat *restrict q, double *restrict heading, double *restrict mark);

/* normalize angle */
float normalize_euler_0_2pi(float a);

/* m is pointer to array of 16 floats in column major order */
void quat_to_lh_rot_matrix(const union quat *restrict q, float *restrict m); /* quat to left handed rot matrix */
void quat_to_rh_rot_matrix(const union quat *restrict q, float *restrict m); /* quat to right handed rot matrix */
void quat_to_rh_rot_matrix_fd(const union quat *restrict q, double *restrict m); /* quat to right handed rot matrix */

/* Create a random quaternion */
void random_quat(union quat *q);
/* Create a random quaternion axis with specified rotation */
void random_axis_quat(union quat *q, float angle);
void consistent_random_axis_quat(struct mtwist_state *restrict mt, union quat *restrict q, float angle);

/* returns square of the length of a vector */
float vec3_len2(const union vec3 *v);

/* Calculate the quaternion to rotate from vector u to vector v */
void quat_from_u2v(union quat *restrict q, const union vec3 *restrict u,
	const union vec3 *restrict v, const union vec3 *restrict up);

/* Calculate the quaternion to rotate from unit_vector u to unit_vector v */
void quat_from_unit_u2v(union quat *restrict q, union vec3 *restrict u, union vec3 *restrict v);

/* calculate normalized linear quaternion interpolation */
union quat* quat_nlerp(union quat *qo, const union quat *qfrom, const union quat *qto, float t);

/* calculate spherical quaternion interpolation */
union quat* quat_slerp(union quat *qo, const union quat *qfrom, const union quat *qto, float t);

/* calculate vec3 linear interpolation */
union vec3 *vec3_lerp(union vec3 *vo,
	const union vec3 *vfrom, const union vec3 *vto, double t);

/* Apply incremental yaw, pitch and roll relative to the quaternion.
 * For example, if the quaternion represents an orientation of a ship,
 * this will apply yaw/pitch/roll *in the ship's local coord system* to the
 * orientation.
 */
union quat *quat_apply_relative_yaw_pitch_roll(union quat *q,
                                        double yaw, double pitch, double roll);

/* Apply incremental yaw and pitch relative to the quaternion.
 * Yaw is applied to world axis so no roll will accumulate */
union quat *quat_apply_relative_yaw_pitch(union quat *q, double yaw, double pitch);

/* decompose a quaternion into a rotation (swing) perpendicular to v1 and a rotation (twist) around v1 */
/* For example, if you have a turret mounted horizontally that can rotate around a vertical axis, and has */
/* another horizontal axis, then twist corresponds to azimuth, and swing corresponds to elevation. */
void quat_decompose_twist_swing(const union quat *restrict q, const union vec3 *restrict v1,
		union quat *restrict twist, union quat *restrict swing);
void quat_decompose_swing_twist(const union quat *restrict q,
		const union vec3 *restrict v1, union quat *restrict swing, union quat *restrict twist);

int sphere_line_segment_intersection(const union vec3 *v0, const union vec3 *v1, const union vec3 *center, double r, union vec3 *vo0, union vec3 *vo1);

void plane_vector_u_and_v_from_normal(union vec3 *u, union vec3 *v, const union vec3 *n);
void random_point_in_3d_annulus(float r1, float r2, const union vec3 *center, const union vec3 *u, const union vec3 *v, union vec3 *point);

/* Returns the square of the magnitude (length) of v */
float vec3_magnitude2(union vec3 *v);

/* Returns the magnitude (length) of v  */
float vec3_magnitude(union vec3 *v);

int ray_intersects_sphere(const union vec3 *ray_origin,
				const union vec3 *ray_direction,
				const union vec3 *sphere_origin,
				const float radius);

/* Returns distance from point to plane defined by plane_point and plane_normal */
float plane_to_point_dist(const union vec3 plane_point, const union vec3 plane_normal,
			const union vec3 point);

/* moving_spheres_intersection() returns true if moving spheres intersect, and fills
 * in *time at which intersection occurs if time_horizon < 0 or
 * if *time < time_horizon. IOW, negative time_horizon == no time horizon.
 * if *time >= time_horizon && time_horizon >= 0 and there is an intersection,
 * then *time is filled in, otherwise, *time is not touched.
 */
int moving_spheres_intersection(union vec3 *s1, float r1, union vec3 *v1,
				union vec3 *s2, float r2, union vec3 *v2,
				float time_horizon, float *time);

/* For the +z face of a cubemapped unit sphere, returns tangent and bitangent vectors */
void cubemapped_sphere_tangent_and_bitangent(float x, float y, union vec3 *u, union vec3 *v);

/* Returns the square of the distance between a point p, and the line segment formed by
 * seg1 and seg2, and the nearest point on the line.
 */
float dist2_from_point_to_line_segment(union vec3 *p, union vec3 *seg1, union vec3 *seg2,
	union vec3 *nearest_point);

/* Returns distance from the given point to surface of a torus at the origin
 * with given major and minor radius. The torus is assumed to be in the y,z plane,
 * so if you were to fly down the x axis, you would fly through the doughnut hole.
 * From http://iquilezles.org/www/articles/distfunctions/distfunctions.htm
 */
float point_to_torus_dist(const union vec3 * const point, const float major_radius, const float minor_radius);

/***********************************************************************************************

Some Notes About Quaternions
----------------------------

A quaternion q is a 4 dimensional quantity:

		q = q0 + q1i + q2j + q3k

	with i, j, k representing orthogonal basis vectors, and also,

		i^2 = j^2 = k^2 = -1
		ij = -ji = k
		jk = -kj = i
		ki = -ik = j

Quaternion multiplication:

	r = pq

	r0 = p0q0 - p1q1 - p2q2 - p3q3
	r1 = p0q1 + p1q0 + p2q3 - p3q2
	r2 = p0q2 + p2q0 - p1q3 + p3q1
	r3 = p0q3 + p3q0 + p1q2 - p2q1

	r = r0 + r1i + r2j + r3k

Magnitude of a quaternion, |q|, is computed:

	|q| = (q0^2 + q1^2 + q2^2 + q3^2)^(0.5)

Inverse of a quaternion, q^(-1) has the property:

		qq^(1) = q^(-1)q = 1

	and is computed:

		q^(-1) = (q0 - q1 - q2 - q3) / |q|^2

	See quat_inverse().

Constructing a unit quaternion from an axis of rotation and an angle of rotation:

	If n1 + n2 + n3 is a unit vector representing an axis about which rotation occurs
	and theta is 0.5 * the angle of the rotation about this axis, then a unit quaternion
	representing that rotation can be written:

		cos(theta) + n1 * sin(theta) + n2 * sin(theta) + n3 * sin(theta)

To rotate a vector v, by quaternion q:

	qvq^(-1)

Rotations may be composed by quaternion multiplication:

	Let R1 and R2 be quaternions representing two rotations performed in
	order, R1, then R2.  The composite rotation quaternion R is:

		R = R2R1		(notice the order is reversed)

	The order is important as quaternion multiplication is not commutative.

Rotations may be divided or multiplied by quaternion powers:

	if q represents a rotation of x around some axis, then q^2 represents a rotation of 2x,
	and q^0.5 represents a rotation of 0.5x arount that axis.

		q^x = |q|^(x) * ( cos(x * theta) + n1 * sin(x * theta) + n2 * sin(x * theta) + n3 * sin(x * theta)).

	Note that if x is -1, then this reduces to the inversion.

Changing the coordinate system of a quaternion:

	Suppose the following:

	1. You have some canonical defined orientation that is consired "zero rotation", ie.
	   "facing down the positive x axis with "up" being up the postitive y axis."

	2. You have a spaceship whose current orientation is defined as a quaternion representing
	   the rotation from this zero rotation, call it r.

	3. You have a rotation which you want to apply to the spaceship in its orientation.  Let's
	   say you want to yaw 5 degrees left.  So you construct a quaternion, q, to represent a 5
	   degrees left yaw from "zero rotation".

	To apply q to r, to get a new orientation quaternion, n, we can do something analogous to rotating a
	vector by q.

	n = rqr^(-1) = r(q0 + q)r^(-1)
	  = rq0r^(-1) + rqr^(-1)
	  = q0rr^(-1) + rqr^(-1)
	  = q0 + rqr^(-1)

	See quat_conjugate().

Main properties of quaternions:

	1. pq is not equal to qp
	2. p(q + r) = pq + pr
	3. (sp)q = p(sq) = s(pq)
	4. q^(-1) = (q0 - q) / |q|^2
	5. p(qr) = (pq)r
	6. qvq^(-1) rotates a vector by q
	7. pq composes two rotations
	8. q^x does rotation q but x times (ie q^3 does rotation q 3 times)
	9. rqr-1 rotates the rotation axis of

***********************************************************************************************/

#endif /* __QUAT_H__ */

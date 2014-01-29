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
union vec3 *vec3_copy(union vec3 *vo, const union vec3 *vi);

/* vo = v1 + v2, return vo */
union vec3 *vec3_add(union vec3 *vo, const union vec3 *v1, const union vec3 *v2);

/* v1 = v1 + v2, return v1 */
union vec3 *vec3_add_self(union vec3 *v1, const union vec3 *v2);

/* v1 = v1 + (x,y,z), return v1 */
union vec3 *vec3_add_c_self(union vec3 *v1, float x, float y, float z);

/* vo = v1 - v2, return vo */
union vec3 *vec3_sub(union vec3 *vo, const union vec3 *v1, const union vec3 *v2);

/* v1 = v1 - v2, return v1 */
union vec3 *vec3_sub_self(union vec3 *v1, const union vec3 *v2);
        
/* v1 = v1 - (x,y,z), return v1 */
union vec3 *vec3_sub_c_self(union vec3 *v1, float x, float y, float z);

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

void vec3_print(const char* prefix, const union vec3 *v);

/* init orientation quaternion from measurements */
void quaternion_init(union quat *quat, const union vec3 *acc, const union vec3 *mag);

/* initialize quaternion from axis angle using floats */
void quat_init_axis(union quat *q, float x, float y, float z, float a);

/* initialize quaternion from axis angle using a vector */
void quat_init_axis_v(union quat *q, const union vec3 *v, float a);

/* decompose a quaternion into an axis angle using floats */
void quat_to_axis(const union quat *q, float *x, float *y, float *z, float *a);

/* decompose a quaternion into an axis angle using vector */
void quat_to_axis_v(const union quat *q, union vec3 *v, float *a);

/* quaternion dot product q1 . q2 */
float quat_dot(const union quat *q1, const union quat *q2);

/* rotate vector vi via unit quaternion q and put result into vector vo */
void quat_rot_vec(union vec3 *vo, const union vec3 *vi, const union quat *q);

/* rotate vector v_in in-place via unit quaternion quat */
void quat_rot_vec_self(union vec3 *v, const union quat *q);

/* returns len of quaternion */
float quat_len(const union quat *q);

/* copy quaternion qi to qo */
void quat_copy(union quat *qo, const union quat *qi);

/* qo = qi * f */
void quat_scale(union quat *qo, const union quat *qi, float f);

/* qo *= f */
void quat_scale_self(union quat *q, float f);

/* conjugate quaternion */
void quat_conj(union quat *qo, const union quat *qi);

/* o = q1 + q2 */
void quat_add(union quat *qo, const union quat *q1, const union quat *q2);

/* o += q */
void quat_add_self(union quat *o, const union quat *q);

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
void vec3_to_heading_mark(const union vec3 *dir, double *r, double *heading, double *mark);
void quat_to_heading_mark(const union quat *q, double *heading, double *mark);

/* normalize angle */
float normalize_euler_0_2pi(float a);

/* m is pointer to array of 16 floats in column major order */
void quat_to_lh_rot_matrix(const union quat *q, float *m); /* quat to left handed rotation matrix */
void quat_to_rh_rot_matrix(const union quat *q, float *m); /* quat to right handed rotation matrix */
void quat_to_rh_rot_matrix_fd(const union quat *q, double *m); /* quat to right handed rotation matrix */

/* Create a random quaternion */
void random_quat(union quat *q);
/* Create a random quaternion axis with specified rotation */
void random_axis_quat(union quat *q, float angle);
void consistent_random_axis_quat(struct mtwist_state *mt, union quat *q, float angle);

/* returns square of the length of a vector */
float vec3_len2(const union vec3 *v);

/* Calculate the quaternion to rotate from vector u to vector v */
void quat_from_u2v(union quat *q, const union vec3 *u, const union vec3 *v, const union vec3 *up);

/* Calculate the quaternion to rotate from unit_vector u to unit_vector v */
void quat_from_unit_u2v(union quat *q, union vec3 *u, union vec3 *v);

/* calculate normalized linear quaternion interpolation */
union quat* quat_nlerp(union quat *qo, const union quat *qfrom, const union quat *qto, float t);

/* calculate spherical quaternion interpolation */
union quat* quat_slerp(union quat *qo, const union quat *qfrom, const union quat *qto, float t);

/* calculate vec3 linear interpolation */
union vec3* vec3_lerp(union vec3* vo, const union vec3* vfrom, const union vec3* vto, double t);

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
void quat_decompose_twist_swing(const union quat *q, const union vec3 *v1, union quat *twist, union quat *swing);
void quat_decompose_swing_twist(const union quat *q, const union vec3 *v1, union quat *swing, union quat *twist);

int sphere_line_segment_intersection(const union vec3 *v0, const union vec3 *v1, const union vec3 *center, double r, union vec3 *vo0, union vec3 *vo1);

void plane_vector_u_and_v_from_normal(union vec3 *u, union vec3 *v, const union vec3 *n);
void random_point_in_3d_annulus(float r1, float r2, const union vec3 *center, const union vec3 *u, const union vec3 *v, union vec3 *point);

float vec3_magnitude(union vec3 *v);

int ray_intersects_sphere(const union vec3 *ray_origin,
				const union vec3 *ray_direction,
				const union vec3 *sphere_origin,
				const float radius);

#endif /* __QUAT_H__ */

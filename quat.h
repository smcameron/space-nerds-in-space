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

/* generic 3d vector */
union vec3 {
	struct {
		float x;
		float y;
		float z;
	} v;
	float vec[3];
};

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

/* euler angle */
union euler {
	struct {
		float yaw;
		float pitch;
		float roll;
	} a;
	float vec[3];
};

/* copy vector vi to vo */
void vec3_copy(union vec3 *vo, union vec3 *vi);

/* init orientation quaternion from measurements */
void quaternion_init(union quat *quat, const union vec3 *acc, const union vec3 *mag);

/* initialize quaternion from axis angle using floats */
void quat_init_axis(union quat *q, float x, float y, float z, float a);

/* initialize quaternion from axis angle using a vector */
void quat_init_axis_v(union quat *q, const union vec3 *v, float a);

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

/* o = q1 + 12 */
void quat_add(union quat *qo, const union quat *q1, const union quat *q2);

/* o += q */
void quat_add_self(union quat *o, const union quat *q);

/* o = q1 * q2 */
void quat_mul(union quat *o, const union quat *q1, const union quat *q2);

/* normalizes quaternion q and puts result into o */
void quat_normalize(union quat *qo, const union quat *qi);

/* normalize q in-place */
void quat_normalize_self(union quat *q);

/* convert quaternion to euler angles */
void quat_to_euler(union euler *e, const union quat *q);

/* normalize angle */
float normalize_euler_0_2pi(float a);

/* m is pointer to array of 16 floats in column major order */
void quat_to_rot_matrix(union quat *q, float *m);

#endif /* __QUAT_H__ */

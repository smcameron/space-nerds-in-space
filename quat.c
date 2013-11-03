/*
   quaternion library - implementation

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

#include <string.h>
#include <math.h>

#include "quat.h"

void vec3_copy(union vec3 *vo, union vec3 *vi)
{
	memcpy(vo, vi, sizeof(union vec3));   
}

void quat_init(union quat *q, const union vec3 *acc, const union vec3 *mag)
{
	float ax = acc->v.x;
	float ay = acc->v.y;
	float az = acc->v.z;
	float mx = mag->v.x;
	float my = mag->v.y;
	float mz = mag->v.z;


	float init_roll = atan2(-ay, -az);
	float init_pitch = atan2(ax, -az);

	float cos_roll = cosf(init_roll);
	float sin_roll = sinf(init_roll);
	float cos_pitch = cosf(init_pitch);
	float sin_pitch = sinf(init_pitch);

	float mag_x = mx * cos_pitch + my * sin_roll * sin_pitch + mz * cos_roll * sin_pitch;
	float mag_y = my * cos_roll - mz * sin_roll;

	float init_yaw = atan2(-mag_y, mag_x);

	cos_roll =  cosf(init_roll * 0.5f);
	sin_roll =  sinf(init_roll * 0.5f);

	cos_pitch = cosf(init_pitch * 0.5f );
	sin_pitch = sinf(init_pitch * 0.5f );

	float cosHeading = cosf(init_yaw * 0.5f);
	float sinHeading = sinf(init_yaw * 0.5f);

	q->q.q0 = cos_roll * cos_pitch * cosHeading + sin_roll * sin_pitch * sinHeading;
	q->q.q1 = sin_roll * cos_pitch * cosHeading - cos_roll * sin_pitch * sinHeading;
	q->q.q2 = cos_roll * sin_pitch * cosHeading + sin_roll * cos_pitch * sinHeading;
	q->q.q3 = cos_roll * cos_pitch * sinHeading - sin_roll * sin_pitch * cosHeading;
}

void quat_init_axis(union quat *q, float x, float y, float z, float a)
{
	/* see: http://www.euclideanspace.com/maths/geometry/rotations
	   /conversions/angleToQuaternion/index.htm */
	float a2 = a * 0.5f;
	float s = sin(a2);
	q->v.x = x * s;   
	q->v.y = y * s;   
	q->v.z = z * s;   
	q->v.w = cos(a2);   
}

void quat_init_axis_v(union quat *q, const union vec3 *v, float a)
{
	quat_init_axis(q, v->v.x, v->v.y, v->v.z, a);
}

void quat_rot_vec_self(union vec3 *v, const union quat *q)
{
	union vec3 vo;
	quat_rot_vec(&vo, v, q);
	vec3_copy(v, &vo);
}

void quat_rot_vec(union vec3 *vo, const union vec3 *vi, const union quat *q)
{
	/* see: https://github.com/qsnake/ase/blob/master/ase/quaternions.py */
	const float vx = vi->v.x, vy = vi->v.y, vz = vi->v.z;
	const float qw = q->v.w, qx = q->v.x, qy = q->v.y, qz = q->v.z;
	const float qww = qw * qw, qxx = qx * qx, qyy = qy * qy, qzz = qz * qz;
	const float qwx = qw * qx, qwy = qw * qy, qwz = qw * qz, qxy = qx * qy;
	const float qxz = qx * qz, qyz = qy * qz;
	vo->v.x = (qww + qxx - qyy - qzz) * vx + 2 * ((qxy - qwz) * vy + (qxz + qwy) * vz);
	vo->v.y = (qww - qxx + qyy - qzz) * vy + 2 * ((qxy + qwz) * vx + (qyz - qwx) * vz);
	vo->v.z = (qww - qxx - qyy + qzz) * vz + 2 * ((qxz - qwy) * vx + (qyz + qwx) * vy);
}

void quat_copy(union quat *qo, const union quat *qi)
{
	memcpy(qo, qi, sizeof(union quat));   
}

float quat_len(const union quat *q)
{
	float s = 0.0f;

	for (int i = 0; i < 4; ++i)
		s += q->vec[i] * q->vec[i];
	return sqrtf(s);
}

void quat_conj(union quat *q_out, const union quat *q_in)
{
	q_out->v.x = -q_in->v.x;
	q_out->v.y = -q_in->v.y;
	q_out->v.z = -q_in->v.z;
	q_out->v.w = q_in->v.w;
}

void quat_to_euler(union euler *euler, const union quat *quat)
{
	const float x = quat->v.x, y = quat->v.y, z = quat->v.z, w = quat->v.w;
	const float ww = w * w, xx = x * x, yy = y * y, zz = z * z;
	euler->a.yaw = normalize_euler_0_2pi(atan2f(2.f * (x * y + z * w), xx - yy - zz + ww));
	euler->a.pitch = asinf(-2.f * (x * z - y * w));
	euler->a.roll = atan2f(2.f * (y * z + x * w), -xx - yy + zz + ww);
}

void quat_mul(union quat *o, const union quat *q1, const union quat *q2)
{
	/* see: http://www.euclideanspace.com/maths/algebra/
	   realNormedAlgebra/quaternions/code/index.htm#mul */
	o->v.x =  q1->v.x * q2->v.w + q1->v.y * q2->v.z - q1->v.z * q2->v.y + q1->v.w * q2->v.x;
	o->v.y = -q1->v.x * q2->v.z + q1->v.y * q2->v.w + q1->v.z * q2->v.x + q1->v.w * q2->v.y;
	o->v.z =  q1->v.x * q2->v.y - q1->v.y * q2->v.x + q1->v.z * q2->v.w + q1->v.w * q2->v.z;
	o->v.w = -q1->v.x * q2->v.x - q1->v.y * q2->v.y - q1->v.z * q2->v.z + q1->v.w * q2->v.w;
}

void quat_add(union quat *o, const union quat *q1, const union quat *q2)
{
	/* see: http://www.euclideanspace.com/maths/algebra/
	   realNormedAlgebra/quaternions/code/index.htm#add */
	o->v.x = q1->v.x + q2->v.x;
	o->v.y = q1->v.y + q2->v.y;
	o->v.z = q1->v.z + q2->v.z;
	o->v.w = q1->v.w + q2->v.w;
}

void quat_add_to(union quat *o, const union quat *q)
{
	union quat tmp;

	quat_add(&tmp, o, q);
	quat_copy(o, &tmp);
}

void quat_scale(union quat *o, const union quat *q, float f)
{
	/* see: http://www.euclideanspace.com/maths/algebra/
	   realNormedAlgebra/quaternions/code/index.htm#scale*/
	for (int i = 0; i < 4; ++i)
		o->vec[i] = q->vec[i] * f;
}

void quat_scale_self(union quat *q, float f)
{
	quat_scale(q, q, f);
}

void quat_normalize(union quat *o, const union quat *q)
{
	/* see: http://www.euclideanspace.com/maths/algebra/
	   realNormedAlgebra/quaternions/code/index.htm#normalise */
	quat_scale(o, q, 1.0f / quat_len(q));
}

void quat_normalize_self(union quat *q)
{
	quat_normalize(q, q);
}

float normalize_euler_0_2pi(float a)
{
	while (a < 0)
		a += (float) (2 * M_PI);
	return a;
}

/* m is pointer to array of 16 floats in column major order */
void quat_to_lh_rot_matrix(union quat *q, float *m)
{
	union quat qn;
	float qw, qx, qy, qz;

	quat_normalize(&qn, q);

	qw = qn.v.w;
	qx = qn.v.x;
	qy = qn.v.y;
	qz = qn.v.z;

	m[0] = 1.0f - 2.0f * qy * qy - 2.0f * qz * qz;
	m[1] = 2.0f * qx * qy + 2.0f * qz * qw;
	m[2] = 2.0f * qx * qz - 2.0f * qy * qw;
	m[3] = 0.0f;

	m[4] = 2.0f * qx * qy - 2.0f * qz * qw;
	m[5] = 1.0f - 2.0f * qx * qx - 2.0f * qz * qz;
	m[6] = 2.0f * qy * qz + 2.0f * qx * qw;
	m[7] = 0.0f;

	m[8] = 2.0f * qx * qz + 2.0f * qy * qw;
	m[9] = 2.0f * qy * qz - 2.0f * qx * qw;
	m[10] = 1.0f - 2.0f * qx * qx - 2.0f * qy * qy;
	m[11] = 0.0f;

	m[12] = 0.0f;
	m[13] = 0.0f;
	m[14] = 0.0f;
	m[15] = 1.0f;
}

void quat_to_rh_rot_matrix(union quat *q, float *m)
{
	union quat qn;
	float qw, qx, qy, qz;

	quat_normalize(&qn, q);

	qw = qn.v.w;
	qx = qn.v.x;
	qy = qn.v.y;
	qz = qn.v.z;

	m[0] = 1.0f - 2.0f * qy * qy - 2.0f * qz * qz;
	m[1] = 2.0f * qx * qy - 2.0f * qz * qw;
	m[2] = 2.0f * qx * qz + 2.0f * qy * qw;
	m[3] = 0.0f;

	m[4] = 2.0f * qx * qy + 2.0f * qz * qw;
	m[5] = 1.0f - 2.0f * qx * qx - 2.0f * qz * qz;
	m[6] = 2.0f * qy * qz - 2.0f * qx * qw;
	m[7] = 0.0f;

	m[8] = 2.0f * qx * qz - 2.0f * qy * qw;
	m[9] = 2.0f * qy * qz + 2.0f * qx * qw;
	m[10] = 1.0f - 2.0f * qx * qx - 2.0f * qy * qy;
	m[11] = 0.0f;

	m[12] = 0.0f;
	m[13] = 0.0f;
	m[14] = 0.0f;
	m[15] = 1.0f;
}

const union quat identity_quat = { {0.0, 0.0, 0.0, 1.0} };


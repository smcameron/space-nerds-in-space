/*
   quaternion library - implementation

   Copyright (C) 2013 Tobias Simon
   Portions Copyright (C) 2014 Jeremy Van Grinsven
   Portions Copyright (C) 2018 Stephen M. Cameron

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
#include <stdio.h>

#include "mtwist.h"
#include "quat.h"
#include "mathutils.h"

static const float ZERO_TOLERANCE = 0.000001f;

union vec3* vec3_copy(union vec3 *vo, const union vec3 *vi)
{
	memcpy(vo, vi, sizeof(union vec3));
	return vo;
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

	cos_roll = cosf(init_roll * 0.5f);
	sin_roll = sinf(init_roll * 0.5f);

	cos_pitch = cosf(init_pitch * 0.5f);
	sin_pitch = sinf(init_pitch * 0.5f);

	float cos_heading = cosf(init_yaw * 0.5f);
	float sin_heading = sinf(init_yaw * 0.5f);

	q->q.q0 = cos_roll * cos_pitch * cos_heading + sin_roll * sin_pitch * sin_heading;
	q->q.q1 = sin_roll * cos_pitch * cos_heading - cos_roll * sin_pitch * sin_heading;
	q->q.q2 = cos_roll * sin_pitch * cos_heading + sin_roll * cos_pitch * sin_heading;
	q->q.q3 = cos_roll * cos_pitch * sin_heading - sin_roll * sin_pitch * cos_heading;
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

void quat_to_axis(const union quat *q, float *x, float *y, float *z, float *a)
{
	/* see: http://www.euclideanspace.com/maths/geometry/rotations
	   /conversions/quaternionToAngle/index.htm */
	float angle = 2 * acos(q->v.w);
	float s = sqrt(1.0 - q->v.w * q->v.w);
	if (s < ZERO_TOLERANCE) {
		/* if s close to zero then direction of axis not important */
		*a = 0;
		*x = 1;
		*y = 0;
		*z = 0;
	} else {
		*a = angle;
		*x = q->v.x / s; /* normalise axis */
		*y = q->v.y / s;
		*z = q->v.z / s;
	}
}

void quat_to_axis_v(const union quat *q, union vec3 *v, float *a)
{
	quat_to_axis(q, &v->v.x, &v->v.y, &v->v.z, a);
}

float quat_dot(const union quat *q1, const union quat *q2)
{
	return q1->vec[0] * q2->vec[0] + q1->vec[1] * q2->vec[1] + q1->vec[2] * q2->vec[2] + q1->vec[3] * q2->vec[3];
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
	if (qo != qi)
		memcpy(qo, qi, sizeof(union quat));
}

float quat_len(const union quat *q)
{
	float s = 0.0f;

	for (int i = 0; i < 4; ++i)
		s += q->vec[i] * q->vec[i];
	return sqrtf(s);
}

void quat_inverse(union quat *q_out, const union quat *q_in)
{
	q_out->v.x = -q_in->v.x;
	q_out->v.y = -q_in->v.y;
	q_out->v.z = -q_in->v.z;
	q_out->v.w = q_in->v.w;
}

union quat *quat_conjugate(union quat *qo, union quat *rotation, union quat *new_coord_system)
{
	union quat temp, inverse;

	/* Convert rotation to new coordinate system */
	quat_mul(&temp, new_coord_system, rotation);
	quat_inverse(&inverse, new_coord_system);
	quat_mul(qo, &temp, &inverse);
	return qo;
}

union vec3* heading_mark_to_vec3(float r, double heading, double mark, union vec3 *dir)
{
	dir->v.x = r*cos(mark)*cos(heading);
	dir->v.y = r*sin(mark);
	dir->v.z = -r*cos(mark)*sin(heading);
	return dir;
}

/* heading is around y from x at zero torwards -z, heading is up/down from xz plane */
void vec3_to_heading_mark(const union vec3 *dir, double *r, double *heading, double *mark)
{
	*heading = normalize_euler_0_2pi(atan2(-dir->v.z,dir->v.x));
	float dist = sqrt(dir->v.x*dir->v.x + dir->v.y*dir->v.y + dir->v.z*dir->v.z);
	if (dist < ZERO_TOLERANCE)
		*mark = 0;
	else
		*mark = asin(dir->v.y / dist);
	if (r)
		*r = dist;
}

void quat_to_heading_mark(const union quat *q, double *heading, double *mark)
{
	union vec3 dir = {{1,0,0}};
	quat_rot_vec_self(&dir, q);
	vec3_to_heading_mark(&dir, 0, heading, mark);
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

/* q = q * qi */
void quat_mul_self(union quat *q, const union quat *qi)
{
	union quat qo;
	quat_mul(&qo, q, qi);
	*q = qo;
}

/* q = qi * q */
void quat_mul_self_right(const union quat *qi, union quat *q)
{
	union quat qo;
	quat_mul(&qo, qi, q);
	*q = qo;
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

/* m is pointer to array of 16 floats in row major order */
void quat_to_rh_rot_matrix(const union quat *q, float *m)
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

/* m is pointer to array of 16 doubles in row major order */
void quat_to_rh_rot_matrix_fd(const union quat *q, double *m)
{
	union quat qn;
	double qw, qx, qy, qz;

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


void quat_to_lh_rot_matrix(const union quat *q, float *m)
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

const union quat identity_quat = IDENTITY_QUAT_INITIALIZER;

void random_quat(union quat *q)
{
	float angle;
	union vec3 v;

	random_point_on_sphere(1.0, &v.v.x, &v.v.y, &v.v.z);
	angle = (float) snis_randn(360) * M_PI / 180.0;
	quat_init_axis_v(q, &v, angle);

}

void random_axis_quat(union quat *q, float angle)
{
	union vec3 v;

	random_point_on_sphere(1.0, &v.v.x, &v.v.y, &v.v.z);
	quat_init_axis_v(q, &v, angle);
}

void consistent_random_axis_quat(struct mtwist_state *mt, union quat *q, float angle)
{
	union vec3 v;

	consistent_random_point_on_sphere(mt, 1.0, &v.v.x, &v.v.y, &v.v.z);
	quat_init_axis_v(q, &v, angle);
}

void vec3_init(union vec3 *vo, float x, float y, float z)
{
	vo->v.x = x;
	vo->v.y = y;
	vo->v.z = z;
}

float vec3_cwise_min(const union vec3 *v)
{
	return fminf(v->v.x, fminf(v->v.y, v->v.z));
}

float vec3_cwise_max(const union vec3 *v)
{
	return fmaxf(v->v.x, fmaxf(v->v.y, v->v.z));
}

union vec3* vec3_add(union vec3 *vo, const union vec3 *v1, const union vec3 *v2)
{
	vo->vec[0] = v1->vec[0] + v2->vec[0];
	vo->vec[1] = v1->vec[1] + v2->vec[1];
	vo->vec[2] = v1->vec[2] + v2->vec[2];
	return vo;
}

union vec3* vec3_add_self(union vec3 *v1, const union vec3 *v2)
{
	return vec3_add(v1, v1, v2);
}

union vec3* vec3_add_c_self(union vec3 *v1, float x, float y, float z)
{
	v1->v.x += x;
	v1->v.y += y;
	v1->v.z += z;
	return v1;
}

union vec3* vec3_sub(union vec3 *vo, const union vec3 *v1, const union vec3 *v2)
{
	vo->vec[0] = v1->vec[0] - v2->vec[0];
	vo->vec[1] = v1->vec[1] - v2->vec[1];
	vo->vec[2] = v1->vec[2] - v2->vec[2];
	return vo;
}

union vec3* vec3_sub_self(union vec3 *v1, const union vec3 *v2)
{
	return vec3_sub(v1, v1, v2);
}

union vec3* vec3_sub_c_self(union vec3 *v1, float x, float y, float z)
{
	v1->v.x -= x;
	v1->v.y -= y;
	v1->v.z -= z;
	return v1;
}

/* Hadamard product or component wise product of two vectors */
union vec3 *vec3_vec3_cwise_product(union vec3 *vo, const union vec3 *v1, const union vec3 *v2)
{
	vo->vec[0] = v1->vec[0] * v2->vec[0];
	vo->vec[1] = v1->vec[1] * v2->vec[1];
	vo->vec[2] = v1->vec[2] * v2->vec[2];
	return vo;
}

union vec3 *vec3_cwise_product_self(union vec3 *vo, const union vec3 *vi)
{
	vec3_vec3_cwise_product(vo, vo, vi);
	return vo;
}

union vec3* vec3_mul(union vec3 *vo, const union vec3 *vi, float scalar)
{
	vo->vec[0] = vi->vec[0] * scalar;
	vo->vec[1] = vi->vec[1] * scalar;
	vo->vec[2] = vi->vec[2] * scalar;
	return vo;
}

union vec3* vec3_mul_self(union vec3 *vi, float scalar)
{
	return vec3_mul(vi, vi, scalar);
}

union vec3 *vec3_div(union vec3 *vo, const union vec3 *vi, float scalar)
{
	vo->vec[0] = vi->vec[0] / scalar;
	vo->vec[1] = vi->vec[1] / scalar;
	vo->vec[2] = vi->vec[2] / scalar;
	return vo;
}

union vec3 *vec3_div_self(union vec3 *vi, float scalar)
{
	return vec3_div(vi, vi, scalar);
}

float vec3_dot(const union vec3 *v1, const union vec3 *v2)
{
	return v1->vec[0] * v2->vec[0] + v1->vec[1] * v2->vec[1] + v1->vec[2] * v2->vec[2];
}

union vec3* vec3_cross(union vec3 *vo, const union vec3 *v1, const union vec3 *v2)
{
	vo->vec[0] = v1->vec[1]*v2->vec[2] - v1->vec[2]*v2->vec[1];
	vo->vec[1] = v1->vec[2]*v2->vec[0] - v1->vec[0]*v2->vec[2];
	vo->vec[2] = v1->vec[0]*v2->vec[1] - v1->vec[1]*v2->vec[0];
	return vo;
}

/* returns square of the length of a vector */
float vec3_len2(const union vec3 *v)
{
	return v->v.x * v->v.x + v->v.y * v->v.y + v->v.z * v->v.z;
}

union vec3 *vec3_normalize(union vec3 *vo, const union vec3 *vi)
{
	float len = sqrt(vec3_len2(vi));
	vo->v.x = vi->v.x / len;
	vo->v.y = vi->v.y / len;
	vo->v.z = vi->v.z / len;
	return vo;
}

union vec3 *vec3_normalize_self(union vec3 *vo)
{
	return vec3_normalize(vo, vo);
}

/* vec3 rotate by axis and angle */
union vec3* vec3_rot_axis(union vec3 *vo, union vec3 *vi, float x, float y, float z, float angle)
{
	vec3_copy(vo, vi);
	return vec3_rot_axis_self(vo, x, y, z, angle);
}

/* vec3 rotate self by axis and angle */
union vec3* vec3_rot_axis_self(union vec3 *vo, float x, float y, float z, float angle)
{
	union quat rotate;
	quat_init_axis(&rotate, x, y, z, angle);
	quat_rot_vec_self(vo, &rotate);
	return vo;
}

double vec3_dist(const union vec3 *v1, const union vec3 *v2)
{
	return sqrt(
		(v1->v.x - v2->v.x)*(v1->v.x - v2->v.x) +
		(v1->v.y - v2->v.y)*(v1->v.y - v2->v.y) +
		(v1->v.z - v2->v.z)*(v1->v.z - v2->v.z));
}

double vec3_dist_c(const union vec3 *v1, float x, float y, float z)
{
	return sqrt(
		(v1->v.x - x)*(v1->v.x - x) +
		(v1->v.y - y)*(v1->v.y - y) +
		(v1->v.z - z)*(v1->v.z - z));
}

float vec3_dist_sqrd(const union vec3 *v1, const union vec3 *v2)
{
	return (v1->v.x - v2->v.x) * (v1->v.x - v2->v.x) +
		(v1->v.y - v2->v.y) * (v1->v.y - v2->v.y) +
		(v1->v.z - v2->v.z) * (v1->v.z - v2->v.z);
}

void vec3_print(const char *prefix, const union vec3 *v)
{
	printf("%s%f, %f, %f\n", prefix, v->v.x, v->v.y, v->v.z);
}

#if 1
/* Calculate the quaternion to rotate from vector u to vector v */
void quat_from_u2v(union quat *q, const union vec3 *u, const union vec3 *v,
		__attribute__((unused)) const union vec3 *up)
{
	/* See: http://lolengine.net/blog/2013/09/18/beautiful-maths-quaternion-from-vectors */
	union vec3 w;

	if (vec3_len2(u) < 0.001 || vec3_len2(v) < 0.001) {
		*q = identity_quat;
		return;
	}
	vec3_cross(&w, u, v);
	q->v.w = vec3_dot(u, v);
	q->v.x = w.v.x;
	q->v.y = w.v.y;
	q->v.z = w.v.z;
	q->v.w += quat_len(q);
	if (quat_len(q) < 0.00001) {
		*q = identity_quat;
		return;
	}
	quat_normalize_self(q);
}

#else
/* see http://gamedev.stackexchange.com/questions/15070/orienting-a-model-to-face-a-target */
/* Calculate the quaternion to rotate from vector u to vector v */
void quat_from_u2v(union quat *q, const union vec3 *u, const union vec3 *v, const union vec3 *up)
{
	union vec3 un, vn, axis, axisn;
	float dot;
	float angle;

	vec3_normalize(&un, u);
	vec3_normalize(&vn, v);
	dot = vec3_dot(&un, &vn);
	if (fabs(dot - -1.0f) < ZERO_TOLERANCE) {
		/* vector a and b point exactly in the opposite direction
		 * so it is a 180 degrees turn around the up-axis
		 */
		union vec3 default_up = { { 0, 1, 0} };
		if (!up)
			up = &default_up;
		quat_init_axis(q, up->v.x, up->v.y, up->v.z, M_PI);
		return;
	}
	if (fabs(dot - 1.0f) < ZERO_TOLERANCE) {
		/* vector a and b point exactly in the same direction
		 * so we return the identity quaternion
		 */
		*q = identity_quat;
		return;
	}
	angle = acos(dot);
	vec3_cross(&axis, &un, &vn);
	vec3_normalize(&axisn, &axis);
	quat_init_axis(q, axisn.v.x, axisn.v.y, axisn.v.z, angle);
}
#endif

union quat *quat_lerp(union quat *qo, const union quat *qfrom, const union quat *qto, float t)
{
	double cosom = quat_dot(qfrom, qto);

	/* qto = qfrom or qto = -qfrom so no rotation to slerp */
	if (cosom >= 1.0) {
		quat_copy(qo, qfrom);
		return qo;
	}

	/* adjust for shortest path */
	union quat to1;
	if (cosom < 0.0) {
		to1.v.x = -qto->v.x;
		to1.v.y = -qto->v.y;
		to1.v.z = -qto->v.z;
		to1.v.w = -qto->v.w;
	} else {
		quat_copy(&to1, qto);
	}

	double scale0 = 1.0 - t;
	double scale1 = t;

	/* calculate final values */
	qo->v.x = scale0 * qfrom->v.x + scale1 * to1.v.x;
	qo->v.y = scale0 * qfrom->v.y + scale1 * to1.v.y;
	qo->v.z = scale0 * qfrom->v.z + scale1 * to1.v.z;
	qo->v.w = scale0 * qfrom->v.w + scale1 * to1.v.w;
	return qo;
}

union quat *quat_nlerp(union quat *qo, const union quat *qfrom, const union quat *qto, float t)
{
	quat_lerp(qo, qfrom, qto, t);
	quat_normalize_self(qo);
	return qo;
}

union quat *quat_slerp(union quat *qo, const union quat *qfrom, const union quat *qto, float t)
{
	/* calc cosine */
	double cosom = quat_dot(qfrom, qto);

	/* qto = qfrom or qto = -qfrom so no rotation to slerp */
	if (cosom >= 1.0) {
		quat_copy(qo, qfrom);
		return qo;
	}

	/* adjust for shortest path */
	union quat to1;
	if (cosom < 0.0) {
		cosom = -cosom;
		to1.v.x = -qto->v.x;
		to1.v.y = -qto->v.y;
		to1.v.z = -qto->v.z;
		to1.v.w = -qto->v.w;
	} else {
		quat_copy(&to1, qto);
	}

	/* calculate coefficients */
	double scale0, scale1;
	if (cosom < 0.99995) {
		/* standard case (slerp) */
		double omega = acos(cosom);
		double sinom = sin(omega);
		scale0 = sin((1.0 - t) * omega) / sinom;
		scale1 = sin(t * omega) / sinom;
	} else {
		/* "from" and "to" quaternions are very close
		 *  ... so we can do a linear interpolation
		 */
		scale0 = 1.0 - t;
		scale1 = t;
	}

	/* calculate final values */
	qo->v.x = scale0 * qfrom->v.x + scale1 * to1.v.x;
	qo->v.y = scale0 * qfrom->v.y + scale1 * to1.v.y;
	qo->v.z = scale0 * qfrom->v.z + scale1 * to1.v.z;
	qo->v.w = scale0 * qfrom->v.w + scale1 * to1.v.w;
	return qo;
}

union vec3* vec3_lerp(union vec3* vo, const union vec3* vfrom, const union vec3* vto, double t)
{
	vo->v.x = vfrom->v.x + t * (vto->v.x - vfrom->v.x);
	vo->v.y = vfrom->v.y + t * (vto->v.y - vfrom->v.y);
	vo->v.z = vfrom->v.z + t * (vto->v.z - vfrom->v.z);
	return vo;
}

/* Apply incremental yaw, pitch and roll relative to the quaternion.
 * For example, if the quaternion represents an orientation of a ship,
 * this will apply yaw/pitch/roll *in the ship's local coord system to the
 * orientation.
 */
union quat *quat_apply_relative_yaw_pitch_roll(union quat *q,
					double yaw, double pitch, double roll)
{
	union quat qyaw, qpitch, qroll, qrot, tempq, local_rotation, rotated_q;

	/* calculate amount of yaw to impart this iteration... */
	quat_init_axis(&qyaw, 0.0, 1.0, 0.0, yaw);
	/* Calculate amount of pitch to impart this iteration... */
	quat_init_axis(&qpitch, 0.0, 0.0, 1.0, pitch);
	/* Calculate amount of roll to impart this iteration... */
	quat_init_axis(&qroll, 1.0, 0.0, 0.0, roll);
	/* Combine pitch, roll and yaw */
	quat_mul(&tempq, &qyaw, &qpitch);
	quat_mul(&qrot, &tempq, &qroll);

	/* Convert rotation to local coordinate system */
	quat_conjugate(&local_rotation, &qrot, q);
	/* Apply to local orientation */
	quat_mul(&rotated_q, &local_rotation, q);
	quat_normalize_self(&rotated_q);
	*q = rotated_q;
	return q;
}

/* Apply incremental yaw and pitch relative to the quaternion.
 * Yaw is applied to world axis so no roll will accumulate */
union quat *quat_apply_relative_yaw_pitch(union quat *q, double yaw, double pitch)
{
	union quat qyaw, qpitch, q1;

	/* calculate amount of yaw to impart this iteration... */
	quat_init_axis(&qyaw, 0.0, 1.0, 0.0, yaw);
	/* Calculate amount of pitch to impart this iteration... */
	quat_init_axis(&qpitch, 0.0, 0.0, 1.0, pitch);

	quat_mul(&q1, &qyaw, q);
	quat_mul(q, &q1, &qpitch);
	return q;
}

void quat_decompose_twist_swing(const union quat *q, const union vec3 *v1, union quat *twist, union quat *swing)
{
	union vec3 v2;
	quat_rot_vec(&v2, v1, q);

	quat_from_u2v(swing, v1, &v2, 0);
	union quat swing_inverse;
	quat_inverse(&swing_inverse, swing);
	quat_mul(twist, q, &swing_inverse);
}

void quat_decompose_swing_twist(const union quat *q, const union vec3 *v1, union quat *swing, union quat *twist)
{
	union vec3 v2;
	quat_rot_vec(&v2, v1, q);

	quat_from_u2v(swing, v1, &v2, 0);
	union quat swing_inverse;
	quat_inverse(&swing_inverse, swing);
	quat_mul(twist, &swing_inverse, q);
}

/* find the two endpoints of a line segment that are inside a given sphere
   http://stackoverflow.com/a/17499940 */
int sphere_line_segment_intersection(const union vec3 *v0, const union vec3 *v1, const union vec3 *center, double r, union vec3 *vo0, union vec3 *vo1)
{
	double cx = center->v.x;
	double cy = center->v.y;
	double cz = center->v.z;

	double px = v0->v.x;
	double py = v0->v.y;
	double pz = v0->v.z;

	double vx = v1->v.x - px;
	double vy = v1->v.y - py;
	double vz = v1->v.z - pz;

	double A = vx * vx + vy * vy + vz * vz;
	double B = 2.0 * (px * vx + py * vy + pz * vz - vx * cx - vy * cy - vz * cz);
	double C = px * px - 2 * px * cx + cx * cx + py * py - 2 * py * cy + cy * cy +
		pz * pz - 2 * pz * cz + cz * cz - r * r;
	double D = B * B - 4.0 * A * C;

	/* outside or tanget to sphere, no segment intersection */
	if (D <= 0)
		return -1;

	double t1 = (-B - sqrt(D)) / (2.0 * A);
	double t2 = (-B + sqrt(D)) / (2.0 * A);

	/* infinte line intersects but this segment doesn't */
	if ((t1 < 0 && t2 < 0) || (t1 > 1 && t2 > 1))
		return -1;

	if (t1 < 0)
		vec3_copy(vo0, v0);
	else if (t1 > 1)
		vec3_copy(vo1, v1);
	else
		vec3_init(vo0, v0->v.x * (1.0 - t1) + t1 * v1->v.x, v0->v.y * (1.0 - t1) + t1 * v1->v.y, v0->v.z * (1.0 - t1) + t1 * v1->v.z);

	if (t2 < 0)
		vec3_copy(vo0, v0);
	else if (t2 > 1)
		vec3_copy(vo1, v1);
	else
		vec3_init(vo1, v0->v.x * (1.0 - t2) + t2 * v1->v.x, v0->v.y * (1.0 - t2) + t2 * v1->v.y, v0->v.z * (1.0 - t2) + t2 * v1->v.z);
	return 2;
}

/* for a plane defined by n=normal, return a u and v vector that is on that plane and perpendictular */
void plane_vector_u_and_v_from_normal(union vec3 *u, union vec3 *v, const union vec3 *n)
{
	union vec3 basis = { { 1, 0, 0 } };

	/* find a vector we can use for our basis to define v */
	float dot = vec3_dot(n, &basis);
	if (fabs(dot) >= 1.0 - ZERO_TOLERANCE) {
		/* if forward is parallel, we can use up */
		vec3_init(&basis, 0, 1, 0);
	}

	/* find the right vector from our basis and the normal */
	vec3_cross(v, &basis, n);
	vec3_normalize_self(v);

	/* now the final forward vector is perpendicular n and v */
	vec3_cross(u, n, v);
	vec3_normalize_self(u);
}

/* return a random point in an annulus on a plane in 3d, r1=inner radius, r2=outer radius, c=center, n=normal */
void random_point_in_3d_annulus(float r1, float r2, const union vec3 *center, const union vec3 *u, const union vec3 *v, union vec3 *point)
{
	float angle = snis_random_float() * M_PI;
	float r = fabs(snis_random_float()) * (r2 - r1) + r1;

	point->v.x = center->v.x + r * cos(angle) * u->v.x + r * sin(angle) * v->v.x;
	point->v.y = center->v.y + r * cos(angle) * u->v.y + r * sin(angle) * v->v.y;
	point->v.z = center->v.z + r * cos(angle) * u->v.z + r * sin(angle) * v->v.z;
}

float vec3_magnitude2(union vec3 *v)
{
	const float x2 = v->v.x * v->v.x;
	const float y2 = v->v.y * v->v.y;
	const float z2 = v->v.z * v->v.z;

	return x2 + y2 + z2;
}

float vec3_magnitude(union vec3 *v)
{
	return sqrt(vec3_magnitude2(v));
}

/* See TestRaySphere() in "Real Time Collision Detection", p. 179, by Christer Ericson. */
int ray_intersects_sphere(const union vec3 *ray_origin,
				const union vec3 *ray_direction,
				const union vec3 *sphere_origin,
				const float radius)
{
	union vec3 m;
	float c, b, disc;

	vec3_sub(&m, ray_origin, sphere_origin);
	c = vec3_dot(&m, &m) - radius * radius;

	/* If there is definitely at least one real root, there must be an intersection */
	if (c <= 0.0f)
		return 1;

	b = vec3_dot(&m, ray_direction);

	/* Early exit if ray origin outside sphere and ray pointing away from sphere */
	if (b > 0.0f)
		return 0;

	disc = b * b - c;
	/* A negative discriminant corresponds to ray missing sphere */
	if (disc < 0.0f)
		return 0;

	/* Now ray must hit sphere */
	return 1;
}

/* Returns distance from point to plane defined by plane_point and plane_normal */
float plane_to_point_dist(const union vec3 plane_point, const union vec3 plane_normal,
			const union vec3 point)
{
	union vec3 diff;

	vec3_sub(&diff, &point, &plane_point);
	return vec3_dot(&plane_normal, &diff);
}

/* See "Real Time Collision Detection", by Christer Ericson, p. 224 */
int moving_spheres_intersection(union vec3 *s1, float r1, union vec3 *v1,
				union vec3 *s2, float r2, union vec3 *v2,
				float time_horizon, float *time)
{
	union vec3 s, v;
	float r, c, t;

	vec3_sub(&s, s2, s1); /* vector between sphere centers */
	vec3_sub(&v, v2, v1); /* relative velocity of s2 wrt stationary s1 */
	r = r1 + r2;
	c = vec3_dot(&s, &s) - r * r;
	if (c < 0.0f) {
		*time = 0.0f; /* already touching */
		return 1;
	}
	float a = vec3_dot(&v, &v);
	if (a < ZERO_TOLERANCE)
		return 0; /* spheres not moving relative to each other */
	float b = vec3_dot(&v, &s);
	if (b >= 0.0f)
		return 0; /* spheres not moving towards each other */
	float d = b * b - a * c;
	if (d < 0.0f)
		return 0; /* No real-valued root, spheres do not intersect */
	t = (-b - sqrtf(d)) / a;
	if (time_horizon < 0 || t < time_horizon) {
		*time = t;
		return 1;
	}
	return 0;
}

/* For the +z face of a cubemapped unit sphere, returns tangent and bitangent vectors
 * See: http://www.iquilezles.org/www/articles/patchedsphere/patchedsphere.htm
 */
void cubemapped_sphere_tangent_and_bitangent(float x, float y, union vec3 *u, union vec3 *v)
{
	u->v.x = -(1.0f + y * y);
	u->v.y = x * y;
	u->v.z = x;
	vec3_normalize_self(u);

	v->v.x = x * y;
	v->v.y = -(1 + x * x);
	v->v.z = y;
	vec3_normalize_self(v);
}

/* Returns the square of the distance between a point p, and the line segment formed by
 * p1 and p2.
 */
float dist2_from_point_to_line_segment(union vec3 *p, union vec3 *p1, union vec3 *p2,
					union vec3 *nearest_point)
{
	union vec3 point, p_to_p1, p_to_p2, p1_to_p2;
	float t;

	vec3_sub(&p1_to_p2, p2, p1);
	vec3_sub(&p_to_p1, p, p1);

	t = vec3_dot(&p1_to_p2, &p_to_p1) / vec3_dot(&p1_to_p2, &p1_to_p2);
	if (t < 0.0) {
		*nearest_point = *p1;
		return vec3_magnitude2(&p_to_p1);
	} else if (t > 1.0) {
		vec3_sub(&p_to_p2, p, p2);
		*nearest_point = *p2;
		return vec3_magnitude2(&p_to_p2);
	}
	vec3_mul_self(&p1_to_p2, t);
	vec3_add(&point, p1, &p1_to_p2); 
	vec3_sub(&p1_to_p2, p, &point);
	*nearest_point = point;
	return vec3_magnitude2(&p1_to_p2);
}

/* Returns distance from the given point to surface of a torus at the origin
 * with given major and minor radius. The torus is assumed to be in the y,z plane,
 * so if you were to fly down the x axis, you would fly through the doughnut hole.
 * From http://iquilezles.org/www/articles/distfunctions/distfunctions.htm
 */
float point_to_torus_dist(const union vec3 * const point, const float major_radius, const float minor_radius)
{
	union vec2 q;

	q.v.y = sqrtf(point->v.y * point->v.y + point->v.z * point->v.z) - major_radius;
	q.v.x = point->v.x;
	return sqrtf(q.v.x * q.v.x + q.v.y * q.v.y) - minor_radius;
}

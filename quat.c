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
#include <stdio.h>

#include "quat.h"
#include "mathutils.h"

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
	*mark = asin(dir->v.y/dist);
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

const union quat identity_quat = { {1.0, 0.0, 0.0, 0.0} };

void random_quat(union quat *q)
{
	float angle;
	union vec3 v;

	random_point_on_sphere(1.0, &v.v.x, &v.v.y, &v.v.z);
	angle = (float) snis_randn(360) * M_PI / 180.0;
	quat_init_axis_v(q, &v, angle);
	
}

void random_axis_quat(union quat *q)
{
	float angle;
	union vec3 v;

	random_point_on_sphere(1.0, &v.v.x, &v.v.y, &v.v.z);
	angle = 0.0;
	quat_init_axis_v(q, &v, angle);
}

void vec3_init(union vec3 *vo, float x, float y, float z)
{
	vo->v.x = x;
	vo->v.y = y;
	vo->v.z = z;
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

union vec3* vec3_mul(union vec3 *vo, const union vec3 *vi, float scalar)
{
	vo->vec[0] = vi->vec[0] * scalar;
	vo->vec[1] = vi->vec[1] * scalar;
	vo->vec[2] = vi->vec[2] * scalar;
	return vo;
}

union vec3* vec3_mul_self(union vec3 *vi, float scalar)
{
	return vec3_mul( vi, vi, scalar );
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

void vec3_print(const char* prefix, const union vec3 *v)
{
	printf("%s%f, %f, %f\n", prefix, v->v.x, v->v.y, v->v.z);
}

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
	if (fabs(dot - -1.0f) <  0.000001f) {
		/* vector a and b point exactly in the opposite direction
		 * so it is a 180 degrees turn around the up-axis 
		 */
		union vec3 default_up = { { 0, 1, 0} };
		if (!up)
			up = &default_up;
		quat_init_axis(q, up->v.x, up->v.y, up->v.z, M_PI);
		return;
	}
	if (fabs(dot - 1.0f) <  0.000001f) {
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

union quat* quat_lerp(union quat *qo, const union quat *qfrom, const union quat *qto, float t)
{
	double cosom = quat_dot(qfrom, qto);

	// qto=qfrom or qto=-qfrom so no rotation to slerp
	if (cosom >= 1.0) {
		quat_copy(qo, qfrom);
		return qo;
	}

	// adjust for shortest path
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

	// calculate final values
	qo->v.x = scale0 * qfrom->v.x + scale1 * to1.v.x;
	qo->v.y = scale0 * qfrom->v.y + scale1 * to1.v.y;
	qo->v.z = scale0 * qfrom->v.z + scale1 * to1.v.z;
	qo->v.w = scale0 * qfrom->v.w + scale1 * to1.v.w;
	return qo;
}

union quat* quat_nlerp(union quat *qo, const union quat *qfrom, const union quat *qto, float t)
{
	quat_lerp(qo, qfrom, qto, t);
	quat_normalize_self(qo);
	return qo;
}

union quat* quat_slerp(union quat *qo, const union quat *qfrom, const union quat *qto, float t)
{
	// calc cosine
	double cosom = quat_dot(qfrom, qto);

	// qto=qfrom or qto=-qfrom so no rotation to slerp
	if (cosom >= 1.0) {
		quat_copy(qo, qfrom);
		return qo;
	}

	// adjust for shortest path
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

	// calculate coefficients
	double scale0, scale1;
	if (cosom < 0.99995) {
		// standard case (slerp)
		double omega = acos(cosom);
		double sinom = sin(omega);
		scale0 = sin((1.0 - t) * omega) / sinom;
		scale1 = sin(t * omega) / sinom;
	} else {
		// "from" and "to" quaternions are very close
		//  ... so we can do a linear interpolation
		scale0 = 1.0 - t;
		scale1 = t;
	}

	// calculate final values
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
 * this will apply yaw/pitch/roll *in the ship's local coord system* to the
 * orientation.
 */
union quat *quat_apply_relative_yaw_pitch_roll(union quat *q,
					double yaw, double pitch, double roll)
{
	union quat qyaw, qpitch, qroll, qrot, q1, q2, q3, q4;

	/* calculate amount of yaw to impart this iteration... */
	quat_init_axis(&qyaw, 0.0, 1.0, 0.0, yaw);
	/* Calculate amount of pitch to impart this iteration... */
	quat_init_axis(&qpitch, 0.0, 0.0, 1.0, pitch);
	/* Calculate amount of roll to impart this iteration... */
	quat_init_axis(&qroll, 1.0, 0.0, 0.0, roll);
	/* Combine pitch, roll and yaw */
	quat_mul(&q1, &qyaw, &qpitch);
	quat_mul(&qrot, &q1, &qroll);

	/* Convert rotation to local coordinate system */
	quat_mul(&q1, q, &qrot);
	quat_conj(&q2, q);
	quat_mul(&q3, &q1, &q2);
	/* Apply to local orientation */
	quat_mul(&q4, &q3, q);
	quat_normalize_self(&q4);
	*q = q4;
	return q;
}


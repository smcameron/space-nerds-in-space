
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


#ifndef FOR_N
#define FOR_N(v, m) for (int v = 0; v < m; ++v)
#endif /* FOR_N */


void vec3_copy(vec3_t *vo, vec3_t *vi)
{
   memcpy(vo, vi, sizeof(vec3_t));   
}


void quat_init(quat_t *q, const vec3_t *acc, const vec3_t *mag)
{
   float ax = acc->x;
   float ay = acc->y;
   float az = acc->z;
   float mx = mag->x;
   float my = mag->y;
   float mz = mag->z;


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

   q->q0 = cos_roll * cos_pitch * cosHeading + sin_roll * sin_pitch * sinHeading;
   q->q1 = sin_roll * cos_pitch * cosHeading - cos_roll * sin_pitch * sinHeading;
   q->q2 = cos_roll * sin_pitch * cosHeading + sin_roll * cos_pitch * sinHeading;
   q->q3 = cos_roll * cos_pitch * sinHeading - sin_roll * sin_pitch * cosHeading;
}


void quat_init_axis(quat_t *q, float x, float y, float z, float a)
{
   /* see: http://www.euclideanspace.com/maths/geometry/rotations
           /conversions/angleToQuaternion/index.htm */
   float a2 = a * 0.5f;
   float s = sin(a2);
   q->x = x * s;   
   q->y = y * s;   
   q->z = z * s;   
   q->w = cos(a2);   
}


void quat_init_axis_v(quat_t *q, const vec3_t *v, float a)
{
   quat_init_axis(q, v->x, v->y, v->z, a);
}


void quat_rot_vec_self(vec3_t *v, const quat_t *q)
{
   vec3_t vo;
   quat_rot_vec(&vo, v, q);
   vec3_copy(v, &vo);
}


void quat_rot_vec(vec3_t *vo, const vec3_t *vi, const quat_t *q)
{
   /* see: https://github.com/qsnake/ase/blob/master/ase/quaternions.py */
   const float vx = vi->x, vy = vi->y, vz = vi->z;
   const float qw = q->w, qx = q->x, qy = q->y, qz = q->z;
   const float qww = qw * qw, qxx = qx * qx, qyy = qy * qy, qzz = qz * qz;
   const float qwx = qw * qx, qwy = qw * qy, qwz = qw * qz, qxy = qx * qy;
   const float qxz = qx * qz, qyz = qy * qz;
   vo->x = (qww + qxx - qyy - qzz) * vx + 2 * ((qxy - qwz) * vy + (qxz + qwy) * vz);
   vo->y = (qww - qxx + qyy - qzz) * vy + 2 * ((qxy + qwz) * vx + (qyz - qwx) * vz);
   vo->z = (qww - qxx - qyy + qzz) * vz + 2 * ((qxz - qwy) * vx + (qyz + qwx) * vy);
}


void quat_copy(quat_t *qo, const quat_t *qi)
{
   memcpy(qo, qi, sizeof(quat_t));   
}


float quat_len(const quat_t *q)
{
   float s = 0.0f;
   FOR_N(i, 4)
      s += q->vec[i] * q->vec[i];
   return sqrtf(s);
}


void quat_conj(quat_t *q_out, const quat_t *q_in)
{
   q_out->x = -q_in->x;
   q_out->y = -q_in->y;
   q_out->z = -q_in->z;
   q_out->w = q_in->w;
}


void quat_to_euler(euler_t *euler, const quat_t *quat)
{
   const float x = quat->x, y = quat->y, z = quat->z, w = quat->w;
   const float ww = w * w, xx = x * x, yy = y * y, zz = z * z;
   euler->yaw = normalize_euler_0_2pi(atan2f(2.f * (x * y + z * w), xx - yy - zz + ww));
   euler->pitch = asinf(-2.f * (x * z - y * w));
   euler->roll = atan2f(2.f * (y * z + x * w), -xx - yy + zz + ww);
}


void quat_mul(quat_t *o, const quat_t *q1, const quat_t *q2)
{
   /* see: http://www.euclideanspace.com/maths/algebra/
           realNormedAlgebra/quaternions/code/index.htm#mul */
   o->x =  q1->x * q2->w + q1->y * q2->z - q1->z * q2->y + q1->w * q2->x;
   o->y = -q1->x * q2->z + q1->y * q2->w + q1->z * q2->x + q1->w * q2->y;
   o->z =  q1->x * q2->y - q1->y * q2->x + q1->z * q2->w + q1->w * q2->z;
   o->w = -q1->x * q2->x - q1->y * q2->y - q1->z * q2->z + q1->w * q2->w;
}


void quat_add(quat_t *o, const quat_t *q1, const quat_t *q2)
{
   /* see: http://www.euclideanspace.com/maths/algebra/
           realNormedAlgebra/quaternions/code/index.htm#add */
   o->x = q1->x + q2->x;
   o->y = q1->y + q2->y;
   o->z = q1->z + q2->z;
   o->w = q1->w + q2->w;
}


void quat_add_to(quat_t *o, const quat_t *q)
{
   quat_t tmp;
   quat_add(&tmp, o, q);
   quat_copy(o, &tmp);
}


void quat_scale(quat_t *o, const quat_t *q, float f)
{
   /* see: http://www.euclideanspace.com/maths/algebra/
           realNormedAlgebra/quaternions/code/index.htm#scale*/
   FOR_N(i, 4)
      o->vec[i] = q->vec[i] * f;
}


void quat_scale_self(quat_t *q, float f)
{
   quat_scale(q, q, f);
}


void quat_normalize(quat_t *o, const quat_t *q)
{
   /* see: http://www.euclideanspace.com/maths/algebra/
           realNormedAlgebra/quaternions/code/index.htm#normalise */
   quat_scale(o, q, 1.0f / quat_len(q));
}


void quat_normalize_self(quat_t *q)
{
   quat_normalize(q, q);
}


float normalize_euler_0_2pi(float a)
{
   while (a < 0)
      a += (float)(2 * M_PI);
   return a;
}


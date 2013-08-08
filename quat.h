
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
typedef union
{
   struct
   {
      float x;
      float y;
      float z;
   };
   float vec[3];
}
vec3_t;


/* quaternion */
typedef union
{
   struct
   {
      float q0;
      float q1;
      float q2;
      float q3;
   };
   struct
   {
      float w;
      float x;
      float y;
      float z;
   };
   float vec[4];
}
quat_t;


/* euler angle */
typedef union
{
   struct
   {
      float yaw;
      float pitch;
      float roll;
   };
   float vec[3];
}
euler_t;


/* copy vector vi to vo */
void vec3_copy(vec3_t *vo, vec3_t *vi);

/* init orientation quaternion from measurements */
void quaternion_init(quat_t *quat, const vec3_t *acc, const vec3_t *mag);

/* initialize quaternion from axis angle using floats */
void quat_init_axis(quat_t *q, float x, float y, float z, float a);

/* initialize quaternion from axis angle using a vector */
void quat_init_axis_v(quat_t *q, const vec3_t *v, float a);

/* rotate vector vi via unit quaternion q and put result into vector vo */
void quat_rot_vec(vec3_t *vo, const vec3_t *vi, const quat_t *q);

/* rotate vector v_in in-place via unit quaternion quat */
void quat_rot_vec_self(vec3_t *v, const quat_t *q);

/* returns len of quaternion */
float quat_len(const quat_t *q);

/* copy quaternion qi to qo */
void quat_copy(quat_t *qo, const quat_t *qi);

/* qo = qi * f */
void quat_scale(quat_t *qo, const quat_t *qi, float f);

/* qo *= f */
void quat_scale_self(quat_t *q, float f);

/* conjugate quaternion */
void quat_conj(quat_t *qo, const quat_t *qi);

/* o = q1 + 12 */
void quat_add(quat_t *qo, const quat_t *q1, const quat_t *q2);

/* o += q */
void quat_add_self(quat_t *o, const quat_t *q);

/* o = q1 * q2 */
void quat_mul(quat_t *o, const quat_t *q1, const quat_t *q2);

/* normalizes quaternion q and puts result into o */
void quat_normalize(quat_t *qo, const quat_t *qi);

/* normalize q in-place */
void quat_normalize_self(quat_t *q);

/* convert quaternion to euler angles */
void quat_to_euler(euler_t *e, const quat_t *q);

/* normalize angle */
float normalize_euler_0_2pi(float a);


#endif /* __QUAT_H__ */


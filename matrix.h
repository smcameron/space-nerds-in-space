#ifndef MATRIX_H__
#define MATRIX_H__

/*
        Copyright (C) 2010 Stephen M. Cameron
        Author: Stephen M. Cameron

        This file is part of Spacenerds In Space.

        Spacenerds in Space is free software; you can redistribute it and/or modify
        it under the terms of the GNU General Public License as published by
        the Free Software Foundation; either version 2 of the License, or
        (at your option) any later version.

        Spacenerds in Space is distributed in the hope that it will be useful,
        but WITHOUT ANY WARRANTY; without even the implied warranty of
        MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
        GNU General Public License for more details.

        You should have received a copy of the GNU General Public License
        along with Spacenerds in Space; if not, write to the Free Software
        Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

/* Matrices need to use row-major memory layout with pre-multiplication OR
 * column-major memory layout with post-multiplication.
 */

#ifdef DEFINE_MATRIX_GLOBALS
#define GLOBAL
#else
#define GLOBAL extern
#endif

union vec3;
union vec4;

struct mat44 {
	float m[4][4];
};

struct mat44d {
	double m[4][4];
};

struct mat33 {
	float m[3][3];
};

struct mat33d {
	double m[3][3];
};

struct mat41 {
	float m[4];
};

struct mat31 {
	float m[3];
};

GLOBAL struct mat33 *mat44_to_mat33_ff(const struct mat44 *src, struct mat33 *output);

GLOBAL struct mat33d *mat44_to_mat33_dd(const struct mat44d *src, struct mat33d *output);

GLOBAL struct mat44d *mat33_to_mat44_dd(const struct mat33d *src, struct mat44d *output);

GLOBAL struct mat33 *mat33_inverse_transpose_ff(const struct mat33 *src, struct mat33 *output);

GLOBAL struct mat33 *mat33_transpose(const struct mat33 *src, struct mat33 *output);

GLOBAL struct mat33d *mat33_transpose_dd(const struct mat33d *src, struct mat33d *output);

GLOBAL void mat33_product(const struct mat33 *lhs, const struct mat33 *rhs, struct mat33 *output);

GLOBAL void mat33_product_ddf(const struct mat33d *lhs, const struct mat33d *rhs, struct mat33 *output);

GLOBAL void mat33_x_vec3(const struct mat33 *lhs, const union vec3 *rhs,
				union vec3 *output);

GLOBAL void mat44_convert_df(const struct mat44d *src, struct mat44 *output);

GLOBAL void mat44_product(const struct mat44 *lhs, const struct mat44 *rhs,
				struct mat44 *output);

GLOBAL void mat44_product_ddd(const struct mat44d *lhs, const struct mat44d *rhs,
				struct mat44d *output);

GLOBAL void mat44_product_ddf(const struct mat44d *lhs, const struct mat44d *rhs,
				struct mat44 *output);

GLOBAL void mat44_x_mat41(const struct mat44 *lhs, const struct mat41 *rhs,
				struct mat41 *output);

GLOBAL void mat44_x_mat41_dff(const struct mat44d *lhs, const struct mat41 *rhs,
				struct mat41 *output);

GLOBAL void mat44_x_vec4_dff(const struct mat44d *lhs, const union vec4 *rhs,
				union vec4 *output);

GLOBAL void mat41_x_mat44(const struct mat41 *lhs, const struct mat44 *rhs,
				struct mat41 *output);

GLOBAL void mat44_x_vec4(const struct mat44 *lhs, const union vec4 *rhs,
				union vec4 *output);

GLOBAL void mat44_x_vec4_into_vec3(const struct mat44 *lhs, const union vec4 *rhs,
				union vec3 *output);

GLOBAL void mat44_x_vec4_into_vec3_dff(const struct mat44d *lhs, const union vec4 *rhs,
				union vec3 *output);

GLOBAL void normalize_vector(struct mat41 *v, struct mat41 *output);
GLOBAL void mat41_cross_mat41(struct mat41 *v1, struct mat41 *v2, struct mat41 *output);
GLOBAL void print44(struct mat44 *m);
GLOBAL void print41(struct mat41 *m);
GLOBAL float mat41_dot_mat41(struct mat41 *m1, struct mat41 *m2);
GLOBAL float dist3d(float dx, float dy, float dz);
GLOBAL float dist3dsqrd(float dx, float dy, float dz);

#undef GLOBAL
#endif


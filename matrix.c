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

#include <stdio.h>
#include <math.h>
#include <string.h>
#define DEFINE_MATRIX_GLOBALS
#include "matrix.h"
#include "mtwist.h"
#include "quat.h"
#include "vec4.h"

struct mat33 *mat44_to_mat33_ff(const struct mat44 *src, struct mat33 *output)
{
	int i, j;

	for (i = 0; i < 3; i++)
		for (j = 0; j < 3; j++)
			output->m[i][j] = src->m[i][j];
	return output;
}

struct mat33d *mat44_to_mat33_dd(const struct mat44d *src, struct mat33d *output)
{
	int i, j;

	for (i = 0; i < 3; i++)
		for (j = 0; j < 3; j++)
			output->m[i][j] = src->m[i][j];
	return output;
}

struct mat44d *mat33_to_mat44_dd(const struct mat33d *src, struct mat44d *output)
{
	int i, j;

	for (i = 0; i < 3; i++)
		for (j = 0; j < 3; j++)
			output->m[i][j] = src->m[i][j];

	output->m[0][3] = 0;
	output->m[1][3] = 0;
	output->m[2][3] = 0;

	output->m[3][0] = 0;
	output->m[3][1] = 0;
	output->m[3][2] = 0;
	output->m[3][3] = 1;

	return output;
}

struct mat33 *mat33_transpose(const struct mat33 *src, struct mat33 *output)
{
	int i, j;

	for (i = 0; i < 3; i++)
		for (j = 0; j < 3; j++)
			output->m[i][j] = src->m[j][i];
	return output;
}

struct mat33d *mat33_transpose_dd(const struct mat33d *src, struct mat33d *output)
{
	int i, j;

	for (i = 0; i < 3; i++)
		for (j = 0; j < 3; j++)
			output->m[i][j] = src->m[j][i];
	return output;
}

struct mat33 *mat33_inverse_transpose_ff(const struct mat33 *src, struct mat33 *output)
{
	float determinant =
		+ src->m[0][0] * (src->m[1][1] * src->m[2][2] - src->m[1][2] * src->m[2][1])
		- src->m[0][1] * (src->m[1][0] * src->m[2][2] - src->m[1][2] * src->m[2][0])
		+ src->m[0][2] * (src->m[1][0] * src->m[2][1] - src->m[1][1] * src->m[2][0]);

	output->m[0][0] = (+(src->m[1][1] * src->m[2][2] - src->m[2][1] * src->m[1][2])) / determinant;
	output->m[0][1] = (-(src->m[1][0] * src->m[2][2] - src->m[2][0] * src->m[1][2])) / determinant;
	output->m[0][2] = (+(src->m[1][0] * src->m[2][1] - src->m[2][0] * src->m[1][1])) / determinant;
	output->m[1][0] = (-(src->m[0][1] * src->m[2][2] - src->m[2][1] * src->m[0][2])) / determinant;
	output->m[1][1] = (+(src->m[0][0] * src->m[2][2] - src->m[2][0] * src->m[0][2])) / determinant;
	output->m[1][2] = (-(src->m[0][0] * src->m[2][1] - src->m[2][0] * src->m[0][1])) / determinant;
	output->m[2][0] = (+(src->m[0][1] * src->m[1][2] - src->m[1][1] * src->m[0][2])) / determinant;
	output->m[2][1] = (-(src->m[0][0] * src->m[1][2] - src->m[1][0] * src->m[0][2])) / determinant;
	output->m[2][2] = (+(src->m[0][0] * src->m[1][1] - src->m[1][0] * src->m[0][1])) / determinant;
	return output;
}

void mat33_x_vec3(const struct mat33 *lhs, const union vec3 *rhs,
				union vec3 *output)
{
	int row, col;

	for (row = 0; row < 3; row++) {
		output->vec[row] = 0;
		for (col = 0; col < 3; col++)
			output->vec[row] += lhs->m[col][row] * rhs->vec[col];
	}
}


void mat44_convert_df(const struct mat44d *src, struct mat44 *output)
{
	int i, j;

	for (i = 0; i < 4; i++)
		for (j = 0; j < 4; j++)
			output->m[i][j] = (float)src->m[i][j];

}

void mat33_product(const struct mat33 *lhs, const struct mat33 *rhs,
			struct mat33 *output)
{
	int i, j, k;

	for (i = 0; i < 3; i++) {
		for (j = 0; j < 3; j++) {
			output->m[i][j] = 0;
			for (k = 0; k < 3; k++)
				output->m[i][j] += lhs->m[k][j] * rhs->m[i][k];
		}
	}
}

void mat33_product_ddf(const struct mat33d *lhs, const struct mat33d *rhs, struct mat33 *output)
{
	int i, j, k;

	for (i = 0; i < 3; i++) {
		for (j = 0; j < 3; j++) {
			double cell = 0;
			for (k = 0; k < 3; k++)
				cell += lhs->m[k][j] * rhs->m[i][k];
			output->m[i][j] = (float)cell;
		}
	}
}

/* Matrices need to be row-major and use row-major memory layout with
 * pre-multiplication 
 *
 * OR
 * 
 * Matrices need to be column major and use column-major memory layout
 * with post-multiplication.  
 * 
 * In row major storage adjacent columns are adjacent in memory and a 
 * row is contiguous, and a column is not contiguous.  In column major
 * storage, adjacent rows are adjacent in memory, and columns are
 * contiguous, and rows are not contiguous.
 *
 * This code is based on a very nice explanation found here:
 * http://www.kmjn.org/notes/3d_rendering_intro.html by Mark J. Nelson
 *
 */

void mat44_product(const struct mat44 *lhs, const struct mat44 *rhs,
			struct mat44 *output)
{
	int i, j, k;

	for (i = 0; i < 4; i++) {
		for (j = 0; j < 4; j++) {
			output->m[i][j] = 0;
			for (k = 0; k < 4; k++)
				output->m[i][j] += lhs->m[k][j] * rhs->m[i][k];
		}
	}
}

void mat44_product_ddd(const struct mat44d *lhs, const struct mat44d *rhs,
                                struct mat44d *output)
{
	int i, j, k;

	for (i = 0; i < 4; i++) {
		for (j = 0; j < 4; j++) {
			output->m[i][j] = 0;
			for (k = 0; k < 4; k++)
				output->m[i][j] += lhs->m[k][j] * rhs->m[i][k];
		}
	}
}

void mat44_product_ddf(const struct mat44d *lhs, const struct mat44d *rhs,
                                struct mat44 *output)
{
	int i, j, k;

	for (i = 0; i < 4; i++) {
		for (j = 0; j < 4; j++) {
			double cell = 0;
			for (k = 0; k < 4; k++)
				cell += lhs->m[k][j] * rhs->m[i][k];
			output->m[i][j] = (float)cell;
		}
	}
}

/* for post muliplication, mat44 must be column major and stored column major order */
void mat44_x_mat41(const struct mat44 *lhs, const struct mat41 *rhs,
				struct mat41 *output)
{
	/*
	     lhs         rhs     output
             | a b c d | | x |   |ax + by + cz + dw|
	     | e f g h | | y | = |ex + fy + gz + hw| 
	     | i j k l | | z |   |ix + jy + kz + lw|
	     | m n o p | | w |   |mx + ny + oz + pw|

	 assumed to be stored in memory like { {a, e, i, m}, {b, f, j, n}... }
	 so, lhs->m[3][2] == g. address like: lhs->[column][row].

	 */

	int row, col;

	for (row = 0; row < 4; row++) {
		output->m[row] = 0;
		for (col = 0; col < 4; col++)
			output->m[row] += lhs->m[col][row] * rhs->m[col];
	}
}

/* see mat44_x_mat41 */
void mat44_x_mat41_dff(const struct mat44d *lhs, const struct mat41 *rhs,
                                struct mat41 *output)
{
	int row, col;

	for (row = 0; row < 4; row++) {
		double cell = 0;
		for (col = 0; col < 4; col++)
			cell += lhs->m[col][row] * rhs->m[col];
		output->m[row] = (float)cell;
	}
}

/* see mat44_x_mat41 */
void mat44_x_vec4_dff(const struct mat44d *lhs, const union vec4 *rhs,
				union vec4 *output)
{
	int row, col;

	for (row = 3; row >= 0; row--) {
		double cell = 0;
		for (col = 3; col >= 0; col--)
			cell += lhs->m[col][row] * rhs->vec[col];
		output->vec[row] = (float)cell;
	}
}

/* see mat44_x_mat41 */
void mat44_x_vec4(const struct mat44 *lhs, const union vec4 *rhs,
				union vec4 *output)
{
	int row, col;

	for (row = 3; row >= 0; row--) {
		output->vec[row] = 0;
		for (col = 3; col >= 0; col--)
			output->vec[row] += lhs->m[col][row] * rhs->vec[col];
	}
}

/* see mat44_x_mat41 */
void mat44_x_vec4_into_vec3(const struct mat44 *lhs, const union vec4 *rhs,
				union vec3 *output)
{
	int row, col;

	for (row = 2; row >= 0; row--) {
		output->vec[row] = 0;
		for (col = 3; col >= 0; col--)
			output->vec[row] += lhs->m[col][row] * rhs->vec[col];
	}
}

/* see mat44_x_mat41 */
void mat44_x_vec4_into_vec3_dff(const struct mat44d *lhs, const union vec4 *rhs,
				union vec3 *output)
{
	int row, col;

	for (row = 2; row >= 0; row--) {
		double cell = 0;
		for (col = 3; col >= 0; col--)
			cell += lhs->m[col][row] * rhs->vec[col];
		output->vec[row] = (float)cell;
	}
}

/* for pre muliplication, mat44 must be row major and stored row major order */
void mat41_x_mat44(const struct mat41 *lhs, const struct mat44 *rhs,
				struct mat41 *output)
{
	/*

	lhs       rhs           output
	--------- | a b c d |   | xa + yb + zc + wd |
	x y z w   | e f g h | = | xe + yf + zg + wh |
	--------- | i j k l |   | xi + yj + zk + wl |
		  | m n o p |   | xm + yn + zo + wp |

	 assumed to be stored in memory like { {a, b, c, d}, {e, f, g, h}... }
	 so, rhs->m[3][2] == j, address like: rhs->[row][col].
	*/

	int row, col;

	for (row = 0; row < 4; row++) {
		output->m[row] = 0;
		for (col = 0; col < 4; col++)
			output->m[row] += lhs->m[col] * rhs->m[row][col];
	}
}

float dist3d(float dx, float dy, float dz)
{
	return sqrt(dx * dx + dy * dy + dz * dz);
}

float dist3dsqrd(float dx, float dy, float dz)
{
	return dx * dx + dy * dy + dz * dz;
}

/* safe to call with v == output */
void normalize_vector(struct mat41 *v, struct mat41 *output)
{
	float d;

	d = dist3d(v->m[0], v->m[1], v->m[2]);
	output->m[0] = v->m[0] / d;
	output->m[1] = v->m[1] / d;
	output->m[2] = v->m[2] / d;
}

void mat41_cross_mat41(struct mat41 *v1, struct mat41 *v2, struct mat41 *output)
{
	/* A x B = (a2b3 - a3b2, a3b1 - a1b3, a1b2 - a2b1); a vector quantity */
	output->m[0] = v1->m[1] * v2->m[2] - v1->m[2] * v2->m[1];
	output->m[1] = v1->m[2] * v2->m[0] - v1->m[0] * v2->m[2];
	output->m[2] = v1->m[0] * v2->m[1] - v1->m[1] * v2->m[0];
	output->m[3] = 1.0;
}

void print44(struct mat44 *m)
{
	printf("%lf %lf %lf %lf\n", m->m[0][0], m->m[1][0], m->m[2][0], m->m[3][0]);
	printf("%lf %lf %lf %lf\n", m->m[0][1], m->m[1][1], m->m[2][1], m->m[3][1]);
	printf("%lf %lf %lf %lf\n", m->m[0][2], m->m[1][2], m->m[2][2], m->m[3][2]);
	printf("%lf %lf %lf %lf\n", m->m[0][3], m->m[1][3], m->m[2][3], m->m[3][3]);
}

void print41(struct mat41 *m)
{
	printf("%lf %lf %lf %lf\n", m->m[0], m->m[1], m->m[2], m->m[3]);
}

float mat41_dot_mat41(struct mat41 *m1, struct mat41 *m2)
{
	return m1->m[0] * m2->m[0] + m1->m[1] * m2->m[1] + m1->m[2] * m2->m[2];
}

#ifdef TEST_MATRIX
#include <stdio.h>
#include <math.h>

int main(__attribute__((unused)) int argc, __attribute__((unused)) char *argv[])
{
	struct mat44 answer;
	struct mat44 identity = {{ { 1, 0, 0, 0 },  
				   { 0, 1, 0, 0 },
				   { 0, 0, 1, 0 },
				   { 0, 0, 0, 1 }}};

	struct mat44 t = {{        { 1, 0, 0, 0 },  
				   { 0, 1, 0, 0 },
				   { 0, 0, 1, 0 },
				   { 1, 2, 3, 1 }}};
#define angle (90.0 * M_PI / 180.0)
#if 0
	struct mat44 rotatex = {{{ 1, 0,            0,           0 },
				 { 0, cosf(angle),  sinf(angle), 0 },
				 { 0, -sinf(angle), cosf(angle), 0 },
				 { 0, 0,            0,           1 }}};
#endif

	struct mat44 rotatex = {{{ 1, 0,            0,           0 },
				 { 0, cosf(angle), -sinf(angle), 0 },
				 { 0, sinf(angle), cosf(angle), 0 },
				 { 0, 0,            0,           1 }}};
	struct mat44 abc = {{   { 0, 4, 8,  12 },
				{ 1, 5, 9,  13 },
				{ 2, 6, 10, 14 },
				{ 3, 7, 11, 15 }}};

	struct mat41 p = {{ 2, 2, 2, 0 }};
	struct mat41 p1 = {{ 1, 2, 3, 0 }};
	struct mat41 p2 = {{ 0 }};
	struct mat41 a = { { 0 } };
	struct mat41 a2 = { { 0 } };
	int row, column;

	mat44_product(&identity, &t, &answer);
	mat44_x_mat41(&identity, &p, &a);
	mat44_x_mat41(&t, &p, &a2);
	mat44_x_mat41(&rotatex, &p1, &p2);

	for (column = 0; column < 4; column++) {
		for (row = 0; row < 4; row++) {
			printf("%f ", answer.m[row][column]);
		}
		printf("\n");
	}

	printf("a = %lf %lf %lf %lf\n", a.m[0], a.m[1], a.m[2], a.m[3]);
	printf("a2 = %lf %lf %lf %lf\n", a2.m[0], a2.m[1], a2.m[2], a2.m[3]);
	printf("p2 = %lf %lf %lf %lf\n", p2.m[0], p2.m[1], p2.m[2], p2.m[3]);

	mat44_product(&identity, &abc, &answer);
	printf("answer =\n");
	print44(&answer);
	

	return 0;
}

#endif


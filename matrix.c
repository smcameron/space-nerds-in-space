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
#define DEFINE_MATRIX_GLOBALS
#include "matrix.h"

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
			output->m[row] += lhs->m[col][row] * rhs->m[row];
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


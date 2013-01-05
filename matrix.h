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

struct mat44 {
	float m[4][4];
};

struct mat41 {
	float m[4];
};

GLOBAL void mat44_x_mat44(const struct mat44 *lhs, const struct mat44 *rhs,
				struct mat44 *output);

GLOBAL void mat44_x_mat41(const struct mat44 *lhs, const struct mat41 *rhs,
				struct mat41 *output);

GLOBAL void mat41_x_mat44(const struct mat41 *lhs, const struct mat44 *rhs,
				struct mat41 *output);

#undef GLOBAL
#endif


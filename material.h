/*
	Copyright (C) 2010 Jeremy Van Grinsven
	Author: Jeremy Van Grinsven

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
#ifndef MATERIAL_H__
#define MATERIAL_H__

#define MATERIAL_COLOR 0
#define MATERIAL_COLOR_BY_W 1
#define MATERIAL_LASER 2
#define MATERIAL_BILLBOARD 3
#define MATERIAL_TEXTURE_MAPPED 4

struct material_color_by_w {
	int near_color;
	int center_color;
	int far_color;

	float near_w;
	float center_w;
	float far_w;
};

#define MATERIAL_BILLBOARD_TYPE_SCREEN 0
#define MATERIAL_BILLBOARD_TYPE_SPHERICAL 1
#define MATERIAL_BILLBOARD_TYPE_AXIS 2

struct material_billboard {
	int billboard_type;
	int texture_id;
};

struct material_texture_mapped {
	int texture_id;
};

#endif

/*
	Copyright (C) 2014 Jeremy Van Grinsven

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

uniform vec4 u_Viewport;

#define SMAA_RT_METRICS u_Viewport
#define SMAA_PRESET_HIGH
#if !defined(SMAA_GLSL_2) && !defined(SMAA_GLSL_3) && !defined(SMAA_GLSL_4)
#define SMAA_GLSL_2
#endif

#define SMAA_AREATEX_SELECT(sample) sample.ra

#if defined(INCLUDE_VS)
#define SMAA_INCLUDE_PS 0
#endif

#if defined(INCLUDE_FS)
#define SMAA_INCLUDE_VS 0
#endif



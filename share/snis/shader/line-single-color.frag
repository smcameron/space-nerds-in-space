/*
 * Copyright © 2014 Jeremy Van Grinsven
 *
 * This file is part of Spacenerds In Space.

 * Spacenerds in Space is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Spacenerds in Space is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Spacenerds in Space; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Authors:
 *  Alexandros Frantzis (glmark2)
 */

uniform float u_DotSize;
uniform float u_DotPitch;
uniform vec4 u_LineColor;

varying float v_IsDotted;
varying vec4 v_Dist;

void main(void)
{
	float i = 1.0;

	if (v_IsDotted > 0.0) {
		// Get the distance from line end 0
		float d = (mod(v_Dist.x * v_Dist.w, u_DotPitch) - u_DotPitch/2.0) / u_DotSize;

		// Get the intensity of the wireframe line
		i = exp2(-2.0 * d * d);
	}

	gl_FragColor = mix(vec4(0.0), u_LineColor, i);
}


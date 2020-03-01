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

	Author:
		Jeremy Van Grinsven
*/


#if defined(INCLUDE_VS)
	varying vec3 v_Position;
	varying vec3 v_Normal;
	varying vec3 v_TexCoord;

	uniform mat4 u_MVPMatrix;  // A constant representing the combined model/view/projection matrix.
	uniform mat4 u_MVMatrix;   // A constant representing the combined model/view matrix.
	uniform mat3 u_NormalMatrix;

	attribute vec4 a_Position; // Per-vertex position information we will pass in.
	attribute vec3 a_Normal;   // Per-vertex normal information we will pass in.

	void main()
	{
		/* Transform the vertex into eye space. */
		v_Position = vec3(u_MVMatrix * a_Position);

		/* Transform the normal's orientation into eye space. */
		v_Normal = normalize(u_NormalMatrix * a_Normal);

		v_TexCoord = a_Position.xyz;

		/* gl_Position is a special variable used to store the final position.
		   Multiply the vertex by the matrix to get the final point in normalized screen coordinates. */
		gl_Position = u_MVPMatrix * a_Position;
	}
#endif

#if defined(INCLUDE_FS)
	varying vec3 v_Position;
	varying vec3 v_Normal;
	varying vec3 v_TexCoord;

	uniform samplerCube u_AlbedoTex;
	uniform vec4 u_TintColor;
	uniform vec3 u_LightPos;   // The position of the light in eye space.

	void main()
	{
		gl_FragColor = textureCube(u_AlbedoTex, v_TexCoord);

		/* tint with alpha pre multiply */
		gl_FragColor.rgb *= u_TintColor.rgb;
		gl_FragColor *= u_TintColor.a;
	}
#endif


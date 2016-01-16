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

#if !defined(AMBIENT)
#define AMBIENT 0.1
#endif

varying vec3 v_Position;
varying vec3 v_Normal;
varying vec3 v_Tangent;
varying vec3 v_BiTangent;
varying vec3 v_TexCoord;
varying mat3 tbn;

#if defined(INCLUDE_VS)
	uniform mat4 u_MVPMatrix;  // A constant representing the combined model/view/projection matrix.
	uniform mat4 u_MVMatrix;   // A constant representing the combined model/view matrix.
	uniform mat3 u_NormalMatrix;

	attribute vec4 a_Position; // Per-vertex position information we will pass in.
	attribute vec3 a_Normal;   // Per-vertex normal, tangent, and bitangent information we will pass in.
	attribute vec3 a_Tangent;
	attribute vec3 a_BiTangent;

	void main()
	{
		/* Transform the vertex into eye space. */
		v_Position = vec3(u_MVMatrix * a_Position);

		/* Transform the normal's, Tangent's and BiTangent's orientations into eye space. */
		v_Normal    = normalize(u_NormalMatrix * a_Normal);
		v_Tangent   = normalize(u_NormalMatrix * a_Tangent);
		v_BiTangent = normalize(u_NormalMatrix * a_BiTangent);

		tbn = mat3(v_Tangent, v_BiTangent, v_Normal);

		v_TexCoord = a_Position.xyz;

		/* gl_Position is a special variable used to store the final position.
		   Multiply the vertex by the matrix to get the final point in normalized screen coordinates. */
		gl_Position = u_MVPMatrix * a_Position;
	}
#endif

#if defined(INCLUDE_FS)
	uniform samplerCube u_AlbedoTex;
	uniform samplerCube u_NormalMapTex;
	uniform vec4 u_TintColor;
	uniform vec3 u_LightPos;   // The position of the light in eye space.

	void main()
	{
		/* Get a lighting direction vector from the light to the vertex. */
		vec3 light_dir = normalize(u_LightPos - v_Position);

		/* Calculate the dot product of the light vector and vertex normal. If the normal and light vector are
		   pointing in the same direction then it will get max illumination. */
		//float direct = dot(normalize(v_Normal), light_dir);

		vec3 pixel_normal = tbn * normalize(textureCube(u_NormalMapTex, v_TexCoord).xyz * 2.0 - 1.0);
		float normal_map_shadow = max(0.0, dot(pixel_normal, light_dir));

		/* make diffuse light atleast ambient */
		float diffuse = max(AMBIENT, normal_map_shadow);
		gl_FragColor = textureCube(u_AlbedoTex, v_TexCoord) * diffuse;

		/* tint with alpha pre multiply */
		gl_FragColor.rgb *= u_TintColor.rgb;
		gl_FragColor *= u_TintColor.a;
	}
#endif


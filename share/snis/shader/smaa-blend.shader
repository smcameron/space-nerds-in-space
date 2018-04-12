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

#if defined(INCLUDE_VS)
	out vec2 v_TexCoord;
	out vec2 v_PixCoord;
	out vec4 v_Offset[3];

	uniform mat4 u_MVPMatrix;

	in vec4 a_Position;
	in vec2 a_TexCoord;

	void main()
	{
		SMAABlendingWeightCalculationVS(a_TexCoord, v_PixCoord, v_Offset);

		v_TexCoord = a_TexCoord;
		gl_Position = u_MVPMatrix * a_Position;
	}
#endif

#if defined(INCLUDE_FS)
	in vec2 v_TexCoord;
	in vec2 v_PixCoord;
	in vec4 v_Offset[3];
	uniform sampler2D u_EdgeTex;
	uniform sampler2D u_AreaTex;
	uniform sampler2D u_SearchTex;

	void main()
	{
		gl_FragColor = SMAABlendingWeightCalculationPS(v_TexCoord, v_PixCoord, v_Offset,
			u_EdgeTex, u_AreaTex, u_SearchTex, ivec4(0));
	}
#endif


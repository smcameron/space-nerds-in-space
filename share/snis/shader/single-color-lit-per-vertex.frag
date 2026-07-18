
in vec3 v_Color;

out vec4 f_FragColor;

#ifdef USE_CSM
uniform sampler2DShadow u_ShadowMap;
uniform float u_Ambient;
in vec4 v_ShadowCoord;
in float v_LightLevel;
in vec3 v_BaseColor;

/* Returns 1.0 for fully lit, 0.0 for fully shadowed. */
float csm_shadow_factor()
{
	vec3 sc = v_ShadowCoord.xyz / v_ShadowCoord.w;
	sc = sc * 0.5 + 0.5;
	/* Fragments outside the shadow map or beyond its far plane are treated as lit. */
	if (sc.x < 0.0 || sc.x > 1.0 || sc.y < 0.0 || sc.y > 1.0 || sc.z > 1.0)
		return 1.0;
	return texture(u_ShadowMap, sc);
}
#endif

void main()
{
#ifdef USE_CSM
	float lit = csm_shadow_factor();
	float diffuse = max(v_LightLevel * lit, u_Ambient);
	f_FragColor = vec4(v_BaseColor * diffuse, 1.0);
#else
	f_FragColor = vec4(v_Color, 1);
#endif
	f_FragColor = filmic_tonemap(f_FragColor);
}


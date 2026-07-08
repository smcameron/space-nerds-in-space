uniform float u_FilmicTonemapping;
uniform float u_TonemappingGain;

vec4 filmic_tonemap(vec4 color) {
     float dont_tonemap = 1.0 - u_FilmicTonemapping;
     vec3 x = max(vec3(0.0), color.rgb - 0.004);
     x = u_TonemappingGain * (x * (6.2 * x + 0.5)) / (x * (6.2 * x + 1.7) + 0.06);
     return dont_tonemap * color + vec4(u_FilmicTonemapping * x, color.a);
}


#!/bin/bash
#

set -e

GLSLANGVALIDATOR=/usr/bin/glslangValidator

validate_es_vs() {
    echo "ES VS: $@"
    cat "share/snis/shader-es/filmic.glsl" "$@" | ${GLSLANGVALIDATOR} -l -DINCLUDE_VS --glsl-version 100 --stdin -S vert
}

validate_es_fs() {
    echo "ES FS: $@"
    cat "share/snis/shader-es/filmic.glsl" "$@" | ${GLSLANGVALIDATOR} -l -DINCLUDE_FS --glsl-version 100 "-Pprecision highp float;" --stdin -S frag
}

validate_es_both() {
    validate_es_vs "$@"
    validate_es_fs "$@"
}

validate_gl_vs() {
    echo "GS VS: $@"
    cat "share/snis/shader/filmic.glsl" "$@" | ${GLSLANGVALIDATOR} -l -DINCLUDE_VS --glsl-version 150 --stdin -S vert
}

validate_gl_fs() {
    echo "GS FS: $@"
    cat "share/snis/shader/filmic.glsl" "$@" | ${GLSLANGVALIDATOR} -l -DINCLUDE_FS --glsl-version 150 --stdin -S frag
}

validate_gl_both() {
    validate_gl_vs "$@"
    validate_gl_fs "$@"
}

cd "$(dirname "$0")/.."

# now actually validate.
validate_es_both "share/snis/shader-es/alpha_by_normal.shader"
validate_es_fs "share/snis/shader-es/atmosphere.frag"
validate_es_vs "share/snis/shader-es/atmosphere.vert"
validate_es_fs "share/snis/shader-es/color_by_w.frag"
validate_es_vs "share/snis/shader-es/color_by_w.vert"
validate_es_both "share/snis/shader-es/fs-effect-copy.shader"
validate_es_fs "share/snis/shader-es/line-single-color.frag"
validate_es_vs "share/snis/shader-es/line-single-color.vert"
validate_es_fs "share/snis/shader-es/per_vertex_color.frag"
validate_es_vs "share/snis/shader-es/per_vertex_color.vert"
validate_es_both "share/snis/shader-es/planetary-lightning.shader"
validate_es_fs "share/snis/shader-es/point_cloud.frag"
validate_es_vs "share/snis/shader-es/point_cloud.vert"
validate_es_fs "share/snis/shader-es/single_color.frag"
validate_es_fs "share/snis/shader-es/single-color-lit-per-pixel.frag"
validate_es_vs "share/snis/shader-es/single-color-lit-per-pixel.vert"
validate_es_fs "share/snis/shader-es/single-color-lit-per-vertex.frag"
validate_es_vs "share/snis/shader-es/single-color-lit-per-vertex.vert"
validate_es_vs "share/snis/shader-es/single_color.vert"
validate_es_fs "share/snis/shader-es/skybox.frag"
validate_es_vs "share/snis/shader-es/skybox.vert"
validate_es_both "share/snis/shader-es/smaa-high.shader" "share/snis/shader-es/SMAA.hlsl" "share/snis/shader-es/smaa-blend.shader"
validate_es_both "share/snis/shader-es/smaa-high.shader" "share/snis/shader-es/SMAA.hlsl" "share/snis/shader-es/smaa-edge.shader"
validate_es_both "share/snis/shader-es/smaa-high.shader" "share/snis/shader-es/SMAA.hlsl" "share/snis/shader-es/smaa-neighborhood.shader"
validate_es_both "share/snis/shader-es/textured-and-lit-per-pixel.shader"
validate_es_both "share/snis/shader-es/textured-and-lit-per-vertex.shader"
validate_es_both "share/snis/shader-es/textured-cubemap-and-lit-with-annulus-shadow-per-pixel.shader"
validate_es_both "share/snis/shader-es/textured-cubemap-shield-per-pixel.shader"
validate_es_fs "share/snis/shader-es/textured-particle.frag"
validate_es_vs "share/snis/shader-es/textured-particle.vert"
validate_es_both "share/snis/shader-es/textured.shader"
validate_es_both "share/snis/shader-es/textured-with-sphere-shadow-per-pixel.shader"
validate_es_both "share/snis/shader-es/warp-gate-effect.shader"
validate_es_fs "share/snis/shader-es/wireframe_filled.frag"
validate_es_vs "share/snis/shader-es/wireframe_filled.vert"
validate_es_fs "share/snis/shader-es/wireframe_transparent.frag"
validate_es_fs "share/snis/shader-es/wireframe-transparent-sphere-clip.frag"
validate_es_vs "share/snis/shader-es/wireframe-transparent-sphere-clip.vert"
validate_es_vs "share/snis/shader-es/wireframe_transparent.vert"

# now actually validate.
validate_gl_both "share/snis/shader/alpha_by_normal.shader"
validate_gl_fs "share/snis/shader/atmosphere.frag"
validate_gl_vs "share/snis/shader/atmosphere.vert"
validate_gl_fs "share/snis/shader/color_by_w.frag"
validate_gl_vs "share/snis/shader/color_by_w.vert"
validate_gl_both "share/snis/shader/fs-effect-copy.shader"
validate_gl_fs "share/snis/shader/line-single-color.frag"
validate_gl_vs "share/snis/shader/line-single-color.vert"
validate_gl_fs "share/snis/shader/per_vertex_color.frag"
validate_gl_vs "share/snis/shader/per_vertex_color.vert"
validate_gl_both "share/snis/shader/planetary-lightning.shader"
validate_gl_fs "share/snis/shader/point_cloud.frag"
validate_gl_vs "share/snis/shader/point_cloud.vert"
validate_gl_fs "share/snis/shader/single_color.frag"
validate_gl_fs "share/snis/shader/single-color-lit-per-pixel.frag"
validate_gl_vs "share/snis/shader/single-color-lit-per-pixel.vert"
validate_gl_fs "share/snis/shader/single-color-lit-per-vertex.frag"
validate_gl_vs "share/snis/shader/single-color-lit-per-vertex.vert"
validate_gl_vs "share/snis/shader/single_color.vert"
validate_gl_fs "share/snis/shader/skybox.frag"
validate_gl_vs "share/snis/shader/skybox.vert"
validate_gl_both "share/snis/shader/smaa-high.shader" "share/snis/shader/SMAA.hlsl" "share/snis/shader/smaa-blend.shader"
validate_gl_both "share/snis/shader/smaa-high.shader" "share/snis/shader/SMAA.hlsl" "share/snis/shader/smaa-edge.shader"
validate_gl_both "share/snis/shader/smaa-high.shader" "share/snis/shader/SMAA.hlsl" "share/snis/shader/smaa-neighborhood.shader"
validate_gl_both "share/snis/shader/textured-and-lit-per-pixel.shader"
validate_gl_both "share/snis/shader/textured-and-lit-per-vertex.shader"
validate_gl_both "share/snis/shader/textured-cubemap-and-lit-with-annulus-shadow-per-pixel.shader"
validate_gl_both "share/snis/shader/textured-cubemap-shield-per-pixel.shader"
validate_gl_fs "share/snis/shader/textured-particle.frag"
validate_gl_vs "share/snis/shader/textured-particle.vert"
validate_gl_both "share/snis/shader/textured.shader"
validate_gl_both "share/snis/shader/textured-with-sphere-shadow-per-pixel.shader"
validate_gl_both "share/snis/shader/warp-gate-effect.shader"
validate_gl_fs "share/snis/shader/wireframe_filled.frag"
validate_gl_vs "share/snis/shader/wireframe_filled.vert"
validate_gl_fs "share/snis/shader/wireframe_transparent.frag"
validate_gl_fs "share/snis/shader/wireframe-transparent-sphere-clip.frag"
validate_gl_vs "share/snis/shader/wireframe-transparent-sphere-clip.vert"
validate_gl_vs "share/snis/shader/wireframe_transparent.vert"


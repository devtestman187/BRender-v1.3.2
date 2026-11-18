##ifdef GL_ES
#version 300 es
precision highp float;
precision highp int;
##endif
##ifdef GL_CORE
#version 140
#extension GL_ARB_explicit_attrib_location:require
#extension GL_ARB_gpu_shader5:require
#extension GL_ARB_shading_language_packing:require
##endif

/*
 * AMD FidelityFX Super Resolution 1.0 - EASU (Edge Adaptive Spatial Upsampling)
 * pass. The EASU implementation is the vendored MIT-licensed AMD code in
 * fsr1/ffx_fsr1.h (see fsr1/LICENSE.txt); this file only provides the
 * platform glue: the input texture, the gather taps and the output position.
 *
 * Input  : the native resolution (640x480) composite scene texture (uSource),
 *          sampled with GL_NEAREST and no mipmaps.
 * Output : the upscaled frame at the present rect size (uniform con constants
 *          computed on the CPU by FsrEasuCon()).
 *
 * GLSL 1.40 / GLSL ES 3.00 have no textureGather (it needs GLSL 4.00 / ES
 * 3.10), so FSR's four gather4 taps are emulated with four texelFetch calls
 * each. The result order must match textureGather exactly:
 *   vec4(Sample_i0_j1, Sample_i1_j1, Sample_i1_j0, Sample_i0_j0)
 * with i0 = floor(P * size - 0.5). textureGather also applies the sampler's
 * CLAMP_TO_EDGE address mode, so each tap is clamped to the image.
 */

#define A_GPU 1
#define A_GLSL 1
#include "fsr1/ffx_a.h"

uniform sampler2D uSource;
uniform uvec4 uCon0;
uniform uvec4 uCon1;
uniform uvec4 uCon2;
uniform uvec4 uCon3;

layout (location = 0) out vec4 mainColour;

/* The four texels of the bilinear quad around 'p', for one channel. */
vec4 FsrGatherR(AF2 p)
{
    ivec2 size = textureSize(uSource, 0);
    ivec2 i0 = ivec2(floor(p * vec2(size) - 0.5));
    ivec2 hi = size - 1;
    return vec4(
        texelFetch(uSource, clamp(i0 + ivec2(0, 1), ivec2(0), hi), 0).r,
        texelFetch(uSource, clamp(i0 + ivec2(1, 1), ivec2(0), hi), 0).r,
        texelFetch(uSource, clamp(i0 + ivec2(1, 0), ivec2(0), hi), 0).r,
        texelFetch(uSource, clamp(i0 + ivec2(0, 0), ivec2(0), hi), 0).r);
}

vec4 FsrGatherG(AF2 p)
{
    ivec2 size = textureSize(uSource, 0);
    ivec2 i0 = ivec2(floor(p * vec2(size) - 0.5));
    ivec2 hi = size - 1;
    return vec4(
        texelFetch(uSource, clamp(i0 + ivec2(0, 1), ivec2(0), hi), 0).g,
        texelFetch(uSource, clamp(i0 + ivec2(1, 1), ivec2(0), hi), 0).g,
        texelFetch(uSource, clamp(i0 + ivec2(1, 0), ivec2(0), hi), 0).g,
        texelFetch(uSource, clamp(i0 + ivec2(0, 0), ivec2(0), hi), 0).g);
}

vec4 FsrGatherB(AF2 p)
{
    ivec2 size = textureSize(uSource, 0);
    ivec2 i0 = ivec2(floor(p * vec2(size) - 0.5));
    ivec2 hi = size - 1;
    return vec4(
        texelFetch(uSource, clamp(i0 + ivec2(0, 1), ivec2(0), hi), 0).b,
        texelFetch(uSource, clamp(i0 + ivec2(1, 1), ivec2(0), hi), 0).b,
        texelFetch(uSource, clamp(i0 + ivec2(1, 0), ivec2(0), hi), 0).b,
        texelFetch(uSource, clamp(i0 + ivec2(0, 0), ivec2(0), hi), 0).b);
}

AF4 FsrEasuRF(AF2 p) { return FsrGatherR(p); }
AF4 FsrEasuGF(AF2 p) { return FsrGatherG(p); }
AF4 FsrEasuBF(AF2 p) { return FsrGatherB(p); }

#define FSR_EASU_F 1
#include "fsr1/ffx_fsr1.h"

void main()
{
    AF3 c;
    FsrEasuF(c, uvec2(gl_FragCoord.xy), uCon0, uCon1, uCon2, uCon3);
    mainColour = vec4(c, 1.0);
}

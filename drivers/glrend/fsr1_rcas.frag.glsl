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
 * AMD FidelityFX Super Resolution 1.0 - RCAS (Robust Contrast Adaptive
 * Sharpening) pass. Run after EASU, at the output resolution. The RCAS
 * implementation is the vendored MIT-licensed AMD code in fsr1/ffx_fsr1.h
 * (see fsr1/LICENSE.txt); this file only provides the platform glue.
 *
 * Input  : the EASU output texture (uSource).
 * Output : written straight into the window framebuffer, so the output pixel
 *          position is gl_FragCoord minus the present rect origin.
 *
 * The present rect can start at a non-zero window position because the harness
 * letterboxes the 4:3 game view, so taps near the rect edge are clamped to the
 * EASU output image (textureGather/CLAMP_TO_EDGE equivalent).
 */

#define A_GPU 1
#define A_GLSL 1
#include "fsr1/ffx_a.h"

uniform sampler2D uSource;
uniform uvec4 uCon0;
uniform uvec2 uSourceRectOrigin;

layout (location = 0) out vec4 mainColour;

AF4 FsrRcasLoadF(ASU2 p)
{
    ivec2 hi = textureSize(uSource, 0) - 1;
    return texelFetch(uSource, clamp(p, ASU2(0), hi), 0);
}

void FsrRcasInputF(inout AF1 r, inout AF1 g, inout AF1 b)
{
}

#define FSR_RCAS_F 1
#include "fsr1/ffx_fsr1.h"

void main()
{
    AF1 r, g, b;
    FsrRcasF(r, g, b, uvec2(gl_FragCoord.xy) - uSourceRectOrigin, uCon0);
    mainColour = vec4(r, g, b, 1.0);
}

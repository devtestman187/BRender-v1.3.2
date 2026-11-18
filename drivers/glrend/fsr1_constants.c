/*
 * CPU-side FSR1 constant setup.
 *
 * FsrEasuCon() and FsrRcasCon() are the official AMD implementations from the
 * vendored MIT-licensed headers in fsr1/ (see fsr1/LICENSE.txt). They run once
 * per present, on the CPU, and are uploaded to the shaders as uvec4 uniforms.
 *
 * This is a separate translation unit so the large ffx_a.h CPU section does not
 * leak into the OpenGL code.
 */
#include "fsr1_constants.h"

#include <math.h>
#include <stdint.h>

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4505) /* unreferenced local function has been removed */
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#endif

#define A_CPU 1
#include "fsr1/ffx_a.h"
#include "fsr1/ffx_fsr1.h"

#if defined(_MSC_VER)
#pragma warning(pop)
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

void FSR1_EasuCon(uint32_t con0[4], uint32_t con1[4], uint32_t con2[4], uint32_t con3[4],
    float input_width, float input_height, float output_width, float output_height) {
    /*
     * The input viewport and the input image are the same size: we always
     * upscale the whole native-resolution scene texture.
     */
    FsrEasuCon(con0, con1, con2, con3, input_width, input_height, input_width, input_height, output_width,
        output_height);
}

void FSR1_RcasCon(uint32_t con[4], float sharpness) {
    FsrRcasCon(con, sharpness);
}

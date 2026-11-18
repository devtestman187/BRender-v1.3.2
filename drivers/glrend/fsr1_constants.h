/*
 * CPU-side FSR1 constant setup (see fsr1_constants.c).
 */
#ifndef FSR1_CONSTANTS_H_
#define FSR1_CONSTANTS_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* FsrEasuCon(): fills the four uvec4 constants used by the EASU pass. */
void FSR1_EasuCon(uint32_t con0[4], uint32_t con1[4], uint32_t con2[4], uint32_t con3[4],
    float input_width, float input_height, float output_width, float output_height);

/* FsrRcasCon(): fills the uvec4 constant used by the RCAS pass. */
void FSR1_RcasCon(uint32_t con[4], float sharpness);

#ifdef __cplusplus
};
#endif
#endif

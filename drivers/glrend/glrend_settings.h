/*
 * Central place for every glrend environment setting.
 *
 * All GLREND_* environment variables are read here (once, on first use) so a
 * future front end (dethrace.ini / command line) can be wired in by replacing
 * this one implementation instead of hunting getenv() calls across the driver.
 */
#ifndef GLREND_SETTINGS_H_
#define GLREND_SETTINGS_H_

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Present-path sharpen strength of the legacy (non-FSR) blit. The shader's
 * kernel sums to 1, so this only controls edge contrast (0 = pass-through,
 * ~0.3-0.5 = mild sharpening). Overridable with GLREND_SHARPEN_AMOUNT.
 */
float GLRendSettingSharpenAmount(void);

/*
 * FSR1 enable override:
 *   -1 = not set (caller decides; GL core defaults to enabled, GLES to off)
 *    0 = explicitly disabled (GLREND_FSR=0)
 *    1 = explicitly enabled
 */
int GLRendSettingFSRMode(void);

/* RCAS sharpness, in stops (0 = maximum sharpness, 2 = 1/4 sharpness). */
float GLRendSettingFSRSharpness(void);

#ifdef __cplusplus
};
#endif
#endif

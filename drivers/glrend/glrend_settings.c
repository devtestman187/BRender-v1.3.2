/*
 * Central place for every glrend environment setting.
 *
 * All GLREND_* environment variables are read here, lazily and once, so the
 * eventual front end (dethrace.ini / command line flags) only has to replace
 * this file instead of every call site.
 */
#include "glrend_settings.h"

#include <ctype.h>
#include <stdlib.h>

#define GLREND_SHARPEN_AMOUNT_DEFAULT 0.4f
#define GLREND_FSR_SHARPNESS_DEFAULT 0.2f

static int settings_loaded;
static float sharpen_amount;
static int fsr_mode;
static float fsr_sharpness;

static const char* env_value(const char* name) {
    const char* s = getenv(name);

    return (s != NULL && *s != '\0') ? s : NULL;
}

static int env_is_off(const char* s) {
    if (s == NULL)
        return 0;

    /* Accept 0, false, no, off as "disabled"; anything else is "enabled". */
    switch (tolower((unsigned char)s[0])) {
    case '0':
    case 'f':
    case 'n':
    case 'o':
        return 1;
    default:
        return 0;
    }
}

static void load_settings(void) {
    const char* s;

    if (settings_loaded)
        return;
    settings_loaded = 1;

    s = env_value("GLREND_SHARPEN_AMOUNT");
    sharpen_amount = (s != NULL) ? (float)atof(s) : GLREND_SHARPEN_AMOUNT_DEFAULT;

    s = env_value("GLREND_FSR_SHARPNESS");
    fsr_sharpness = (s != NULL) ? (float)atof(s) : GLREND_FSR_SHARPNESS_DEFAULT;

    s = env_value("GLREND_FSR");
    fsr_mode = (s == NULL) ? -1 : (env_is_off(s) ? 0 : 1);
}

float GLRendSettingSharpenAmount(void) {
    load_settings();
    return sharpen_amount;
}

int GLRendSettingFSRMode(void) {
    load_settings();
    return fsr_mode;
}

float GLRendSettingFSRSharpness(void) {
    load_settings();
    return fsr_sharpness;
}

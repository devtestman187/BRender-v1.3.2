/*
 * Texture replacement support for the glrend driver.
 *
 * The game's 3D textures are paletted (BR_PMT_INDEX_8) pixelmaps whose colours
 * come from the global device CLUT.  This module lets a user supply a
 * higher-resolution RGB image for a texture, keyed by the pixelmap's embedded
 * identifier, and uploads that image as a plain RGBA8 GL texture instead.
 *
 * Hook point: sbuffer.c:updateMemory(), the single place where a BRender
 * memory pixelmap becomes a GL texture.  Because the replacement leaves
 * paletted_source_dirty == BR_FALSE and palette_pointer == NULL, the bind-time
 * palette expansion in v1model.c is naturally skipped and no hot-path code is
 * touched.
 *
 * Environment variables (read once, glrend_settings.c style):
 *   GLREND_TEXTURE_DIR       replacement folder (default "textures", relative
 *                            to the game's working directory)
 *   GLREND_TEXTURE_DISABLE   any value starting with 0/f/n/o disables the
 *                            feature entirely (falls back to original paths)
 *   GLREND_TEXTURE_DEBUG     non-off value logs every identifier the hook sees
 *   GLREND_TEXTURE_MAX_SIZE  maximum replacement side in pixels (default 2048)
 *
 * Decoding is done by stb_image.h (v2.32, public domain / MIT; vendored next
 * to this file, see the license block at the end of that header).  Exactly
 * this translation unit compiles STB_IMAGE_IMPLEMENTATION.
 */
#include "drv.h"
#include "texture_replace.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <io.h>
#else
#include <dirent.h>
#endif

#define TEXTURE_REPLACE_DIR_DEFAULT "textures"
#define TEXTURE_REPLACE_MAX_SIZE_DEFAULT 2048
#define TEXTURE_REPLACE_PATH_MAX 1024
#define TEXTURE_REPLACE_NAME_MAX 512

static int settings_loaded;
static char replacement_dir[TEXTURE_REPLACE_PATH_MAX];
static int replacement_disabled;
static int debug_enabled;
static int max_side;

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

    s = env_value("GLREND_TEXTURE_DIR");
    if (s != NULL) {
        strncpy(replacement_dir, s, sizeof(replacement_dir) - 1);
        replacement_dir[sizeof(replacement_dir) - 1] = '\0';
    } else {
        strcpy(replacement_dir, TEXTURE_REPLACE_DIR_DEFAULT);
    }

    replacement_disabled = env_is_off(env_value("GLREND_TEXTURE_DISABLE"));

    s = env_value("GLREND_TEXTURE_DEBUG");
    debug_enabled = (s != NULL) ? !env_is_off(s) : 0;

    s = env_value("GLREND_TEXTURE_MAX_SIZE");
    max_side = (s != NULL) ? atoi(s) : TEXTURE_REPLACE_MAX_SIZE_DEFAULT;
    if (max_side <= 0)
        max_side = TEXTURE_REPLACE_MAX_SIZE_DEFAULT;
}

static int string_equals_ci(const char* a, const char* b) {
    while (*a != '\0' && *b != '\0') {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b))
            return 0;
        a++;
        b++;
    }

    return *a == '\0' && *b == '\0';
}

static int file_exists(const char* path) {
    FILE* f = fopen(path, "rb");

    if (f == NULL)
        return 0;
    fclose(f);
    return 1;
}

/*
 * Find "wanted_name" inside "dir", matching case-insensitively.  The direct
 * fopen() attempt in find_texture_file() already covers the common case
 * (exact name; and on Windows the whole case-insensitive filesystem), so this
 * scan only runs when that failed, i.e. mainly for Linux and mixed-case names.
 */
static int find_in_dir_ci(const char* dir, const char* wanted_name, char* out, size_t out_size) {
#if defined(_WIN32)
    struct _finddata_t entry;
    intptr_t handle;
    char pattern[TEXTURE_REPLACE_PATH_MAX];

    snprintf(pattern, sizeof(pattern), "%s/*", dir);
    handle = _findfirst(pattern, &entry);
    if (handle == -1)
        return 0;

    do {
        if ((entry.attrib & _A_SUBDIR) != 0)
            continue;
        if (string_equals_ci(entry.name, wanted_name)) {
            snprintf(out, out_size, "%s/%s", dir, entry.name);
            _findclose(handle);
            return 1;
        }
    } while (_findnext(handle, &entry) == 0);

    _findclose(handle);
#else
    DIR* d;
    struct dirent* ent;

    d = opendir(dir);
    if (d == NULL)
        return 0;

    while ((ent = readdir(d)) != NULL) {
        if (string_equals_ci(ent->d_name, wanted_name)) {
            snprintf(out, out_size, "%s/%s", dir, ent->d_name);
            closedir(d);
            return 1;
        }
    }

    closedir(d);
#endif

    return 0;
}

static int find_texture_file(const char* name, char* out, size_t out_size) {
    char path[TEXTURE_REPLACE_PATH_MAX];

    snprintf(path, sizeof(path), "%s/%s", replacement_dir, name);
    if (file_exists(path)) {
        snprintf(out, out_size, "%s", path);
        return 1;
    }

    return find_in_dir_ci(replacement_dir, name, out, out_size);
}

/*
 * Resolve <identifier> to an on-disk image.  Candidates, in order, for each of
 * PNG/TGA/BMP: the raw identifier plus the extension ("ROAD.PIX.png") and the
 * extension-stripped identifier plus the extension ("ROAD.png").
 */
static int resolve_replacement_path(const char* identifier, char* out, size_t out_size) {
    static const char* extensions[] = { ".png", ".tga", ".bmp" };
    char raw[TEXTURE_REPLACE_NAME_MAX];
    char stripped[TEXTURE_REPLACE_NAME_MAX];
    char candidate[TEXTURE_REPLACE_NAME_MAX + 8];
    size_t ext;
    char* dot;
    char* slash;
    char* last_slash;

    strncpy(raw, identifier, sizeof(raw) - 1);
    raw[sizeof(raw) - 1] = '\0';

    strncpy(stripped, raw, sizeof(stripped) - 1);
    stripped[sizeof(stripped) - 1] = '\0';

    dot = strrchr(stripped, '.');
    if (dot != NULL) {
        slash = strrchr(stripped, '/');
        last_slash = strrchr(stripped, '\\');
        if (last_slash != NULL && (slash == NULL || last_slash > slash))
            slash = last_slash;
        if (slash == NULL || dot > slash)
            *dot = '\0';
    }

    for (ext = 0; ext < sizeof(extensions) / sizeof(extensions[0]); ext++) {
        snprintf(candidate, sizeof(candidate), "%s%s", raw, extensions[ext]);
        if (find_texture_file(candidate, out, out_size))
            return 1;

        if (strcmp(stripped, raw) != 0) {
            snprintf(candidate, sizeof(candidate), "%s%s", stripped, extensions[ext]);
            if (find_texture_file(candidate, out, out_size))
                return 1;
        }
    }

    return 0;
}

br_boolean TextureReplacementUpload(br_buffer_stored* self, br_pixelmap* pm) {
    char path[TEXTURE_REPLACE_PATH_MAX];
    unsigned char* pixels;
    int width = 0;
    int height = 0;
    int channels = 0;
    br_boolean has_alpha = BR_FALSE;
    GLint gl_max_texture_size = 0;
    int side_limit;
    GLenum err;
    size_t pixel_count;
    size_t i;
    br_boolean created_texture = BR_FALSE;

    load_settings();
    if (replacement_disabled)
        return BR_FALSE;

    if (pm == NULL || pm->identifier == NULL || pm->identifier[0] == '\0')
        return BR_FALSE;

    /* Identifiers are plain names; refuse anything that looks like a path. */
    if (strchr(pm->identifier, '/') != NULL || strchr(pm->identifier, '\\') != NULL)
        return BR_FALSE;

    if (debug_enabled)
        BrLogPrintf("GLREND: texture_replace: seen '%s' %dx%d type=%d\n", pm->identifier, (int)pm->width,
            (int)pm->height, (int)pm->type);

    if (!resolve_replacement_path(pm->identifier, path, sizeof(path))) {
        if (debug_enabled)
            BrLogPrintf("GLREND: texture_replace: no image for '%s'\n", pm->identifier);
        return BR_FALSE;
    }

    pixels = stbi_load(path, &width, &height, &channels, 4);
    if (pixels == NULL) {
        BrLogPrintf("GLREND: texture_replace: failed to decode '%s': %s\n", path, stbi_failure_reason());
        return BR_FALSE;
    }

    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &gl_max_texture_size);
    side_limit = max_side;
    if (gl_max_texture_size > 0 && gl_max_texture_size < side_limit)
        side_limit = (int)gl_max_texture_size;

    if (width <= 0 || height <= 0 || width > side_limit || height > side_limit) {
        BrLogPrintf("GLREND: texture_replace: skipping '%s' (%dx%d; limit is %dx%d)\n", path, width, height,
            side_limit, side_limit);
        stbi_image_free(pixels);
        return BR_FALSE;
    }

    pixel_count = (size_t)width * (size_t)height;
    for (i = 0; i < pixel_count; i++) {
        if (pixels[i * 4 + 3] != 255) {
            has_alpha = BR_TRUE;
            break;
        }
    }

    if (self->gl_tex == 0) {
        glGenTextures(1, &self->gl_tex);
        created_texture = BR_TRUE;
    }
    if (self->gl_tex == 0) {
        BrLogPrintf("GLREND: texture_replace: glGenTextures() failed for '%s'\n", pm->identifier);
        stbi_image_free(pixels);
        return BR_FALSE;
    }

    glBindTexture(GL_TEXTURE_2D, self->gl_tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    /* No vertical flip: stb_image is top-down, same as the game's uploads. */
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    err = glGetError();
    if (err != 0) {
        BrLogPrintf("GLREND: texture_replace: glTexImage2D() failed (%u) for '%s'\n", (unsigned)err,
            pm->identifier);
        glBindTexture(GL_TEXTURE_2D, 0);
        if (created_texture) {
            glDeleteTextures(1, &self->gl_tex);
            self->gl_tex = 0;
        }
        stbi_image_free(pixels);
        return BR_FALSE;
    }
    glGenerateMipmap(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);
    stbi_image_free(pixels);

    /*
     * Skip the bind-time palette expansion in v1model.c: the GL texture holds
     * final RGBA texels now, so it must never be recoloured from the device
     * CLUT (that is the documented "no palette animation" trade-off).
     */
    self->paletted_source_dirty = BR_FALSE;
    self->palette_pointer = NULL;
    if (has_alpha == BR_TRUE)
        self->blended = BR_TRUE;

    BrLogPrintf("GLREND: texture_replace: loaded '%s' %s %dx%d (source %dx%d, alpha=%d)\n", pm->identifier, path,
        width, height, (int)pm->width, (int)pm->height, (int)has_alpha);

    if (pm->width > 0 && pm->height > 0 && height > 0) {
        double source_aspect = (double)pm->width / (double)pm->height;
        double replacement_aspect = (double)width / (double)height;

        if (replacement_aspect > source_aspect * 1.05 || replacement_aspect < source_aspect / 1.05) {
            BrLogPrintf("GLREND: texture_replace: WARNING '%s' aspect %.3f differs from source %.3f (will stretch)\n",
                pm->identifier, replacement_aspect, source_aspect);
        }
    }

    return BR_TRUE;
}

/*
 * Texture replacement support for the glrend driver.
 *
 * Lets a user drop higher-resolution images (PNG/TGA/BMP) into a "textures"
 * folder (overridable with the GLREND_TEXTURE_DIR environment variable) and
 * have them used in place of the game's original paletted (INDEX_8) textures.
 * The replacement is keyed by the pixelmap's embedded identifier, e.g.
 * "ROAD.PIX" or "bgltop.pix".
 */
#ifndef _TEXTURE_REPLACE_H_
#define _TEXTURE_REPLACE_H_

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Driver-local header: include it after "drv.h", which defines br_boolean and
 * pulls in br_buffer_stored/sbuffer.h for the driver's private types.
 */
struct br_buffer_stored;
struct br_pixelmap;

/*
 * If a replacement image exists for pm->identifier, decode it, upload it into
 * self->gl_tex as an RGBA8 texture (with mipmaps, GL_REPEAT) and leave the
 * stored buffer's palette-expansion state clear so the bind-time palette path
 * in v1model.c is skipped.  Returns BR_TRUE when the replacement was uploaded,
 * BR_FALSE to fall back to the original paletted upload path.
 */
br_boolean TextureReplacementUpload(struct br_buffer_stored* self, br_pixelmap* pm);

#ifdef __cplusplus
};
#endif

#endif

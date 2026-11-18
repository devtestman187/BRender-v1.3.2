#include "drv.h"
#include "brassert.h"

GLuint DeviceGLBuildWhiteTexture(void)
{
    const static uint8_t white_rgba[] = {255, 255, 255, 255};

    GLuint tex;

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, white_rgba);
    return tex;
}

GLuint DeviceGLBuildCheckerboardTexture(void)
{
    extern const uint8_t checkerboard_rgba[];

    GLuint tex;

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 64, 64, 0, GL_RGBA, GL_UNSIGNED_BYTE, checkerboard_rgba);

    return tex;
}

br_error DevicePixelmapGLBindFramebuffer(GLenum target, br_device_pixelmap *pm)
{
    GLuint fbo;

    if(pm == NULL)
        return BRE_FAIL;

    switch(pm->use_type) {
        case BRT_OFFSCREEN:
            fbo = pm->asBack.glFbo;
            break;
        case BRT_DEPTH:
            if(pm->asDepth.backbuffer == NULL)
                return BRE_FAIL;
            fbo = pm->asDepth.backbuffer->asBack.glFbo;
            break;
        default:
            return BRE_FAIL;
    }

    glBindFramebuffer(target, fbo);
    GL_CHECK_ERROR();
    return BRE_OK;
}

void DevicePixelmapGLGetFramebufferSize(br_device_pixelmap* self, br_int_32* width, br_int_32* height)
{
    int x, y;
    float width_multiplier, height_multiplier;

    if(self == NULL || width == NULL || height == NULL)
        return;

    DevicePixelmapGLGetViewport(self, &x, &y, &width_multiplier, &height_multiplier);

    /*
     * The logical pixelmap is presented through a centred, scaled viewport, so
     * recover the drawable size from the viewport origin and scale.
     */
    *width = (br_int_32)(self->pm_width * width_multiplier) + 2 * x;
    *height = (br_int_32)(self->pm_height * height_multiplier) + 2 * y;
}

/*
 * The rect, in window (drawable) pixels, that the logical pixelmap is presented
 * into. The harness letterboxes it to preserve the game's aspect ratio, e.g.
 * 240,0 + 1440x1080 inside a 1920x1080 window.
 */
void DevicePixelmapGLGetViewportRect(br_device_pixelmap* self, int* x, int* y, int* width, int* height)
{
    float width_multiplier, height_multiplier;

    if(self == NULL || x == NULL || y == NULL || width == NULL || height == NULL)
        return;

    DevicePixelmapGLGetViewport(self, x, y, &width_multiplier, &height_multiplier);

    *width = (int)(self->pm_width * width_multiplier);
    *height = (int)(self->pm_height * height_multiplier);
}

/*
 * Map a pixelmap into its off-screen framebuffer 1:1, in OpenGL viewport
 * coordinates ((0, 0) at the bottom left). The game renders at its native
 * logical resolution into the off-screen target and the present path scales the
 * finished frame up to the window, so no harness viewport scale is applied
 * here; only the sub-pixelmap offsets (e.g. the rear view mirror) remain.
 */
void DevicePixelmapGLGetRenderViewport(br_device_pixelmap* self, int* x, int* y, int* width, int* height)
{
    br_int_16 base_y = 0;

    if(self == NULL || x == NULL || y == NULL || width == NULL || height == NULL)
        return;

    if(self->pm_base_y != 0) {
        if(self->sub_pixelmap) {
            base_y = self->parent_height - self->pm_height - self->pm_base_y;
        } else {
            base_y = self->pm_height - self->pm_base_y;
        }
    }

    *x = self->pm_base_x;
    *y = base_y;
    *width = self->pm_width;
    *height = self->pm_height;
}

br_uint_8 DeviceGLTypeOrBits(br_uint_8 pixel_type, br_int_32 pixel_bits)
{
    if(pixel_type != BR_PMT_MAX)
        return pixel_type;

    switch(pixel_bits) {
        case 16:
            return BR_PMT_RGB_565;
        case 24:
            return BR_PMT_RGB_888;
        case 32:
            return BR_PMT_RGBX_888;
        default:
            break;
    }

    return BR_PMT_MAX;
}

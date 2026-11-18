/*
 * AMD FidelityFX Super Resolution 1.0 programs (EASU + RCAS).
 *
 * The filter implementations and constant setup functions come from the
 * vendored MIT-licensed AMD code in fsr1/ (see fsr1/LICENSE.txt); this file
 * compiles the platform shader wrappers and owns the two programs. The passes
 * are only used by the present path when the game is rendered at its native
 * 640x480 resolution and upscaled to the window.
 */
#include "drv.h"
#include "brassert.h"
#include "glrend_settings.h"
#include "fsr1_constants.h"

#include "post.vert.glsl.h"
#include "fsr1_easu.frag.glsl.h"
#include "fsr1_rcas.frag.glsl.h"

/*
 * Compile + link one FSR1 program. Unlike the core shaders, failure is not
 * fatal: a driver that cannot run FSR1 must fall back to the legacy present
 * path rather than dropping the game to the software renderer.
 */
static br_boolean compile_fsr1_program(HVIDEO hVideo, const char* name, const char* fragment_source,
    size_t fragment_size, GLuint* program) {
    GLuint vert, frag;

    (void)hVideo;

    vert = VIDEOI_CreateAndCompileShaderSoft("fsr1.vert", GL_VERTEX_SHADER, POST_VERT_GLSL, sizeof(POST_VERT_GLSL));
    if (vert == 0)
        return BR_FALSE;

    frag = VIDEOI_CreateAndCompileShaderSoft(name, GL_FRAGMENT_SHADER, fragment_source, fragment_size);
    if (frag == 0) {
        glDeleteShader(vert);
        return BR_FALSE;
    }

    *program = VIDEOI_CreateAndCompileProgramSoft(vert, frag);
    glDeleteShader(vert);
    glDeleteShader(frag);

    return *program != 0;
}

br_boolean VIDEOI_CompileFSRShaders(HVIDEO hVideo) {
    if (hVideo == NULL)
        return BR_FALSE;

    if (!compile_fsr1_program(hVideo, "fsr1_easu.frag", FSR1_EASU_FRAG_GLSL, sizeof(FSR1_EASU_FRAG_GLSL),
            &hVideo->fsrEasuProgram.program)) {
        return BR_FALSE;
    }

    if (!compile_fsr1_program(hVideo, "fsr1_rcas.frag", FSR1_RCAS_FRAG_GLSL, sizeof(FSR1_RCAS_FRAG_GLSL),
            &hVideo->fsrRcasProgram.program)) {
        glDeleteProgram(hVideo->fsrEasuProgram.program);
        hVideo->fsrEasuProgram.program = 0;
        return BR_FALSE;
    }

    hVideo->fsrEasuProgram.uSource = glGetUniformLocation(hVideo->fsrEasuProgram.program, "uSource");
    hVideo->fsrEasuProgram.uCon0 = glGetUniformLocation(hVideo->fsrEasuProgram.program, "uCon0");
    hVideo->fsrEasuProgram.uCon1 = glGetUniformLocation(hVideo->fsrEasuProgram.program, "uCon1");
    hVideo->fsrEasuProgram.uCon2 = glGetUniformLocation(hVideo->fsrEasuProgram.program, "uCon2");
    hVideo->fsrEasuProgram.uCon3 = glGetUniformLocation(hVideo->fsrEasuProgram.program, "uCon3");

    hVideo->fsrRcasProgram.uSource = glGetUniformLocation(hVideo->fsrRcasProgram.program, "uSource");
    hVideo->fsrRcasProgram.uCon0 = glGetUniformLocation(hVideo->fsrRcasProgram.program, "uCon0");
    hVideo->fsrRcasProgram.uSourceRectOrigin = glGetUniformLocation(hVideo->fsrRcasProgram.program, "uSourceRectOrigin");

    BrLogPrintf("GLREND: FSR1 shaders compiled (EASU + RCAS, sharpness %.2f stops)\n", GLRendSettingFSRSharpness());

    GL_CHECK_ERROR();
    return BR_TRUE;
}

/*
 * (Re)allocate the EASU output target at the present rect size. The texture is
 * 32bpp (RGBA8), which is what FSR1 wants for its intermediate image.
 */
static br_boolean ensure_fsr_target(HVIDEO hVideo, int width, int height) {
    GLenum status;

    if (hVideo->fsrTarget.fbo != 0 && hVideo->fsrTarget.width == width && hVideo->fsrTarget.height == height)
        return BR_TRUE;

    if (hVideo->fsrTarget.fbo != 0) {
        glDeleteFramebuffers(1, &hVideo->fsrTarget.fbo);
        hVideo->fsrTarget.fbo = 0;
    }
    if (hVideo->fsrTarget.tex != 0) {
        glDeleteTextures(1, &hVideo->fsrTarget.tex);
        hVideo->fsrTarget.tex = 0;
    }

    glGenFramebuffers(1, &hVideo->fsrTarget.fbo);
    glGenTextures(1, &hVideo->fsrTarget.tex);

    glBindTexture(GL_TEXTURE_2D, hVideo->fsrTarget.tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glBindTexture(GL_TEXTURE_2D, 0);

    glBindFramebuffer(GL_FRAMEBUFFER, hVideo->fsrTarget.fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, hVideo->fsrTarget.tex, 0);

    status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        BrLogPrintf("GLREND: FSR1 target framebuffer is not complete (0x%x), disabling FSR1.\n", status);
        glDeleteFramebuffers(1, &hVideo->fsrTarget.fbo);
        hVideo->fsrTarget.fbo = 0;
        glDeleteTextures(1, &hVideo->fsrTarget.tex);
        hVideo->fsrTarget.tex = 0;
        hVideo->fsrAvailable = BR_FALSE;
        return BR_FALSE;
    }

    hVideo->fsrTarget.width = width;
    hVideo->fsrTarget.height = height;
    BrLogPrintf("GLREND: FSR1 upscale target %dx%d (GL_RGBA8)\n", width, height);

    GL_CHECK_ERROR();
    return BR_TRUE;
}

br_boolean VIDEOI_FSRShouldPresent(HVIDEO hVideo, int input_width, int input_height, int output_width,
    int output_height) {
    if (hVideo == NULL || !hVideo->fsrAvailable)
        return BR_FALSE;

    /* No upscale to do, so the legacy 1:1 present path is the right one. */
    if (output_width <= input_width || output_height <= input_height)
        return BR_FALSE;

    return BR_TRUE;
}

br_boolean VIDEOI_FSRPresent(br_device_pixelmap* self, GLuint source_tex, int input_width, int input_height,
    int x, int y, int output_width, int output_height, float sharpness) {
    HVIDEO hVideo = &self->asFront.video;
    uint32_t easu_con0[4], easu_con1[4], easu_con2[4], easu_con3[4];
    uint32_t rcas_con[4];

    if (!ensure_fsr_target(hVideo, output_width, output_height))
        return BR_FALSE;

    FSR1_EasuCon(easu_con0, easu_con1, easu_con2, easu_con3, (float)input_width, (float)input_height,
        (float)output_width, (float)output_height);
    FSR1_RcasCon(rcas_con, sharpness);

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glBindVertexArray(self->asFront.screen_buffer_vao);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, self->asFront.screen_buffer_ebo);

    /*
     * EASU: native-resolution scene -> upscaled target. Both the source and the
     * target are bottom-up framebuffer textures, and gl_FragCoord is used as
     * the EASU output position, so the pass is orientation-consistent.
     */
    glBindFramebuffer(GL_FRAMEBUFFER, hVideo->fsrTarget.fbo);
    glViewport(0, 0, output_width, output_height);
    glBindTexture(GL_TEXTURE_2D, source_tex);
    glUseProgram(hVideo->fsrEasuProgram.program);
    glUniform1i(hVideo->fsrEasuProgram.uSource, 0);
    glUniform4ui(hVideo->fsrEasuProgram.uCon0, easu_con0[0], easu_con0[1], easu_con0[2], easu_con0[3]);
    glUniform4ui(hVideo->fsrEasuProgram.uCon1, easu_con1[0], easu_con1[1], easu_con1[2], easu_con1[3]);
    glUniform4ui(hVideo->fsrEasuProgram.uCon2, easu_con2[0], easu_con2[1], easu_con2[2], easu_con2[3]);
    glUniform4ui(hVideo->fsrEasuProgram.uCon3, easu_con3[0], easu_con3[1], easu_con3[2], easu_con3[3]);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

    /*
     * RCAS: upscaled target -> window framebuffer, at the letterboxed present
     * rect. This replaces the legacy unsharp pass when FSR1 is active.
     */
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(x, y, output_width, output_height);
    glBindTexture(GL_TEXTURE_2D, hVideo->fsrTarget.tex);
    glUseProgram(hVideo->fsrRcasProgram.program);
    glUniform1i(hVideo->fsrRcasProgram.uSource, 0);
    glUniform4ui(hVideo->fsrRcasProgram.uCon0, rcas_con[0], rcas_con[1], rcas_con[2], rcas_con[3]);
    glUniform2ui(hVideo->fsrRcasProgram.uSourceRectOrigin, (GLuint)x, (GLuint)y);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

    glBindTexture(GL_TEXTURE_2D, 0);
    glBindVertexArray(0);
    glEnable(GL_DEPTH_TEST);

    GL_CHECK_ERROR();
    return BR_TRUE;
}

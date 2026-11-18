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

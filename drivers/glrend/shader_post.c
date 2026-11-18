#include "drv.h"
#include "brassert.h"
#include "post.frag.glsl.h"
#include "post.vert.glsl.h"


/*
 * Present-path post-process program. Kept separate from defaultProgram so the
 * overlay composite path (which also uses the fullscreen quad) is unaffected.
 */
br_boolean VIDEOI_CompilePostShader(HVIDEO hVideo) {
    GLuint vert = VIDEOI_CreateAndCompileShader("post.vert", GL_VERTEX_SHADER, POST_VERT_GLSL, sizeof(POST_VERT_GLSL));
    if (!vert)
        return BR_FALSE;
    GLuint frag = VIDEOI_CreateAndCompileShader("post.frag", GL_FRAGMENT_SHADER, POST_FRAG_GLSL, sizeof(POST_FRAG_GLSL));
    if (!frag) {
        glDeleteShader(vert);
        return BR_FALSE;
    }
    hVideo->postProgram.program = VIDEOI_CreateAndCompileProgram(vert, frag);
    glDeleteShader(vert);
    glDeleteShader(frag);
    if (hVideo->postProgram.program) {
        hVideo->postProgram.aPosition = glGetAttribLocation(hVideo->postProgram.program, "aPosition");
        hVideo->postProgram.aColour = glGetAttribLocation(hVideo->postProgram.program, "aColour");
        hVideo->postProgram.aUV = glGetAttribLocation(hVideo->postProgram.program, "aUV");

        hVideo->postProgram.uSampler = glGetUniformLocation(hVideo->postProgram.program, "uSampler");
        hVideo->postProgram.uFlipVertically = glGetUniformLocation(hVideo->postProgram.program, "uFlipVertically");
        hVideo->postProgram.uSharpenAmount = glGetUniformLocation(hVideo->postProgram.program, "uSharpenAmount");
        glUseProgram(hVideo->postProgram.program);
    }

    GL_CHECK_ERROR();
    return hVideo->postProgram.program != 0;
}

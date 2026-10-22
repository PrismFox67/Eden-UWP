// SPDX-FileCopyrightText: Copyright 2026 Eden-UWP contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Draw-heavy workload for profiling the emulator's per-draw CPU cost (#190 item 4, #207).
//
// Ordinary open-source Switch homebrew (no keys, no firmware, no commercial content). Each frame
// issues DRAWS_PER_FRAME small instanced-free glDrawArrays calls, each with its own uniform update,
// so the guest command stream is dominated by draw + constant-buffer traffic - the path a real game
// stresses. Every second it logs "DRAW_STRESS fps=<n> draws=<n>" through svcOutputDebugString, and
// at the end "DRAW_STRESS_DONE". Press + to exit early.

#include <stdio.h>
#include <switch.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <glad/glad.h>

#define DRAWS_PER_FRAME 4000
#define RUN_SECONDS     60

static void Log(const char* s) { svcOutputDebugString(s, __builtin_strlen(s)); }

static EGLDisplay s_display;
static EGLContext s_context;
static EGLSurface s_surface;

static bool InitEgl(NWindow* win) {
    s_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (!s_display || !eglInitialize(s_display, NULL, NULL)) return false;
    eglBindAPI(EGL_OPENGL_API);
    static const EGLint fb_attr[] = {EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT, EGL_RED_SIZE, 8,
                                     EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8, EGL_NONE};
    EGLConfig config;
    EGLint n = 0;
    eglChooseConfig(s_display, fb_attr, &config, 1, &n);
    if (n == 0) return false;
    s_surface = eglCreateWindowSurface(s_display, config, win, NULL);
    if (!s_surface) return false;
    static const EGLint ctx_attr[] = {EGL_CONTEXT_OPENGL_PROFILE_MASK_KHR,
                                      EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT_KHR,
                                      EGL_CONTEXT_MAJOR_VERSION_KHR, 4,
                                      EGL_CONTEXT_MINOR_VERSION_KHR, 3, EGL_NONE};
    s_context = eglCreateContext(s_display, config, EGL_NO_CONTEXT, ctx_attr);
    if (!s_context) return false;
    eglMakeCurrent(s_display, s_surface, s_surface, s_context);
    return true;
}

static const char* kVs =
    "#version 330 core\n"
    "layout(location=0) in vec2 pos;\n"
    "uniform vec4 xform;\n" // xy offset, z scale
    "void main(){ gl_Position = vec4(pos*xform.z + xform.xy, 0.0, 1.0); }\n";
static const char* kFs =
    "#version 330 core\n"
    "uniform vec4 color;\n"
    "out vec4 o;\n"
    "void main(){ o = color; }\n";

static GLuint Compile(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, NULL);
    glCompileShader(s);
    return s;
}

int main(int argc, char** argv) {
    Log("DRAW_STRESS_START");
    if (!InitEgl(nwindowGetDefault())) { Log("DRAW_STRESS_EGL_FAIL"); return 1; }
    gladLoadGL();

    GLuint prog = glCreateProgram();
    glAttachShader(prog, Compile(GL_VERTEX_SHADER, kVs));
    glAttachShader(prog, Compile(GL_FRAGMENT_SHADER, kFs));
    glLinkProgram(prog);
    glUseProgram(prog);
    const GLint u_xform = glGetUniformLocation(prog, "xform");
    const GLint u_color = glGetUniformLocation(prog, "color");

    static const float quad[] = {-1, -1, 1, -1, -1, 1, 1, 1};
    GLuint vao, vbo;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(0);

    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    PadState pad;
    padInitializeDefault(&pad);

    const u64 freq = armGetSystemTickFreq();
    const u64 start = armGetSystemTick();
    u64 second_start = start;
    u32 frames = 0, frame_no = 0;
    char line[96];
    while (appletMainLoop()) {
        padUpdate(&pad);
        if (padGetButtonsDown(&pad) & HidNpadButton_Plus) break;

        glClearColor(0.05f, 0.05f, 0.08f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        for (u32 i = 0; i < DRAWS_PER_FRAME; ++i) {
            const u32 k = i * 2654435761u + frame_no * 40503u; // cheap hash: positions move every frame
            glUniform4f(u_xform, (float)(k & 1023) / 511.5f - 1.0f,
                        (float)((k >> 10) & 1023) / 511.5f - 1.0f, 0.01f, 0.0f);
            glUniform4f(u_color, (float)(k >> 24) / 255.0f, (float)((k >> 16) & 255) / 255.0f,
                        (float)(i & 255) / 255.0f, 1.0f);
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        }
        eglSwapBuffers(s_display, s_surface);
        ++frames;
        ++frame_no;

        const u64 now = armGetSystemTick();
        if (now - second_start >= freq) {
            snprintf(line, sizeof(line), "DRAW_STRESS fps=%u draws=%u", frames,
                     frames * DRAWS_PER_FRAME);
            Log(line);
            frames = 0;
            second_start = now;
        }
        if (now - start >= freq * RUN_SECONDS) break;
    }
    Log("DRAW_STRESS_DONE");

    eglMakeCurrent(s_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglDestroyContext(s_display, s_context);
    eglDestroySurface(s_display, s_surface);
    eglTerminate(s_display);
    return 0;
}

/*
 * Copyright (C) 2010 LunarG Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 *    Chia-I Wu <olv@lunarg.com>
 */

#ifndef EGLUT_H
#define EGLUT_H

#ifdef __cplusplus
extern "C" {
#endif

/* used by eglutInitAPIMask */
enum {
    EGLUT_OPENGL_BIT     = 0x1,
    EGLUT_OPENGL_ES1_BIT = 0x2,
    EGLUT_OPENGL_ES2_BIT = 0x4,
    EGLUT_OPENVG_BIT     = 0x8
};

/* used by EGLUTspecialCB */
enum {
    /* function keys */
            EGLUT_KEY_F1,
    EGLUT_KEY_F2,
    EGLUT_KEY_F3,
    EGLUT_KEY_F4,
    EGLUT_KEY_F5,
    EGLUT_KEY_F6,
    EGLUT_KEY_F7,
    EGLUT_KEY_F8,
    EGLUT_KEY_F9,
    EGLUT_KEY_F10,
    EGLUT_KEY_F11,
    EGLUT_KEY_F12,

    /* directional keys */
            EGLUT_KEY_LEFT,
    EGLUT_KEY_UP,
    EGLUT_KEY_RIGHT,
    EGLUT_KEY_DOWN,
};

/* used by eglutGet */
enum {
    EGLUT_ELAPSED_TIME,
    EGLUT_FULLSCREEN_MODE
};

/* used by EGLUTkeyboardCB */
enum {
    EGLUT_KEY_PRESS,
    EGLUT_KEY_RELEASE,
    EGLUT_KEY_REPEAT
};

/* used by EGLUTmouseButtonCB */
enum {
    EGLUT_MOUSE_PRESS,
    EGLUT_MOUSE_RELEASE
};

enum {
    EGLUT_POINTER_INVISIBLE = 0,
    EGLUT_POINTER_VISIBLE = 1
};

enum {
    EGLUT_POINTER_UNLOCKED = 0,
    EGLUT_POINTER_LOCKED = 1
};

enum {
    EGLUT_WINDOWED = 0,
    EGLUT_FULLSCREEN = 1
};

enum {
    EGLUT_NOT_FOCUSED = 0,
    EGLUT_FOCUSED = 1
};

typedef void (*EGLUTidleCB)(void);
typedef void (*EGLUTreshapeCB)(int, int);
typedef void (*EGLUTdisplayCB)(void);
typedef void (*EGLUTkeyboardCB)(char*, int);
typedef void (*EGLUTdropCB)(const char*);
typedef void (*EGLUTspecialCB)(int, int, unsigned int);
typedef void (*EGLUTpasteCB)(const char*, int);
typedef void (*EGLUTmouseCB)(int, int);
typedef void (*EGLUTmouseRawCB)(double, double);
typedef void (*EGLUTmouseButtonCB)(int, int, int, int);
typedef void (*EGLUTtouchStartCB)(int, double, double);
typedef void (*EGLUTtouchUpdateCB)(int, double, double);
typedef void (*EGLUTtouchEndCB)(int, double, double);
typedef void (*EGLUTfocusCB)(int);
typedef void (*EGLUTcloseCB)(void);

void eglutInitAPIMask(int mask);
void eglutInitWindowSize(int width, int height);
void eglutInit(int argc, char **argv);

int eglutGet(int state);

void eglutIdleFunc(EGLUTidleCB func);
void eglutPostRedisplay(void);

void eglutMainLoop(void);
void eglutFini(void);

int eglutCreateWindow(const char *title);
void eglutSetWindowIcon(const char *path);
void eglutShowWindow();
void eglutDestroyWindow(int win);
void eglutMakeCurrent(int win);
void eglutSwapInterval(int interval);
void eglutSwapBuffers();
void eglutPollEvents(void);

int eglutGetWindowX(void);
int eglutGetWindowY(void);
int eglutGetWindowWidth(void);
int eglutGetWindowHeight(void);
int eglutToggleFullscreen(void);

void eglutDisplayFunc(EGLUTdisplayCB func);
void eglutReshapeFunc(EGLUTreshapeCB func);
void eglutKeyboardFunc(EGLUTkeyboardCB func);
void eglutDropFunc(EGLUTdropCB func);
void eglutSpecialFunc(EGLUTspecialCB func);
void eglutPasteFunc(EGLUTpasteCB func);
void eglutMouseFunc(EGLUTmouseCB func);
void eglutMouseRawFunc(EGLUTmouseRawCB func);
void eglutMouseButtonFunc(EGLUTmouseButtonCB func);
void eglutTouchStartFunc(EGLUTtouchStartCB func);
void eglutTouchUpdateFunc(EGLUTtouchUpdateCB func);
void eglutTouchEndFunc(EGLUTtouchEndCB func);
void eglutFocusFunc(EGLUTfocusCB func);
void eglutCloseWindowFunc(EGLUTcloseCB func);

void eglutWarpMousePointer(int x, int y);
void eglutSetMousePointerVisiblity(int visible);
void eglutSetMousePointerLocked(int locked);
void eglutRequestPaste();
void eglutSetClipboardText(const char* value);
void eglutSetKeyboardState(int active);

#ifdef __cplusplus
}
#endif

#endif /* EGLUT_H */
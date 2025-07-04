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

#ifndef _EGLUTINT_H_
#define _EGLUTINT_H_

#include "EGL/egl.h"
#include "eglut.h"
#include "eglut_x11.h"

struct eglut_window {
    EGLConfig config;
    EGLContext context;

    /* initialized by native display */
    struct {
        union {
            EGLNativeWindowType window;
            EGLNativePixmapType pixmap;
            EGLSurface surface; /* pbuffer or screen surface */
        } u;
        int x, y;
        int width, height;
    } native;

    EGLSurface surface;

    int index;

    EGLUTreshapeCB reshape_cb;
    EGLUTdisplayCB display_cb;
    EGLUTkeyboardCB keyboard_cb;
    EGLUTdropCB drop_cb;
    EGLUTspecialCB special_cb;
    EGLUTpasteCB paste_cb;
    EGLUTmouseCB mouse_cb;
    EGLUTmouseRawCB mouse_raw_cb;
    EGLUTmouseButtonCB mouse_button_cb;
    EGLUTtouchStartCB touch_start_cb;
    EGLUTtouchUpdateCB touch_update_cb;
    EGLUTtouchEndCB touch_end_cb;
    EGLUTfocusCB focus_cb;
    EGLUTcloseCB close_cb;
    int keyboardstate;
};

struct eglut_state {
    int api_mask;
    int window_width, window_height;
    int window_fullscreen;
    const char *display_name;
    int verbose;
    int init_time;

    EGLUTidleCB idle_cb;

    int num_windows;

    /* initialized by native display */
    EGLNativeDisplayType native_dpy;
    EGLint surface_type;

    EGLDisplay dpy;
    EGLint major, minor;

    struct eglut_window *current;

    int redisplay;

    Atom XdndDrop;
    Atom XdndTypeList;
    Atom XdndSelection;
    Atom XdndEnter;
    Atom XdndPosition;
    Atom XdndStatus;
    Atom XdndLeave;
    Atom XdndFinished;
    Atom XdndActionCopy;
    Atom XtextUriList;
    int64_t dnd_source, dnd_version;
    int32_t dnd_format;
};

extern struct eglut_state *_eglut;

void _eglutFatal(char *format, ...);

int _eglutNow(void);

void _eglutNativeInitDisplay(void);

void _eglutNativeFiniDisplay(void);

void _eglutNativeInitWindow(struct eglut_window *win, const char *title,
                            int x, int y, int w, int h);

void _eglutNativeFiniWindow(struct eglut_window *win);

void _eglutNativeEventLoop(void);

void *
_eglutReadPNG(const char *filename, unsigned int *width, unsigned int *height);

#endif /* _EGLUTINT_H_ */
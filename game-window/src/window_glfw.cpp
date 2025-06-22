#include "window_glfw.h"
#include "game_window_manager.h"
#include "joystick_manager_glfw.h"

#include <codecvt>
#include <iomanip>
#include <thread>
#include <sstream>

#include <math.h>

GLFWGameWindow::GLFWGameWindow(const std::string& title, int width, int height, GraphicsApi api) : GameWindow(title, width, height, api), width(width), height(height), windowedWidth(width), windowedHeight(height) {
#ifdef GAMEWINDOW_X11_LOCK
    std::lock_guard<std::recursive_mutex> lock(x11_sync);
#endif
    glfwDefaultWindowHints();
    if(api == GraphicsApi::OPENGL_ES2) {
        glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
        glfwWindowHint(GLFW_CONTEXT_CREATION_API, GLFW_EGL_CONTEXT_API);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    } else if(api == GraphicsApi::OPENGL) {
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    }
    window = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
    if(window == nullptr && api == GraphicsApi::OPENGL_ES2) {
        // Failed to get es3 request es2
        glfwDefaultWindowHints();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
        glfwWindowHint(GLFW_CONTEXT_CREATION_API, GLFW_EGL_CONTEXT_API);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
        window = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
    }
    if(window == nullptr) {
        // Throw an exception, otherwise it would crash due to a nullptr without any information
        const char* error = nullptr;
        glfwGetError(&error);
        throw std::runtime_error(error == nullptr ? "GLFW failed to create a window without any error message" : error);
    }
    glfwSetWindowUserPointer(window, this);
    glfwSetFramebufferSizeCallback(window, _glfwWindowSizeCallback);
    glfwSetCursorPosCallback(window, _glfwCursorPosCallback);
    glfwSetMouseButtonCallback(window, _glfwMouseButtonCallback);
    glfwSetScrollCallback(window, _glfwScrollCallback);
    glfwSetWindowCloseCallback(window, _glfwWindowCloseCallback);
    glfwSetKeyCallback(window, _glfwKeyCallback);
    glfwSetCharCallback(window, _glfwCharCallback);
    glfwSetDropCallback(window, _glfwDropCallback);
    glfwSetWindowFocusCallback(window, _glfwWindowFocusCallback);
    glfwSetWindowContentScaleCallback(window, _glfwWindowContentScaleCallback);
    glfwMakeContextCurrent(window);

    setRelativeScale();

    lastFrame = std::chrono::system_clock::now();
}

void GLFWGameWindow::makeCurrent(bool c) {
#ifdef GAMEWINDOW_X11_LOCK
    std::lock_guard<std::recursive_mutex> lock(x11_sync);
#endif
    glfwMakeContextCurrent(c ? window : nullptr);
}

GLFWGameWindow::~GLFWGameWindow() {
#ifdef GAMEWINDOW_X11_LOCK
    std::lock_guard<std::recursive_mutex> lock(x11_sync);
#endif
    GLFWJoystickManager::removeWindow(this);
    glfwDestroyWindow(window);
}

void GLFWGameWindow::setIcon(std::string const& iconPath) {
    // TODO:
}

void GLFWGameWindow::setRelativeScale() {
#ifdef GAMEWINDOW_X11_LOCK
    std::lock_guard<std::recursive_mutex> lock(x11_sync);
#endif
    int fx, fy;
    glfwGetFramebufferSize(window, &fx, &fy);

    int wx, wy;
    glfwGetWindowSize(window, &wx, &wy);

    relativeScale = (((double)fx / (double)wx) + ((double)fy / (double)wy)) / 2.0;
    // Update window size to match content size mismatch
    width = fx;
    height = fy;
    resized = true;
}

double GLFWGameWindow::getRelativeScale() const {
    return relativeScale;
}

void GLFWGameWindow::getWindowSize(int& width, int& height) const {
    width = this->width;
    height = this->height;
}

void GLFWGameWindow::show() {
#ifdef GAMEWINDOW_X11_LOCK
    std::lock_guard<std::recursive_mutex> lock(x11_sync);
#endif
    GLFWJoystickManager::addWindow(this);
    glfwShowWindow(window);
}

void GLFWGameWindow::close() {
#ifdef GAMEWINDOW_X11_LOCK
    std::lock_guard<std::recursive_mutex> lock(x11_sync);
#endif
    onClose();
    glfwSetWindowShouldClose(window, GLFW_TRUE);
}

static std::string getModeDescription(const GLFWvidmode& mode) {
    std::stringstream desc;
    desc << mode.width << "x" << mode.height << " @ " << mode.refreshRate;
    return desc.str();
}

void GLFWGameWindow::pollEvents() {
#ifdef GAMEWINDOW_X11_LOCK
    std::lock_guard<std::recursive_mutex> lock(x11_sync);
#endif
    if((glfwGetWindowMonitor(window) != NULL) != requestFullscreen) {
        if(requestFullscreen) {
            glfwGetWindowPos(window, &windowedX, &windowedY);
            // convert pixels to window coordinates getRelativeScale() is 2 on macOS retina screens
            windowedWidth = (int)floor(width / getRelativeScale());
            windowedHeight = (int)floor(height / getRelativeScale());
            GLFWmonitor* monitor = glfwGetPrimaryMonitor();
            int nModes = 0;
            auto modes = glfwGetVideoModes(monitor, &nModes);
            if(mode.id != -1 && nModes > mode.id && mode.description == getModeDescription(modes[mode.id])) {
                glfwSetWindowMonitor(window, monitor, 0, 0, modes[mode.id].width, modes[mode.id].height, modes[mode.id].refreshRate);
                return;
            }
            const GLFWvidmode* mode = glfwGetVideoMode(monitor);
            glfwSetWindowMonitor(window, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
        } else {
            glfwSetWindowMonitor(window, nullptr, windowedX, windowedY, windowedWidth, windowedHeight, GLFW_DONT_CARE);
        }
    } else if(pendingFullscreenModeSwitch) {
        pendingFullscreenModeSwitch = false;
        int nModes = 0;
        auto display = glfwGetWindowMonitor(window);
        if(display) {
            auto modes = glfwGetVideoModes(display, &nModes);
            if(mode.id != -1 && nModes > mode.id && mode.description == getModeDescription(modes[mode.id])) {
                glfwSetWindowMonitor(window, display, 0, 0, modes[mode.id].width, modes[mode.id].height, modes[mode.id].refreshRate);
            }
        }
    }
    glfwPollEvents();
    if(resized) {
        onWindowSizeChanged(width, height);
        resized = false;
    }
    GLFWJoystickManager::update(this);
}

bool GLFWGameWindow::getCursorDisabled() {
    return glfwGetInputMode(window, GLFW_CURSOR) == GLFW_CURSOR_DISABLED;
}

void GLFWGameWindow::setCursorDisabled(bool disabled) {
#ifdef GAMEWINDOW_X11_LOCK
    std::lock_guard<std::recursive_mutex> lock(x11_sync);
#endif
    if(disabled) {
        if(glfwRawMouseMotionSupported())
            glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
        if(getenv("GAMEWINDOW_CENTER_CURSOR")) {
            glfwSetCursorPos(window, (width / 2) / getRelativeScale(), (height / 2) / getRelativeScale());
        }
    } else {
        if(glfwRawMouseMotionSupported())
            glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, GLFW_FALSE);
    }
    glfwSetInputMode(window, GLFW_CURSOR, disabled ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
    glfwGetCursorPos(window, &lastMouseX, &lastMouseY);
}

bool GLFWGameWindow::getFullscreen() {
    return glfwGetWindowMonitor(window) != NULL;
}

void GLFWGameWindow::setFullscreen(bool fullscreen) {
    // Bug macOS fullscreen doesn't work correctly on newer macOS
    // Ignore Fullscreen starting from now
#ifdef GAMEWINDOW_X11_LOCK
    std::lock_guard<std::recursive_mutex> lock(x11_sync);
#endif
    requestFullscreen = fullscreen;
}

void GLFWGameWindow::setClipboardText(std::string const& text) {
#ifdef GAMEWINDOW_X11_LOCK
    std::lock_guard<std::recursive_mutex> lock(x11_sync);
#endif
    glfwSetClipboardString(window, text.c_str());
}

void GLFWGameWindow::swapBuffers() {
#ifdef GAMEWINDOW_X11_LOCK
    std::lock_guard<std::recursive_mutex> lock(x11_sync);
#endif
#ifdef __APPLE__
    if(swapInterval > 0 && brokenVSync) {
        std::chrono::time_point<std::chrono::system_clock, std::chrono::nanoseconds> waitTime = lastFrame + swapInterval * std::chrono::nanoseconds(1000000000 / 120);
        lastFrame = std::chrono::system_clock::now();
        glfwSwapBuffers(window);
        std::this_thread::sleep_for(waitTime - lastFrame);
    } else if(swapInterval > 0 && checkBrokenVSync >= 0) {
        glfwSwapBuffers(window);
        if(lastFrame + std::chrono::seconds(5) < std::chrono::system_clock::now()) {
            checkBrokenVSync = -1;
        } else {
            checkBrokenVSync++;
            if(checkBrokenVSync > 256 * 5) {
                brokenVSync = true;
            }
        }
    } else {
#endif
        glfwSwapBuffers(window);
#ifdef __APPLE__
    }
#endif
}

void GLFWGameWindow::setSwapInterval(int interval) {
#ifdef GAMEWINDOW_X11_LOCK
    std::lock_guard<std::recursive_mutex> lock(x11_sync);
#endif
    glfwSwapInterval(interval);
    swapInterval = interval;
}

void GLFWGameWindow::_glfwWindowSizeCallback(GLFWwindow* window, int w, int h) {
    GLFWGameWindow* user = (GLFWGameWindow*)glfwGetWindowUserPointer(window);
    user->width = w;
    user->height = h;
    user->resized = true;
}

void GLFWGameWindow::_glfwCursorPosCallback(GLFWwindow* window, double x, double y) {
    GLFWGameWindow* user = (GLFWGameWindow*)glfwGetWindowUserPointer(window);

    if(glfwGetInputMode(window, GLFW_CURSOR) == GLFW_CURSOR_DISABLED) {
        double dx = (x - user->lastMouseX) * user->getRelativeScale();
        double dy = (y - user->lastMouseY) * user->getRelativeScale();

        user->onMouseRelativePosition(dx, dy);
        user->lastMouseX = x;
        user->lastMouseY = y;
    } else {
        x *= user->getRelativeScale();
        y *= user->getRelativeScale();

        user->onMousePosition(x, y);
    }
}

void GLFWGameWindow::_glfwMouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
    GLFWGameWindow* user = (GLFWGameWindow*)glfwGetWindowUserPointer(window);
    double x, y;
    glfwGetCursorPos(window, &x, &y);

    x *= user->getRelativeScale();
    y *= user->getRelativeScale();

    user->onMouseButton(x, y, button + (button > GLFW_MOUSE_BUTTON_3 ? 5 : 1), action == GLFW_PRESS ? MouseButtonAction::PRESS : MouseButtonAction::RELEASE);
}

void GLFWGameWindow::_glfwScrollCallback(GLFWwindow* window, double x, double y) {
    GLFWGameWindow* user = (GLFWGameWindow*)glfwGetWindowUserPointer(window);
    double cx, cy;
    glfwGetCursorPos(window, &cx, &cy);
    user->onMouseScroll(cx, cy, x, y);
}

KeyCode GLFWGameWindow::getKeyMinecraft(int keyCode) {
    if(keyCode >= GLFW_KEY_F1 && keyCode <= GLFW_KEY_F12)
        return (KeyCode)(keyCode - GLFW_KEY_F1 + (int)KeyCode::FN1);
    if(keyCode >= GLFW_KEY_KP_0 && keyCode <= GLFW_KEY_KP_9)
        return (KeyCode)(keyCode - GLFW_KEY_KP_0 + (int)KeyCode::NUMPAD_0);
    switch(keyCode) {
    case GLFW_KEY_BACKSPACE:
        return KeyCode::BACKSPACE;
    case GLFW_KEY_TAB:
        return KeyCode::TAB;
    case GLFW_KEY_ENTER:
        return KeyCode::ENTER;
    case GLFW_KEY_LEFT_SHIFT:
        return KeyCode::LEFT_SHIFT;
    case GLFW_KEY_RIGHT_SHIFT:
        return KeyCode::RIGHT_SHIFT;
    case GLFW_KEY_LEFT_CONTROL:
        return KeyCode::LEFT_CTRL;
    case GLFW_KEY_RIGHT_CONTROL:
        return KeyCode::RIGHT_CTRL;
    case GLFW_KEY_PAUSE:
        return KeyCode::PAUSE;
    case GLFW_KEY_CAPS_LOCK:
        return KeyCode::CAPS_LOCK;
    case GLFW_KEY_ESCAPE:
        return KeyCode::ESCAPE;
    case GLFW_KEY_PAGE_UP:
        return KeyCode::PAGE_UP;
    case GLFW_KEY_PAGE_DOWN:
        return KeyCode::PAGE_DOWN;
    case GLFW_KEY_END:
        return KeyCode::END;
    case GLFW_KEY_HOME:
        return KeyCode::HOME;
    case GLFW_KEY_LEFT:
        return KeyCode::LEFT;
    case GLFW_KEY_UP:
        return KeyCode::UP;
    case GLFW_KEY_RIGHT:
        return KeyCode::RIGHT;
    case GLFW_KEY_DOWN:
        return KeyCode::DOWN;
    case GLFW_KEY_INSERT:
        return KeyCode::INSERT;
    case GLFW_KEY_DELETE:
        return KeyCode::DELETE;
    case GLFW_KEY_NUM_LOCK:
        return KeyCode::NUM_LOCK;
    case GLFW_KEY_SCROLL_LOCK:
        return KeyCode::SCROLL_LOCK;
    case GLFW_KEY_SEMICOLON:
        return KeyCode::SEMICOLON;
    case GLFW_KEY_EQUAL:
        return KeyCode::EQUAL;
    case GLFW_KEY_COMMA:
        return KeyCode::COMMA;
    case GLFW_KEY_MINUS:
        return KeyCode::MINUS;
    case GLFW_KEY_PERIOD:
        return KeyCode::PERIOD;
    case GLFW_KEY_SLASH:
        return KeyCode::SLASH;
    case GLFW_KEY_GRAVE_ACCENT:
        return KeyCode::GRAVE;
    case GLFW_KEY_LEFT_BRACKET:
        return KeyCode::LEFT_BRACKET;
    case GLFW_KEY_BACKSLASH:
        return KeyCode::BACKSLASH;
    case GLFW_KEY_RIGHT_BRACKET:
        return KeyCode::RIGHT_BRACKET;
    case GLFW_KEY_APOSTROPHE:
        return KeyCode::APOSTROPHE;

    case GLFW_KEY_LEFT_SUPER:
        return KeyCode::LEFT_SUPER;
    case GLFW_KEY_RIGHT_SUPER:
        return KeyCode::RIGHT_SUPER;
    case GLFW_KEY_LEFT_ALT:
        return KeyCode::LEFT_ALT;
    case GLFW_KEY_RIGHT_ALT:
        return KeyCode::RIGHT_ALT;

    case GLFW_KEY_KP_ENTER:
        return KeyCode::ENTER;
    case GLFW_KEY_KP_SUBTRACT:
        return KeyCode::NUMPAD_SUBTRACT;
    case GLFW_KEY_KP_MULTIPLY:
        return KeyCode::NUMPAD_MULTIPLY;
    case GLFW_KEY_KP_ADD:
        return KeyCode::NUMPAD_ADD;
    case GLFW_KEY_KP_DIVIDE:
        return KeyCode::NUMPAD_DIVIDE;
    case GLFW_KEY_KP_DECIMAL:
        return KeyCode::NUMPAD_DECIMAL;
    }
    if(keyCode < 256)
        return (KeyCode)keyCode;
    return KeyCode::UNKNOWN;
}

void GLFWGameWindow::_glfwKeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    GLFWGameWindow* user = (GLFWGameWindow*)glfwGetWindowUserPointer(window);
    if(action == GLFW_PRESS || action == GLFW_REPEAT) {
        if(key == GLFW_KEY_BACKSPACE)
            user->onKeyboardText("\x08");
        if(key == GLFW_KEY_DELETE)
            user->onKeyboardText("\x7f");
        if(key == GLFW_KEY_ENTER)
            user->onKeyboardText("\n");
    }
    KeyAction enumAction = (action == GLFW_PRESS ? KeyAction::PRESS : (action == GLFW_REPEAT ? KeyAction::REPEAT : KeyAction::RELEASE));
    auto minecraftKey = getKeyMinecraft(key);
    int meta = translateMeta(mods);
    if(key != GLFW_KEY_UNKNOWN && minecraftKey != KeyCode::UNKNOWN) {
        user->onKeyboard(minecraftKey, enumAction, meta);
    }
#ifdef __APPLE__
    if(action == GLFW_PRESS && mods & GLFW_MOD_SUPER && key == GLFW_KEY_V) {
#else
    if(action == GLFW_PRESS && mods & GLFW_MOD_CONTROL && key == GLFW_KEY_V) {
#endif
        auto clipboardString = glfwGetClipboardString(window);
        if(clipboardString != nullptr)
            user->onPaste(clipboardString);
    }
#ifndef NDEBUG
    else {
        if(!user->warnedButtons) {
            user->warnedButtons = true;
            GameWindowManager::getManager()->getErrorHandler()->onError("GLFW Unknown Key", "Please check your Keyboard Layout. Falling back to scancode for unknown Keys.");
        }
        user->onKeyboard((KeyCode)scancode, enumAction, meta);
    }
#endif
}

void GLFWGameWindow::_glfwCharCallback(GLFWwindow* window, unsigned int ch) {
    GLFWGameWindow* user = (GLFWGameWindow*)glfwGetWindowUserPointer(window);
    std::wstring_convert<std::codecvt_utf8<char32_t>, char32_t> cvt;
    user->onKeyboardText(cvt.to_bytes(ch));
}

void GLFWGameWindow::_glfwDropCallback(GLFWwindow* window, int count, const char** paths) {
    GLFWGameWindow* user = (GLFWGameWindow*)glfwGetWindowUserPointer(window);
    for(int i = 0; i < count; i++) {
        user->onDrop(std::string(paths[i]));
    }
}

void GLFWGameWindow::_glfwWindowCloseCallback(GLFWwindow* window) {
    GLFWGameWindow* user = (GLFWGameWindow*)glfwGetWindowUserPointer(window);
    glfwSetWindowShouldClose(window, GLFW_FALSE);
    user->onClose();
}

void GLFWGameWindow::_glfwWindowFocusCallback(GLFWwindow* window, int focused) {
    GLFWGameWindow* user = (GLFWGameWindow*)glfwGetWindowUserPointer(window);
    GLFWJoystickManager::onWindowFocused(user, focused == GLFW_TRUE);
    user->focused = (focused == GLFW_TRUE);
}

void GLFWGameWindow::_glfwWindowContentScaleCallback(GLFWwindow* window, float scalex, float scaley) {
    GLFWGameWindow* user = (GLFWGameWindow*)glfwGetWindowUserPointer(window);
    user->setRelativeScale();
}

void GLFWGameWindow::setFullscreenMode(const FullscreenMode& mode) {
    this->mode = mode;
    pendingFullscreenModeSwitch = true;
}

std::vector<FullscreenMode> GLFWGameWindow::getFullscreenModes() {
    if(modes.empty()) {
        int nModes = 0;
        auto display = glfwGetPrimaryMonitor();
        auto modes = glfwGetVideoModes(display, &nModes);
        for(int j = 0; j < nModes; j++) {
            this->modes.emplace_back(FullscreenMode{.id = j, .description = getModeDescription(modes[j])});
        }
    }
    return modes;
}

FullscreenMode GLFWGameWindow::getFullscreenMode() {
    auto display = glfwGetPrimaryMonitor();
    int nModes;
    auto modes = glfwGetVideoModes(display, &nModes);
    auto mode = glfwGetVideoMode(display);
    if(mode && modes) {
        auto desc = getModeDescription(*mode);
        for(int i = 0; i < nModes; i++) {
            if(desc == getModeDescription(modes[i])) {
                return FullscreenMode{.id = i, .description = desc};
            }
        }
    }
    return FullscreenMode{-1};
}

int GLFWGameWindow::translateMeta(unsigned int meta) {
    int mods = 0;
    if(meta & GLFW_MOD_SHIFT) {
        mods |= KEY_MOD_SHIFT;
    }
    if(meta & GLFW_MOD_CONTROL) {
        mods |= KEY_MOD_CTRL;
    }
    if(meta & GLFW_MOD_ALT) {
        mods |= KEY_MOD_ALT;
    }
    if(meta & GLFW_MOD_SUPER) {
        mods |= KEY_MOD_SUPER;
    }
    if(meta & GLFW_MOD_CAPS_LOCK) {
        mods |= KEY_MOD_CAPSLOCK;
    }
    if(meta & GLFW_MOD_NUM_LOCK) {
        mods |= KEY_MOD_NUMLOCK;
    }
    return mods;
}
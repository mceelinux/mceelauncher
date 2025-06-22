#pragma once
#if !defined(GAMEWINDOW_NO_X11_LOCK) && !defined(GAMEWINDOW_X11_LOCK) && !defined(__APPLE__)
#define GAMEWINDOW_X11_LOCK
#endif

#include <game_window.h>
#include <mutex>
#include <SDL3/SDL.h>

class SDL3GameWindow : public GameWindow {
private:
    enum RequestWindowMode { Fullscreen,
                             Windowed,
                             None };
    SDL_Window* window;
    SDL_GLContext context;
    bool cursorDisabled = false;
    bool centerMouseNextEnable = false;
    int windowedX = -1, windowedY = -1;
    // width and height in content pixels
    int width = -1, height = -1;
    // width and height in window coordinates = pixels / relativeScale
    int windowedWidth = -1, windowedHeight = -1;
    int relativeScale = 1;
    double relativeScaleX = 1, relativeScaleY = 1;
    bool resized = false;
    bool focused = true;
    bool warnedButtons = false;
    RequestWindowMode requestedWindowMode = RequestWindowMode::None;
    bool pendingFullscreenModeSwitch = false;
    FullscreenMode mode;
    std::vector<FullscreenMode> modes;

    bool isMouseInWindow();
    static KeyCode getKeyMinecraft(int keyCode);
    static int translateMeta(SDL_Keymod meta);

public:
    SDL3GameWindow(const std::string& title, int width, int height, GraphicsApi api);

    ~SDL3GameWindow() override;

    void setIcon(std::string const& iconPath) override;

    void makeCurrent(bool active) override;

    int getRelativeScale() const;

    void setRelativeScale();

    void show() override;

    void close() override;

    void pollEvents() override;

    bool getCursorDisabled() override;

    void setCursorDisabled(bool disabled) override;

    bool getFullscreen() override;

    void setFullscreen(bool fullscreen) override;

    void getWindowSize(int& width, int& height) const override;

    void setClipboardText(std::string const& text) override;

    void swapBuffers() override;

    void setSwapInterval(int interval) override;

    void startTextInput() override;

    void stopTextInput() override;

    void setFullscreenMode(const FullscreenMode& mode) override;

    FullscreenMode getFullscreenMode() override;

    std::vector<FullscreenMode> getFullscreenModes() override;

    int getKeyFromKeyCode(KeyCode code, int metaState) override;
};

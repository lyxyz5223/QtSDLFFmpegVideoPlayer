#pragma once
#include <SDL3/SDL_surface.h>
#include <SDL3/SDL_video.h>
#include <SDL3/SDL.h>
#ifdef _WIN32
#include <wtypes.h>
#include <string>
typedef HWND SDLWindowHandle;
#define SDLWindowHandleInvalid 0
#elif defined(__APPLE__)
typedef void* SDLWindowHandle;
#define SDLWindowHandleInvalid nullptr
#else // Linux and others
typedef uint32_t SDLWindowHandle;
#define SDLWindowHandleInvalid 0
#endif

class SDLWidget
{
private:
    SDL_Window* m_window = nullptr;
    SDLWindowHandle m_parentWinId = 0;
    SDL_Renderer* m_renderer = nullptr;

public:
    // SDL_GetRenderDriver
    // \param renderDriverName 渲染器名称，如"direct3d11"、"opengl"等，传入空字符串表示SDL自动选择
    SDLWidget(SDLWidget* parent, const std::string& renderDriverName = "")
        : m_parentWinId(parent->m_parentWinId) {
        if (parent && parent->m_parentWinId != SDLWindowHandleInvalid)
        {
            SDL_PropertiesID props = SDL_CreateProperties();
            SDL_SetPointerProperty(props, SDL_PROP_WINDOW_CREATE_WIN32_HWND_POINTER, (void*)parent->m_parentWinId);
            m_window = SDL_CreateWindowWithProperties(props);
            if (m_window)
                m_renderer = SDL_CreateRenderer(m_window, renderDriverName.size() ? renderDriverName.c_str() : nullptr);
        }
    }
    SDLWidget(SDLWindowHandle parent, const std::string& renderDriverName = "") : m_parentWinId(parent) {
        if (parent != SDLWindowHandleInvalid)
        {
            SDL_PropertiesID props = SDL_CreateProperties();
            SDL_SetPointerProperty(props, SDL_PROP_WINDOW_CREATE_WIN32_HWND_POINTER, (void*)parent);
            m_window = SDL_CreateWindowWithProperties(props);
            if (m_window)
                m_renderer = SDL_CreateRenderer(m_window, renderDriverName.size() ? renderDriverName.c_str() : nullptr);
        }
    }
    ~SDLWidget() {
        if (m_renderer)
            SDL_DestroyRenderer(m_renderer);
        if (m_window)
            SDL_DestroyWindow(m_window);
    }
    bool isWindowValid() const {
        return m_window;
    }
    bool isRendererValid() const {
        return m_renderer;
    }
    // 重新创建渲染器
    bool recreateRenderer(const std::string& renderDriverName = "") {
        if (!m_window)
            return false;
        if (m_renderer)
            SDL_DestroyRenderer(m_renderer);
        m_renderer = SDL_CreateRenderer(m_window, renderDriverName.size() ? renderDriverName.c_str() : nullptr);
        return m_renderer;
    }
    // 重新创建窗口，注意：重新创建窗口会销毁现有的渲染器和窗口
    bool recreateWindow() {
        if (m_renderer)
            SDL_DestroyRenderer(m_renderer);
        if (m_window)
            SDL_DestroyWindow(m_window);
        if (m_parentWinId == SDLWindowHandleInvalid)
            return false;
        SDL_PropertiesID props = SDL_CreateProperties();
        SDL_SetPointerProperty(props, SDL_PROP_WINDOW_CREATE_WIN32_HWND_POINTER, (void*)m_parentWinId);
        m_window = SDL_CreateWindowWithProperties(props);
        return m_window;
    }
    SDL_Window* getSDLWindow() const {
        return m_window;
    }
    SDL_Renderer* getSDLRenderer() const {
        return m_renderer;
    }
    void show() {
        if (m_window)
            SDL_ShowWindow(m_window);
    }
    void hide() {
        if (m_window)
            SDL_HideWindow(m_window);
    }
    void raise() {
        if (m_window)
            SDL_RaiseWindow(m_window);
    }
    void setResiable(bool resiable) {
        if (m_window)
            SDL_SetWindowResizable(m_window, resiable);
    }
    void setWindowTitle(const std::string& title) {
        if (m_window)
            SDL_SetWindowTitle(m_window, title.c_str());
    }
    void getWindowTitle() const {
        if (m_window)
            SDL_GetWindowTitle(m_window);
    }
    void setWindowSize(int width, int height) {
        if (m_window)
            SDL_SetWindowSize(m_window, width, height);
    }
    void getWindowSize(int& width, int& height) const {
        if (m_window)
            SDL_GetWindowSize(m_window, &width, &height);
    }
    void getWindowSizeInPixels(int& width, int& height) const {
        if (m_window)
            SDL_GetWindowSizeInPixels(m_window, &width, &height);
    }
    void resize(int w, int h, SDL_RendererLogicalPresentation mode) {
        if (m_window) {
            SDL_SetWindowSize(m_window, w, h);
        }
        //if (m_renderer)
        //    SDL_SetRenderLogicalPresentation(m_renderer, w, h, mode);
    }
    void setAlwaysOnTop(bool onTop) {
        if (m_window) {
            SDL_SetWindowAlwaysOnTop(m_window, onTop);
        }
    }
};


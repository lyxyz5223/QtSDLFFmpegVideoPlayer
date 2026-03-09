#include "SDLWidget.h"
#include <QGuiApplication>
#include <qnativeinterface.h>

#ifdef _WIN32

#elif defined(__linux__) // Linux
#include <xcb/xcb.h>
#include <X11/Xlib.h>
// #ifdef None
// #undef None
// #endif
// #ifdef Unsorted
// #undef Unsorted
// #endif

#elif defined(__APPLE__)

#else

#endif

SDLWidget::SDLWidget(SDLWidget* parent, const std::string& renderDriverName)
    : m_parentWinId(parent->m_parentWinId)
{
    if (parent && parent->m_parentWinId != SDLWindowHandleInvalid)
    {
        SDL_PropertiesID props = SDL_CreateProperties();
        SDL_SetPointerProperty(props, SDL_PROP_WINDOW_CREATE_WIN32_HWND_POINTER, reinterpret_cast<void*>(parent->m_parentWinId));
        m_window = SDL_CreateWindowWithProperties(props);
        if (m_window)
            m_renderer = SDL_CreateRenderer(m_window, renderDriverName.size() ? renderDriverName.c_str() : nullptr);
    }
}

SDLWidget::SDLWidget(long parent, const std::string& renderDriverName)
    : m_parentWinId((SDLWindowHandle)parent)
{
    SDLWindowHandle hp = (SDLWindowHandle)parent;
    if (hp != SDLWindowHandleInvalid)
    {
        SDL_PropertiesID props = SDL_CreateProperties();
#ifdef _WIN32
        bool rst = SDL_SetPointerProperty(props, SDL_PROP_WINDOW_CREATE_WIN32_HWND_POINTER, reinterpret_cast<void*>(parent));
#elif defined(__linux__)
        // Linux 平台 - 使用 X11
        bool rstX11 = SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_X11_WINDOW_NUMBER, parent);
        bool rstWayland = SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_WAYLAND_WL_SURFACE_POINTER, parent);
        // 如果需要指定父窗口的display
        // Display *display = nullptr;
        // xcb_connection_t *connection = nullptr;
        // bool isPlatformX11 = false;
        // if (auto *x11Application = qGuiApp->nativeInterface<QNativeInterface::QX11Application>())
        // {
        //     display = x11Application->display();
        //     connection = x11Application->connection();
        //     isPlatformX11 = true;
        // }
#elif defined(__APPLE__)
        // macOS 平台
        bool rst = SDL_SetPointerProperty(props, SDL_PROP_WINDOW_CREATE_COCOA_WINDOW_POINTER, reinterpret_cast<void*>(parent));
#endif
        m_window = SDL_CreateWindowWithProperties(props);
        if (m_window)
            m_renderer = SDL_CreateRenderer(m_window, renderDriverName.size() ? renderDriverName.c_str() : nullptr);
    }
}

#include "SDLApp.h"

std::mutex SDLApp::m_mutexInstance;
SDLApp* SDLApp::m_instance = nullptr;

std::vector<std::string> SDLApp::appArgs;

bool SDLApp::init(int argc, char** argv, SDL_InitFlags initFlags)
{
    for (int i = 0; i < argc; i++)
        appArgs.emplace_back(argv[i]);
    if (!SDL_Init(initFlags))
        return false;
    return true;
}

void SDLApp::pushEvent(SDL_Event& e)
{
    SDL_assert(e.type < SDL_EVENT_USER || e.type >= UserEvent);
    if (e.type >= SDL_EVENT_USER && e.type < UserEvent)
    {
        SDL_Log("Error: Pushing user event with e.type >= SDL_EVENT_USER && e.type < UserEvent is not allowed.");
        throw std::runtime_error("Error: Pushing user event with e.type >= SDL_EVENT_USER && e.type < UserEvent is not allowed.");
        return;
    }
    SDL_PushEvent(&e);
}

void SDLApp::pushEventFunction(UserCallFunction function, void* userData, SDL_WindowID winId)
{
    SDL_Event e;
    SDL_zero(e);
    e.type = InternalUserEventType(InternalUserEventType::FunctionCall);
    e.user.timestamp = SDL_GetTicksNS();
    e.user.code = 0;
    e.user.data1 = new UserCallFunction(function);
    e.user.data2 = userData;
    e.user.windowID = winId;
    SDL_PushEvent(&e);
}


void SDLApp::runOnMainThread(UserSimpleCallFunction function, void* userData, bool waitComplete)
{
    struct Data {
        UserSimpleCallFunction func;
        void* userData;
    } *data = new Data{
        function,
        userData
    };
    SDL_RunOnMainThread([](void* userData) {
        auto* p = static_cast<Data*>(userData);
        if (p)
        {
            p->func(p->userData);
            delete p;
        }
        }, data, waitComplete);
}

SDLApp::EventHandlerResult SDLApp::AppEventHandler(const SDL_Event& event)
{
    EventHandlerResult result;
    switch (event.type) {
    case SDL_EVENT_QUIT:
    {
        result.appExit = true;
        break;
    }
    case SDL_EVENT_KEY_DOWN:
    {
        SDL_Log("Key down: scancode=%d, keycode=%d\n", event.key.scancode, event.key.key);
        break;
    }
    case SDL_EVENT_KEY_UP:
    {
        SDL_Log("Key up: scancode=%d, keycode=%d\n", event.key.scancode, event.key.key);
        break;
    }
    default:
        break;
    }
    return result;
}

bool SDLApp::createWindowAndRenderer(const std::string& title, int width, int height, SDL_WindowFlags windowFlags)
{
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    bool ret = SDL_CreateWindowAndRenderer(title.c_str(), width, height, windowFlags, &window, &renderer);
    if (!m_window)
    {
        m_window = window;
        m_renderer = renderer;
    }
    return ret;
}

void SDLApp::runEventLoop()
{
    if (m_eventLoopRunning) // 已经在运行
        return;
    m_shouldExit = false;
    m_eventLoopRunning = true;
    SDL_Event e;
    //while (SDL_WaitEvent(&e))
    while (!m_shouldExit)
    {
        if (!SDL_WaitEvent(&e))
            continue;
        if (m_shouldExit)
            break;
        //SDL_Log("Event Type: %u", e.type);
        if (e.type == SDL_EVENT_QUIT)
            break;
        else
        {
            bool handled = false;
            try {
                if (e.type >= SDL_EVENT_USER && e.type < SDLApp::UserEvent)
                {
                    switch (e.type)
                    {
                    case InternalUserEventType::FunctionCall:
                    {
                        if (e.user.data1)
                        {
                            auto func = static_cast<UserCallFunction*>(e.user.data1);
                            (*func)(e.user.data2, e.user.windowID);
                            delete func;
                            handled = true;
                        }
                        break;
                    }
                    default:
                        break;
                    }
                }
            }
            catch (...)
            {
                SDL_Log("Exception occurred while handling user event.");
            }
            if (!handled)
            {
                auto r = AppEventHandler(e);
                if (r.appExit)
                    break;
            }
        }
    }
    m_eventLoopRunning = false;

    std::lock_guard lock(m_mtxEventLoopExited);
    m_shouldExit = false;
    m_cvEventLoopExited.notify_all();
}

void SDLApp::stopEventLoop()
{
    m_shouldExit = true;
    SDL_Event e;
    e.type = SDL_EVENT_QUIT;
    pushEvent(e);
    std::unique_lock lock(m_mtxEventLoopExited);
    m_cvEventLoopExited.wait(lock, [&]() { return !m_eventLoopRunning; });
    m_shouldExit = false;
}
void SDLApp::stopEventLoopAsync()
{
    m_shouldExit = true;
    SDL_Event e;
    e.type = SDL_EVENT_QUIT;
    pushEvent(e);
}
SDL_WindowID SDLApp::getWindowId(SDL_Window* window)
{
    if (!window)
        return 0;
    return SDL_GetWindowID(window);
}

SDL_WindowID SDLApp::getMainWindowId() const
{
    if (!m_window)
        return 0;
    return SDL_GetWindowID(m_window);
}
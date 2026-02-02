#pragma once
#ifndef SDLApp_H
#define SDLApp_H

//#include "SingletonObject.h"
#include <vector>
#include <mutex>
#include <condition_variable>
#include <string>
#include <queue>
#include <atomic>
#include <any>
#include <thread>
#include <functional>
#include <sstream>


#include <SDL3/SDL.h>


class SDLApp;


// 单例模式的SDL应用程序类
class SDLApp
{
private:
    struct EventHandlerResult {
        bool appExit = false;
    };

    class InternalUserEventType {
    public:
        enum Type {
            FunctionCall = SDL_EVENT_USER,
        };
        static constexpr int typeCount = 1;
    private:
        Type type = FunctionCall;
    public:
        InternalUserEventType() {}
        InternalUserEventType(const InternalUserEventType& t) : type(t.type) {}
        InternalUserEventType(Type t) : type(t) {}
        int constexpr count() const {
            return typeCount;
        }
        operator Type() const {
            return type;
        }
        operator SDL_EventType() const {
            return static_cast<SDL_EventType>(type);
        }
        operator Uint32() const {
            return static_cast<Uint32>(type);
        }
    };

    static std::vector<std::string> appArgs;
    

    SDLApp() {

    }

    ~SDLApp() {

    }
public:
    static constexpr Uint32 UserEvent = static_cast<Uint32>(SDL_EVENT_USER + InternalUserEventType::typeCount);
    typedef std::function<void(void*, SDL_WindowID)> UserCallFunction;
    typedef std::function<void(void*)> UserSimpleCallFunction;

private:
    static std::mutex m_mutexInstance;
    static SDLApp* m_instance;
public:
    static SDLApp* instance() {
        std::lock_guard<std::mutex> lock(m_mutexInstance);
        if (!m_instance)
            m_instance = new SDLApp();
        return m_instance;
    }
    static void destroyInstance() {
        std::lock_guard<std::mutex> lock(m_mutexInstance);
        if (m_instance)
        {
            delete m_instance;
            m_instance = nullptr;
        }
    }


public:
    static bool init(int argc, char** argv, SDL_InitFlags initFlags);

    static auto& getAppArgs() {
        return appArgs;
    }
    static void uninit() {
        SDL_Quit();
    }

    // 事件传递
    // \param e 要推送的事件
    // \note 事件e如果是用户事件，请使用当前类中的UserEvent，或者将其递增，否则将与当前类的内部实现矛盾，可能会导崩溃，调用该函数将会判断边界并提示错误
    // \note 使用该函数而不是SDL_PushEvent的原因是该函数会对用户事件类型进行边界检查
    // \note 该函数是线程安全的，可以在任何线程中调用
    static void pushEvent(SDL_Event& e);

    /**
     * 在事件循环线程中执行一个函数
     * \param function 要执行的函数
     * \param winId 关联的窗口ID，默认为0，表示不关联特定窗口
     */
    static void pushEventFunction(UserCallFunction function, void* userData = nullptr, SDL_WindowID winId = 0);

    static void runInEventLoopThread(UserCallFunction function, void* userData = nullptr) {
        pushEventFunction(function, userData, 0);
    }
    static void runOnMainThread(UserSimpleCallFunction function, void* userData = nullptr, bool waitComplete = true);

private:
    //class A {
    //public:
    //    explicit A(int, std::string) {}
    //    void t() {
    //        SDL_MessageBoxData data;
    //        SDL_MessageBoxButtonData buttons[2];
    //        int buttonid;
    //        // 设置按钮
    //        buttons[0] = SDL_MessageBoxButtonData{
    //            SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT,
    //            0, "确定"
    //        };
    //        buttons[1] = SDL_MessageBoxButtonData{
    //            SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT,
    //            1, "取消"
    //        };
    //        // 设置消息框数据
    //        data = SDL_MessageBoxData{
    //            SDL_MESSAGEBOX_INFORMATION,    // 标志
    //            NULL,                          // 父窗口
    //            "提示",                        // 标题
    //            "这是一个简单的消息框示例",    // 消息内容
    //            SDL_arraysize(buttons),        // 按钮数量
    //            buttons,                       // 按钮数组
    //            NULL                           // 颜色方案（使用默认）
    //        };
    //        // 显示消息框
    //        if (SDL_ShowMessageBox(&data, &buttonid) < 0) {
    //            SDL_Log("显示消息框失败: %s", SDL_GetError());
    //        }
    //        else {
    //            SDL_Log("用户点击了按钮: %d", buttonid);
    //        }
    //    }
    //};
    SDL_Window* m_window = nullptr;
    SDL_Renderer* m_renderer = nullptr;
    std::atomic<bool> m_shouldExit = false;
    std::mutex m_mtxEventLoopExited;
    std::condition_variable m_cvEventLoopExited;
    std::atomic<bool> m_eventLoopRunning = false;
    //std::mutex m_mtxShouldExit;
    //std::condition_variable m_cvShouldExit;
    //ThreadSafeVariable<std::atomic<bool>> m_shouldExit{false};

    EventHandlerResult AppEventHandler(const SDL_Event& event);
public:
    bool createWindow(const std::string& title, int width, int height, SDL_WindowFlags flags) {
        SDL_Window* window = SDL_CreateWindow(title.c_str(), width, height, flags);
        if (window)
        {
            if (!m_window)
                m_window = window;
        }
        else
            return false;
        return true;
    }
    bool createRenderer(const std::string& name) {
        SDL_Renderer* renderer = SDL_CreateRenderer(m_window, name.c_str());
        if (renderer)
        {
            if (!m_renderer)
                m_renderer = renderer;
        }
        else
            return false;
        return true;
    }
    bool createWindowAndRenderer(const std::string& title, int width, int height, SDL_WindowFlags windowFlags);
    
    // 运行事件循环，只能运行一遍
    void runEventLoop(bool waitMode = false);
    // 停止所有事件循环
    void stopEventLoop();
    void stopEventLoopAsync();

    static SDL_WindowID getWindowId(SDL_Window* window);
    SDL_WindowID getMainWindowId() const;

};


#endif // !SDLApp_H

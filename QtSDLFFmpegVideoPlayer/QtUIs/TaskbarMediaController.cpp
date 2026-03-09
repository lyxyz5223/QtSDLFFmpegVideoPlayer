#include "TaskbarMediaController.h"
#ifdef _WIN32
#include "Win32TaskbarMediaController.h"
using PlatformTaskbarMediaController = Win32TaskbarMediaController;
#elif defined(__linux__)
#include "LinuxTaskbarMediaController.h"
using PlatformTaskbarMediaController = LinuxTaskbarMediaController;
#elif defined(__APPLE__)
#error "Taskbar media control is not implemented for macOS. You can implement it using NSStatusBar and related APIs."
#else
#error "Unsupported platform for TaskbarMediaController."
#endif

class TaskbarMediaController::Impl {
    PlatformTaskbarMediaController pc;
    friend class TaskbarMediaController;
public:
    Impl() {}
    ~Impl() {}
};

TaskbarMediaController::TaskbarMediaController()
    : pImpl(std::make_unique<Impl>())
{
}

TaskbarMediaController::~TaskbarMediaController()
{
}

bool TaskbarMediaController::initialize(long winId)
{
    return pImpl->pc.initialize(winId);
}

void TaskbarMediaController::addThumbBarButtons(const std::vector<ThumbBarButton>& btnList)
{
    return pImpl->pc.addThumbBarButtons(btnList);
}

void TaskbarMediaController::updateThumbBarButton(ThumbBarButton newBtn)
{
    return pImpl->pc.updateThumbBarButton(newBtn);
}
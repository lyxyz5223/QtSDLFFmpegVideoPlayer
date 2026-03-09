#include "SystemVolumeController.h"

#ifdef _WIN32
#include "Win32SystemVolumeController.h"
using PlatformSystemVolumeController = Win32SystemVolumeController;
#else
// #error "Unsupported platform for SystemVolumeController"
#include "LinuxSystemVolumeController.h"
using PlatformSystemVolumeController = LinuxSystemVolumeController;
#endif

class SystemVolumeController::Impl {
    friend class SystemVolumeController;
    PlatformSystemVolumeController platformController;
public:
    Impl() = default;
    ~Impl() = default;
};

float SystemVolumeController::getSystemMasterVolume() const
{
    return pImpl->platformController.getSystemMasterVolume();
}

void SystemVolumeController::setSystemMasterVolume(float volume)
{
    return pImpl->platformController.setSystemMasterVolume(volume);
}

float SystemVolumeController::getSystemMixerVolume() const
{
    return pImpl->platformController.getSystemMixerVolume();
}

void SystemVolumeController::setSystemMixerVolume(float volume)
{
    return pImpl->platformController.setSystemMixerVolume(volume);
}

bool SystemVolumeController::setVolumeChangeCallback(VolumeChangeCallback callback)
{
    return pImpl->platformController.setVolumeChangeCallback(callback);
}


SystemVolumeController::SystemVolumeController() : pImpl(std::make_unique<Impl>())
{
}

SystemVolumeController::~SystemVolumeController()
{
}

#include "SystemVolumeController.h"
#include <LoggerPredefine.h>
#include <algorithm>

class LinuxSystemVolumeController : public ISystemVolumeController
{
    DefineLoggerSinks(LinuxSystemVolumeControllerLoggerSinks, "LinuxSystemVolumeController");
    mutable Logger logger{ "LinuxSystemVolumeController", LinuxSystemVolumeControllerLoggerSinks };
    VolumeChangeCallback volumeChangeCallback = nullptr;

    bool init() { return true; }

    // 清理资源
    void cleanUp() {}
public:
    ~LinuxSystemVolumeController() { cleanUp(); }
    LinuxSystemVolumeController() { init(); }
    virtual float getSystemMasterVolume() const {
        return 0.0f;
    } // 获取系统音量，返回值范围0-100
    virtual void setSystemMasterVolume(float volume) {} // 设置系统音量，参数volume范围0-100
    virtual float getSystemMixerVolume() const {
        return 0.0f;
    } // 获取音量合成器的音量，返回值范围0-100
    virtual void setSystemMixerVolume(float volume) {} // 设置音量合成器的音量，参数volume范围0-100
    virtual bool setVolumeChangeCallback(VolumeChangeCallback callback) {
        return true;
    }
};
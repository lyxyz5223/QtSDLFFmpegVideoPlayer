#pragma once
#include <memory>
#include <functional>

struct ISystemVolumeController {
    enum VolumeDeviceType {
        Master,
        Mixer
    };
    using VolumeChangeCallback = std::function<void(VolumeDeviceType type, float volume, bool muted)>;
    virtual ~ISystemVolumeController() = default;
    virtual float getSystemMasterVolume() const = 0; // 获取系统音量，返回值范围0-1
    virtual void setSystemMasterVolume(float volume) = 0; // 设置系统音量，参数volume范围0-1
    virtual float getSystemMixerVolume() const = 0; // 获取音量合成器的音量，返回值范围0-1
    virtual void setSystemMixerVolume(float volume) = 0; // 设置音量合成器的音量，参数volume范围0-1
    virtual bool setVolumeChangeCallback(VolumeChangeCallback callback) = 0;
};

class SystemVolumeController : public ISystemVolumeController {
    class Impl;
    std::unique_ptr<Impl> pImpl{ nullptr };
public:
    explicit SystemVolumeController();
    ~SystemVolumeController();
    virtual float getSystemMasterVolume() const override;
    virtual void setSystemMasterVolume(float volume) override;
    virtual float getSystemMixerVolume() const override;
    virtual void setSystemMixerVolume(float volume) override;
    virtual bool setVolumeChangeCallback(VolumeChangeCallback callback) override;
};
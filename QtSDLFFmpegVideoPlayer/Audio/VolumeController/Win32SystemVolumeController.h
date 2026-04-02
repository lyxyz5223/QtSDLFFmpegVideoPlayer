#include "SystemVolumeController.h"
#include <Windows.h>
#include <mmdeviceapi.h>
#include <endpointvolume.h> // 系统主音量
#include <audiopolicy.h> // 音量合成器
#include <LoggerPredefine.h>
#include <COMUtils.h>
#include <algorithm>

class Win32SystemVolumeController : public ISystemVolumeController
{
    DefineLoggerSinks(win32SystemVolumeControllerLoggerSinks, "Win32SystemVolumeController");
    mutable Logger logger{ "Win32SystemVolumeController", win32SystemVolumeControllerLoggerSinks };
    IAudioEndpointVolume* endpointVolume = nullptr;
    IMMDeviceEnumerator* devEnumerator = nullptr;
    IMMDevice* device = nullptr;
    VolumeChangeCallback volumeChangeCallback = nullptr;
    class AudioEndpointVolumeCallback : public IAudioEndpointVolumeCallback {
        IUnknownImpl impl{ false };
        Win32SystemVolumeController& parent;
    public:
        AudioEndpointVolumeCallback(Win32SystemVolumeController& parent) : parent(parent) {}
        virtual HRESULT STDMETHODCALLTYPE OnNotify(PAUDIO_VOLUME_NOTIFICATION_DATA pNotify) {
            parent.logger.trace("Master volume changed, new volume: {}", pNotify->fMasterVolume);
            pNotify->fMasterVolume;
            if (parent.volumeChangeCallback)
                parent.volumeChangeCallback(Master, std::clamp(pNotify->fMasterVolume, 0.0f, 1.0f), pNotify->bMuted);
            return S_OK;
        }
        // IUnknown 方法实现
        HRESULT __stdcall QueryInterface(const IID& iid, void** ppv) override {
            if (iid == __uuidof(IAudioEndpointVolumeCallback))
            {
                *ppv = this;
                return S_OK;
            }
            return impl.QueryInterface(iid, ppv);
        }
        ULONG __stdcall AddRef() override { return impl.AddRef(); }
        ULONG __stdcall Release() override {
            ULONG ref = impl.Release();
            if (ref == 0)
                delete this;
            return ref;
        }
    }* cbInst = nullptr;
    friend class AudioEndpointVolumeCallback;
    
    // 音量合成器
    IAudioSessionManager2* sessionManager = nullptr;
    // 存储所有会话及其回调
    struct SessionInfo {
        IAudioSessionControl* control = nullptr;
        //IAudioSessionControl2* control2 = nullptr;
        IAudioSessionEvents* events = nullptr;
        ISimpleAudioVolume* volumeControl = nullptr;
    };
    SessionInfo session;

    class AudioSessionEvents : public IAudioSessionEvents {
        IUnknownImpl impl{ false };
        Win32SystemVolumeController& parent;
    public:
        AudioSessionEvents(Win32SystemVolumeController& parent) : parent(parent) {}
        // IUnknown 方法实现
        HRESULT __stdcall QueryInterface(const IID& iid, void** ppv) override {
            if (iid == __uuidof(IAudioSessionEvents))
            {
                *ppv = this;
                return S_OK;
            }
            return impl.QueryInterface(iid, ppv);
        }
        ULONG __stdcall AddRef() override { return impl.AddRef(); }
        ULONG __stdcall Release() override {
            ULONG ref = impl.Release();
            if (ref == 0)
                delete this;
            return ref;
        }

        virtual HRESULT STDMETHODCALLTYPE OnDisplayNameChanged(/* [annotation][string][in] */ _In_  LPCWSTR NewDisplayName, /* [in] */ LPCGUID EventContext) override {
            return S_OK;
        }

        virtual HRESULT STDMETHODCALLTYPE OnIconPathChanged(/* [annotation][string][in] */ _In_  LPCWSTR NewIconPath, /* [in] */ LPCGUID EventContext) override {
            return S_OK;
        }

        virtual HRESULT STDMETHODCALLTYPE OnSimpleVolumeChanged(/* [annotation][in] */ _In_  float NewVolume, /* [annotation][in] */ _In_  BOOL NewMute, /* [in] */ LPCGUID EventContext) override {
            if (parent.volumeChangeCallback)
                parent.volumeChangeCallback(Mixer, NewVolume, NewMute);
            return S_OK;
        }

        virtual HRESULT STDMETHODCALLTYPE OnChannelVolumeChanged(/* [annotation][in] */ _In_  DWORD ChannelCount, /* [annotation][size_is][in] */ _In_reads_(ChannelCount)  float NewChannelVolumeArray[], /* [annotation][in] */ _In_  DWORD ChangedChannel, /* [in] */ LPCGUID EventContext) override {
            return S_OK;
        }

        virtual HRESULT STDMETHODCALLTYPE OnGroupingParamChanged(/* [annotation][in] */ _In_  LPCGUID NewGroupingParam, /* [in] */ LPCGUID EventContext) override {
            return S_OK;
        }

        virtual HRESULT STDMETHODCALLTYPE OnStateChanged(/* [annotation][in] */ _In_  AudioSessionState NewState) override {
            return S_OK;
        }

        virtual HRESULT STDMETHODCALLTYPE OnSessionDisconnected(/* [annotation][in] */ _In_  AudioSessionDisconnectReason DisconnectReason) override {
            return S_OK;
        }

    };

    //bool EnumerateAndRegisterCurrentProcessSession() {
    //    auto targetPid = GetCurrentProcessId();
    //    IAudioSessionEnumerator* pSessionEnumerator = nullptr;
    //    HRESULT hr = sessionManager->GetSessionEnumerator(&pSessionEnumerator);
    //    if (!checkAndLogErrorHRESULT("SessionManager->GetSessionEnumerator", hr)) return false;
    //    int sessionCount = 0;
    //    hr = pSessionEnumerator->GetCount(&sessionCount);
    //    if (!checkAndLogErrorHRESULT("SessionEnumerator->GetCount", hr)) return false;
    //    for (int i = 0; i < sessionCount; i++)
    //    {
    //        IAudioSessionControl* pSessionControl = nullptr;
    //        HRESULT hr = pSessionEnumerator->GetSession(i, &pSessionControl);
    //        if (!checkAndLogErrorHRESULT("SessionEnumerator->GetSession", hr)) continue;
    //        IAudioSessionControl2* pSessionControl2 = nullptr;
    //        hr = pSessionControl->QueryInterface(__uuidof(IAudioSessionControl2), (void**)&pSessionControl2);
    //        if (!checkAndLogErrorHRESULT("SessionControl2->QueryInterface", hr)) continue;
    //        DWORD pid = 0;
    //        hr = pSessionControl2->GetProcessId(&pid);
    //        if (!checkAndLogErrorHRESULT("SessionControl2->GetProcessId", hr)) continue;
    //        if (pid != targetPid) continue;
    //        // 创建回调
    //        AudioSessionEvents* pEvents = new AudioSessionEvents(*this);
    //        // 注册通知
    //        hr = pSessionControl->RegisterAudioSessionNotification(pEvents);
    //        if (!checkAndLogErrorHRESULT("SessionControl->RegisterAudioSessionNotification", hr))
    //        {
    //            pEvents->Release();
    //            continue;
    //        }
    //        // 存储信息
    //        sessions.emplace_back(pSessionControl, pSessionControl2, pEvents, pid);
    //        logger.info("Successfully register audio session notification for PID: {}", pid);
    //        return true;
    //    }
    //    pSessionEnumerator->Release();
    //    return false;
    //}

    bool registerSession();
    // 返回true代表hr成功
    bool checkAndLogErrorHRESULT(const std::string& action, HRESULT hr) const {
        if (FAILED(hr))
        {
            logger.error("{} error，HRESULT: {:#X}", action, static_cast<unsigned long>(hr));
            return false;
        }
        return true;
    }

    bool init();

    // 清理资源
    void cleanUp();
public:
    ~Win32SystemVolumeController() { cleanUp(); }
    Win32SystemVolumeController() { init(); }
    virtual float getSystemMasterVolume() const; // 获取系统音量，返回值范围0-100
    virtual void setSystemMasterVolume(float volume); // 设置系统音量，参数volume范围0-100
    virtual float getSystemMixerVolume() const; // 获取音量合成器的音量，返回值范围0-100
    virtual void setSystemMixerVolume(float volume); // 设置音量合成器的音量，参数volume范围0-100
    virtual bool setVolumeChangeCallback(VolumeChangeCallback callback);
};
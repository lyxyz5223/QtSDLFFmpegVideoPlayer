#include "Win32SystemVolumeController.h"

bool Win32SystemVolumeController::registerSession()
{
    if (!session.control) return false;
    // 创建回调
    AudioSessionEvents* pEvents = new AudioSessionEvents(*this);
    // 注册通知
    HRESULT hr = session.control->RegisterAudioSessionNotification(pEvents);
    if (!checkAndLogErrorHRESULT("SessionControl->RegisterAudioSessionNotification", hr))
    {
        pEvents->Release();
        return false;
    }
    // 存储信息
    session.events = pEvents;
    logger.info("Successfully registered audio session notification.");
    return true;
}

bool Win32SystemVolumeController::init()
{
    // 创建设备枚举器
    HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL,
        CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void**)&devEnumerator);
    if (!checkAndLogErrorHRESULT("Create device enumerator", hr)) return false;
    // 获取默认渲染设备（扬声器/耳机）
    hr = devEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device);
    if (!checkAndLogErrorHRESULT("Get the default audio endpoint", hr)) return false;
    // 激活音量控制接口
    bool success = true;
    hr = device->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, NULL, (void**)&endpointVolume); // 激活IAudioEndpointVolume
    if (!checkAndLogErrorHRESULT("Activate IAudioEndpointVolume interface", hr)) success = false;
    hr = device->Activate(__uuidof(IAudioSessionManager2), CLSCTX_ALL, NULL, (void**)&sessionManager); // 激活IAudioSessionManager2
    if (!checkAndLogErrorHRESULT("Activate IAudioSessionManager2 interface", hr)) return false;
    hr = sessionManager->GetAudioSessionControl(&GUID_NULL, 0, &session.control);
    if (!checkAndLogErrorHRESULT("SessionManager->GetAudioSessionControl", hr)) return false;
    //IAudioSessionControl2* pSessionControl2 = nullptr;
    //hr = session.control->QueryInterface(__uuidof(IAudioSessionControl2), (void**)&session.control2);
    //if (!checkAndLogErrorHRESULT("SessionControl->QueryInterface(IAudioSessionControl2)", hr)) return false;
    hr = session.control->QueryInterface(__uuidof(ISimpleAudioVolume), (void**)&session.volumeControl);
    if (!checkAndLogErrorHRESULT("SessionControl->QueryInterface(ISimpleAudioVolume)", hr)) return false;
    if (!success) return false;
    return true;
}


void Win32SystemVolumeController::cleanUp()
{
    if (session.volumeControl)
    {
        session.volumeControl->Release();
        session.volumeControl = nullptr;
    }
    // 注销应用音量通知
    if (session.control)
    {
        if (session.events)
        {
            session.control->UnregisterAudioSessionNotification(session.events);
            session.events->Release();
            session.events = nullptr;
        }
        session.control->Release();
        session.control = nullptr;
    }
    if (sessionManager)
    {
        sessionManager->Release();
        sessionManager = nullptr;
    }
    if (endpointVolume)
    {
        if (cbInst) endpointVolume->UnregisterControlChangeNotify(cbInst);
        endpointVolume->Release();
        endpointVolume = nullptr;
    }
    if (cbInst)
    {
        cbInst->Release();
        cbInst = nullptr;
    }
    if (device)
    {
        device->Release();
        device = nullptr;
    }
    if (devEnumerator)
    {
        devEnumerator->Release();
        devEnumerator = nullptr;
    }
}

float Win32SystemVolumeController::getSystemMasterVolume() const
{
    float level = 0.0f;
    if (endpointVolume)
        endpointVolume->GetMasterVolumeLevelScalar(&level);
    return std::clamp(level, 0.0f, 1.0f);
}

void Win32SystemVolumeController::setSystemMasterVolume(float volume)
{
    if (endpointVolume)
    {
        if (volume < 0.0f) volume = 0.0f;
        if (volume > 1.0f) volume = 1.0f;
        HRESULT hr = endpointVolume->SetMasterVolumeLevelScalar(volume, NULL);
        checkAndLogErrorHRESULT("IAudioEndpointVolume->SetMasterVolumeLevelScalar(" + std::to_string(volume) + ")", hr);
    }
}

float Win32SystemVolumeController::getSystemMixerVolume() const
{
    if (session.volumeControl)
    {
        float v = 0.0f;
        HRESULT hr = session.volumeControl->GetMasterVolume(&v);
        if (checkAndLogErrorHRESULT("ISimpleAudioVolume->GetMasterVolume", hr))
            return v;
    }
    return 0;
}

void Win32SystemVolumeController::setSystemMixerVolume(float volume)
{
    if (session.volumeControl)
    {
        HRESULT hr = session.volumeControl->SetMasterVolume(volume, &GUID_NULL);
        checkAndLogErrorHRESULT("ISimpleAudioVolume->SetMasterVolume(" + std::to_string(volume) + ")", hr);
    }
}

bool Win32SystemVolumeController::setVolumeChangeCallback(VolumeChangeCallback callback)
{
    this->volumeChangeCallback = callback;
    if (!endpointVolume || !sessionManager)
        if (!init())
            return false;
    bool success = true;
    if (!cbInst)
    {
        cbInst = new AudioEndpointVolumeCallback(*this);
        if (endpointVolume)
        {
            HRESULT hr = endpointVolume->RegisterControlChangeNotify(cbInst);
            if (!checkAndLogErrorHRESULT("IAudioEndpointVolume->RegisterControlChangeNotify", hr))
            {
                cbInst->Release();
                cbInst = nullptr;
                success = false;
            }
        }
        else
        {
            cbInst->Release();
            cbInst = nullptr;
            success = false;
        }
    }
    if (!registerSession())
        success = false;
    return success;
}

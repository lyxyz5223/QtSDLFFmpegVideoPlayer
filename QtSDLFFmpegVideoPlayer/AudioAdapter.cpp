#include "AudioAdapter.h"
#include <unordered_map>
#define AUDIO_ADAPTER_LISTITEM(type, creator) { type, creator },
#ifdef HAVE_RTAUDIO
#include "RtAudioAdapter.h"

#endif
#ifdef HAVE_PORTAUDIO
#include "PortAudioAdapter.h"
#endif

std::unordered_map<AudioAdapterFactory::AudioAdapterAdapter, AudioAdapter::CreateFunction> AudioAdapterFactory::compiledAudioAdapterAdapters = {
#ifdef HAVE_RTAUDIO
    AUDIO_ADAPTER_LISTITEM(AudioAdapterFactory::RtAudioAdapter, RtAudioAdapter::create)
#endif
#ifdef HAVE_PORTAUDIO
    AUDIO_ADAPTER_LISTITEM(AudioAdapterFactory::PortAudioAdapter, PortAudioAdapter::create)
#endif
};


AudioAdapter* AudioAdapterFactory::create(AudioAdapterAdapter adapter, AudioAdapter::AudioApi api, AudioAdapter::AudioErrorCallback errorCallback)
{
    if (compiledAudioAdapterAdapters.count(adapter) == 0)
        return nullptr;
    // 创建新的实例
    auto audioDevice = compiledAudioAdapterAdapters.at(adapter)(api, errorCallback); // RtAudio(RtAudio::Api api = UNSPECIFIED, RtAudioErrorCallback && errorCallback = 0);
    if (!audioDevice)
        return nullptr;
    // 设置错误回调函数
    try {
        if (errorCallback)
            audioDevice->setErrorCallback(errorCallback);
    } catch (const std::exception& e) {
        //SDL_Log("设置音频错误回调失败: %s", e.what());
    } catch (...) {
        //SDL_Log("设置音频错误回调失败: 未知错误");
    }
    return audioDevice;
}



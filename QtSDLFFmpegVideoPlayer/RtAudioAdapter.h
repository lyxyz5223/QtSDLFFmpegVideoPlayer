#pragma once
#include "AudioAdapter.h"
#include <RtAudio.h>

class RtAudioAdapter : public AudioAdapter
{
private:
    template <typename T>
    using UniquePtr = std::unique_ptr<T, std::function<void(T*)>>;
    template <typename T>
    using UniquePtrD = std::unique_ptr<T>;

    UniquePtrD<RtAudio> audioDevice;

public:
    struct RtAudioCallbackArgs {
        void* outputBuffer;
        void* inputBuffer;
        unsigned int nFrames;
        double streamTime;
        RtAudioStreamStatus status;
        void* userData;
    };

    // bThrowIfError = true 时，如果没有匹配的转换会抛出std::runtime_error错误
    constexpr static RtAudioFormat avSampleFormatToRtAudioFormat(AVSampleFormat sampleFmt, RtAudioFormat defaultValue = RTAUDIO_SINT16, bool bThrowIfError = false) noexcept(false)
    {
        RtAudioFormat rtAudioFmt = defaultValue;
        auto throwErr = [&](const std::string& fmtStr) {
            if (bThrowIfError)
                throw std::runtime_error("Unsupported audio sample format: " + fmtStr);
            };
        switch (sampleFmt)
        {
        case AV_SAMPLE_FMT_NONE:
            throwErr("AV_SAMPLE_FMT_NONE");
            break;
        case AV_SAMPLE_FMT_U8:
            rtAudioFmt = RTAUDIO_SINT8;
            break;
        case AV_SAMPLE_FMT_S16:
            rtAudioFmt = RTAUDIO_SINT16;
            break;
        case AV_SAMPLE_FMT_S32:
            rtAudioFmt = RTAUDIO_SINT32;
            break;
        case AV_SAMPLE_FMT_FLT:
            rtAudioFmt = RTAUDIO_FLOAT32;
            break;
        case AV_SAMPLE_FMT_DBL:
            rtAudioFmt = RTAUDIO_FLOAT64;
            break;
        case AV_SAMPLE_FMT_U8P: // planar formats 即分离格式，处理的时候AVFrame::data数组中每个通道的数据是分开存储的
            rtAudioFmt = RTAUDIO_SINT8;
            //throwErr("AV_SAMPLE_FMT_U8P");
            break;
        case AV_SAMPLE_FMT_S16P:
            rtAudioFmt = RTAUDIO_SINT16;
            //throwErr("AV_SAMPLE_FMT_S16P");
            break;
        case AV_SAMPLE_FMT_S32P:
            rtAudioFmt = RTAUDIO_SINT32;
            //throwErr("AV_SAMPLE_FMT_S32P");
            break;
        case AV_SAMPLE_FMT_FLTP:
            rtAudioFmt = RTAUDIO_FLOAT32;
            //throwErr("AV_SAMPLE_FMT_FLTP");
            break;
        case AV_SAMPLE_FMT_DBLP:
            rtAudioFmt = RTAUDIO_FLOAT64;
            //throwErr("AV_SAMPLE_FMT_DBLP");
            break;
        case AV_SAMPLE_FMT_S64:
            throwErr("AV_SAMPLE_FMT_S64");
            break;
        case AV_SAMPLE_FMT_S64P:
            throwErr("AV_SAMPLE_FMT_S64P");
            break;
        case AV_SAMPLE_FMT_NB:
            throwErr("AV_SAMPLE_FMT_NB");
            break;
        default:
            break;
        }
        return rtAudioFmt;
    }
    constexpr static RtAudioFormat audioFormatToRtAudioFormat(AudioFormat sampleFmt, RtAudioFormat defaultValue = RTAUDIO_SINT16, bool bThrowIfError = false) noexcept(false)
    {
        RtAudioFormat rtAudioFmt = defaultValue;
        auto throwErr = [&](const std::string& fmtStr) {
            if (bThrowIfError)
                throw std::runtime_error("Unsupported audio sample format: " + fmtStr);
            };
        switch (sampleFmt)
        {
        case AudioAdapter::AFSignedInt8:
            rtAudioFmt = RTAUDIO_SINT8;
            break;
        case AudioAdapter::AFSignedInt16:
            rtAudioFmt = RTAUDIO_SINT16;
            break;
        case AudioAdapter::AFSignedInt24:
            rtAudioFmt = RTAUDIO_SINT24;
            break;
        case AudioAdapter::AFSignedInt32:
            rtAudioFmt = RTAUDIO_SINT32;
            break;
        case AudioAdapter::AFFloat32:
            rtAudioFmt = RTAUDIO_FLOAT32;
            break;
        case AudioAdapter::AFFloat64:
            rtAudioFmt = RTAUDIO_FLOAT64;
            break;
        default:
            break;
        }
        return rtAudioFmt;
    }
    constexpr static AudioFormat rtAudioFormatToAudioFormat(RtAudioFormat sampleFmt, AudioFormat defaultValue = AudioFormat::AFSignedInt16, bool bThrowIfError = false) noexcept(false)
    {
        AudioFormat audioFmt = defaultValue;
        auto throwErr = [&](const std::string& fmtStr) {
            if (bThrowIfError)
                throw std::runtime_error("Unsupported audio sample format: " + fmtStr);
            };
        switch (sampleFmt)
        {
        case RTAUDIO_SINT8:
            audioFmt = AudioAdapter::AFSignedInt8;
            break;
        case RTAUDIO_SINT16:
            audioFmt = AudioAdapter::AFSignedInt16;
            break;
        case RTAUDIO_SINT24:
            audioFmt = AudioAdapter::AFSignedInt24;
            break;
        case RTAUDIO_SINT32:
            audioFmt = AudioAdapter::AFSignedInt32;
            break;
        case RTAUDIO_FLOAT32:
            audioFmt = AudioAdapter::AFFloat32;
            break;
        case RTAUDIO_FLOAT64:
            audioFmt = AudioAdapter::AFFloat64;
            break;
        default:
            break;
        }
        return audioFmt;
    }
    constexpr static RtAudioStreamFlags audioStreamFlagsToRtAudioStreamFlags(AudioStreamFlags f) {
        RtAudioStreamFlags rst = static_cast<RtAudioStreamFlags>(0);
        if (f & AudioAdapter::NonInterleaved)
            rst |= RTAUDIO_NONINTERLEAVED;
        if (f & AudioAdapter::MinimizeLatency)
            rst |= RTAUDIO_MINIMIZE_LATENCY;
        if (f & AudioAdapter::HogDevice)
            rst |= RTAUDIO_HOG_DEVICE;
        if (f & AudioAdapter::ScheduleRealtime)
            rst |= RTAUDIO_SCHEDULE_REALTIME;
        if (f & AudioAdapter::AlsaUseDefault)
            rst |= RTAUDIO_ALSA_USE_DEFAULT;
        if (f & AudioAdapter::JackDontConnect)
            rst |= RTAUDIO_JACK_DONT_CONNECT;
        return rst;
    }
    constexpr static AudioStreamStatuses rtAudioStreamStatusToAudioStreamStatuses(RtAudioStreamStatus s) {
        AudioStreamStatuses rst = static_cast<AudioStreamStatus>(0);
        if (s & RTAUDIO_INPUT_OVERFLOW)
            rst |= AudioAdapter::InputOverflow;
        if (s & RTAUDIO_OUTPUT_UNDERFLOW)
            rst |= AudioAdapter::OutputUnderflow;
        return rst;
    }
    constexpr static int audioCallbackResultToRtAudioCallbackResult(AudioCallbackResult r) {
        /*
           To continue normal stream operation, the RtAudioCallback function
           should return a value of zero.  To stop the stream and drain the
           output buffer, the function should return a value of one.  To abort
           the stream immediately, the client should return a value of two.
        */
        if (r == AudioAdapter::Continue)
            return 0;
        else if (r == AudioAdapter::Complete)
            return 1;
        else // if (r == AudioAdapter::Abort)
            return 2;
    }
    constexpr static RtAudio::Api audioApiToRtAudioApi(AudioApi api) {
        RtAudio::Api a = RtAudio::UNSPECIFIED;
        switch (api)
        {
        case AudioAdapter::Unspecified:
            a = RtAudio::UNSPECIFIED;
            break;
        case AudioAdapter::WindowsWasapi:
            a = RtAudio::WINDOWS_WASAPI;
            break;
        case AudioAdapter::WindowsDirectSound:
            a = RtAudio::WINDOWS_DS;
            break;
        case AudioAdapter::WindowsAsio:
            a = RtAudio::WINDOWS_ASIO;
            break;
        case AudioAdapter::MacOsxCore:
            a = RtAudio::MACOSX_CORE;
            break;
        case AudioAdapter::LinuxAlsa:
            a = RtAudio::LINUX_ALSA;
            break;
        case AudioAdapter::LinuxOss:
            a = RtAudio::LINUX_OSS;
            break;
        case AudioAdapter::UnixJack:
            a = RtAudio::UNIX_JACK;
            break;
        case AudioAdapter::Dummy:
            a = RtAudio::RTAUDIO_DUMMY;
            break;
        default:
            break;
        }
        return a;
    }
    constexpr static AudioErrorType rtAudioErrorTypeToAudioErrorType(RtAudioErrorType t) {
        AudioErrorType r = AudioErrorType::NoError;
        switch (t)
        {
        case RTAUDIO_NO_ERROR:
            r = AudioErrorType::NoError;
            break;
        case RTAUDIO_WARNING:
            r = AudioErrorType::Warning;
            break;
        case RTAUDIO_UNKNOWN_ERROR:
            r = AudioErrorType::Unknown;
            break;
        case RTAUDIO_NO_DEVICES_FOUND:
            r = AudioErrorType::NoDevicesFound;
            break;
        case RTAUDIO_INVALID_DEVICE:
            r = AudioErrorType::InvalidDevice;
            break;
        case RTAUDIO_DEVICE_DISCONNECT:
            r = AudioErrorType::DeviceDisconnect;
            break;
        case RTAUDIO_MEMORY_ERROR:
            r = AudioErrorType::MemoryError;
            break;
        case RTAUDIO_INVALID_PARAMETER:
            r = AudioErrorType::InvalidParameter;
            break;
        case RTAUDIO_INVALID_USE:
            r = AudioErrorType::InvalidUse;
            break;
        case RTAUDIO_DRIVER_ERROR:
            r = AudioErrorType::DriverError;
            break;
        case RTAUDIO_SYSTEM_ERROR:
            r = AudioErrorType::SystemError;
            break;
        case RTAUDIO_THREAD_ERROR:
            r = AudioErrorType::ThreadError;
            break;
        default:
            break;
        }
        return r;
    }

public:
    RtAudioAdapter(AudioApi api = AudioApi::Unspecified, AudioErrorCallback errorCallback = 0) {
        audioDevice = std::make_unique<RtAudio>(audioApiToRtAudioApi(api), [errorCallback](RtAudioErrorType t, const std::string& m) {
            errorCallback(rtAudioErrorTypeToAudioErrorType(t), m);
            });
    }

    static RtAudioAdapter* create(AudioApi api, AudioErrorCallback errorCallback)
    {
        return new RtAudioAdapter(api, errorCallback);
    }


    // 重写 AudioAdapter 的纯虚函数
    virtual unsigned int getDeviceCount() override {
        if (!audioDevice)
            return 0;
        return audioDevice->getDeviceCount();
    }
    virtual AudioDeviceInfo getDeviceInfo(unsigned int deviceId) override {
        AudioDeviceInfo info;
        if (!audioDevice)
            return info;
        RtAudio::DeviceInfo rtInfo = audioDevice->getDeviceInfo(deviceId);
        info.ID = rtInfo.ID;
        info.name = rtInfo.name;
        info.outputChannels = rtInfo.outputChannels;
        info.inputChannels = rtInfo.inputChannels;
        info.duplexChannels = rtInfo.duplexChannels;
        info.isDefaultOutput = rtInfo.isDefaultOutput;
        info.isDefaultInput = rtInfo.isDefaultInput;
        info.sampleRates = rtInfo.sampleRates;
        info.currentSampleRate = rtInfo.currentSampleRate;
        info.preferredSampleRate = rtInfo.preferredSampleRate;
        info.nativeFormats = rtAudioFormatToAudioFormat(rtInfo.nativeFormats);
        return info;
    }

    virtual unsigned int getDefaultOutputDevice() override {
        if (!audioDevice)
            return 0;
        return audioDevice->getDefaultOutputDevice();
    }

    virtual AudioErrorType openStream(AudioStreamParameters* outputParameters,
        AudioStreamParameters* inputParameters,
        AudioFormats format, unsigned int sampleRate,
        unsigned int* bufferFrames, AudioCallbackFunction callback,
        std::any userData = 0, AudioStreamOptions* options = 0) override {
        if (!audioDevice)
            return AudioErrorType::MemoryError;
        RtAudio::StreamParameters rtOutputParams;
        if (outputParameters)
        {
            rtOutputParams.deviceId = outputParameters->deviceId;
            rtOutputParams.nChannels = outputParameters->nChannels;
            rtOutputParams.firstChannel = outputParameters->firstChannel;
        }
        RtAudio::StreamParameters rtInputParams;
        if (inputParameters)
        {
            rtInputParams.deviceId = inputParameters->deviceId;
            rtInputParams.nChannels = inputParameters->nChannels;
            rtInputParams.firstChannel = inputParameters->firstChannel;
        }
        struct CallbackUserData {
            RtAudioAdapter* self;
            AudioCallbackFunction callback;
            std::any userData;
        } *pUserData = new CallbackUserData{
            this, callback, userData
        };
        RtAudio::StreamOptions rtOptions;
        if (options)
        {
            rtOptions.flags = audioStreamFlagsToRtAudioStreamFlags(options->flags);
            rtOptions.numberOfBuffers = options->numberOfBuffers;
            rtOptions.streamName = options->streamName;
            rtOptions.priority = options->priority;
        }
        return rtAudioErrorTypeToAudioErrorType(audioDevice->openStream(outputParameters ? &rtOutputParams : 0,
            inputParameters ? &rtInputParams : 0,
            audioFormatToRtAudioFormat(format), sampleRate, bufferFrames,
            [] (void* outputBuffer, void* inputBuffer, unsigned int nFrames, double streamTime, RtAudioStreamStatus status, void* userData) -> int {
                CallbackUserData* p = static_cast<CallbackUserData*>(userData);
                AudioCallbackResult rst = Continue;
                RtAudioCallbackArgs args{
                    outputBuffer,
                    inputBuffer,
                    nFrames,
                    streamTime,
                    status,
                    userData
                };
                rst = p->callback(outputBuffer, inputBuffer, nFrames, streamTime, rtAudioStreamStatusToAudioStreamStatuses(status), args, userData);
                if (rst != Continue)
                    delete userData;
                return audioCallbackResultToRtAudioCallbackResult(rst);
            }
            , pUserData, options ? &rtOptions : 0));
    }


    virtual void closeStream() override {
        if (!audioDevice)
            return;
        return audioDevice->closeStream();
    }


    virtual AudioErrorType startStream() override {
        if (!audioDevice)
            return AudioErrorType::MemoryError;
        return rtAudioErrorTypeToAudioErrorType(audioDevice->startStream());
    }


    virtual AudioErrorType stopStream() override {
        if (!audioDevice)
            return AudioErrorType::MemoryError;
        return rtAudioErrorTypeToAudioErrorType(audioDevice->stopStream());
    }


    virtual AudioErrorType abortStream() override {
        if (!audioDevice)
            return AudioErrorType::MemoryError;
        return rtAudioErrorTypeToAudioErrorType(audioDevice->abortStream());
    }


    virtual const std::string getLastErrorText() override {
        if (!audioDevice)
            return "MemoryError";
        return audioDevice->getErrorText();
    }


    virtual bool isStreamOpen() const override {
        if (!audioDevice)
            return false;
        return audioDevice->isStreamOpen();
    }


    virtual bool isStreamRunning() const override {
        if (!audioDevice)
            return false;
        return audioDevice->isStreamRunning();
    }


    virtual double getStreamTime() override {
        if (!audioDevice)
            return 0.0;
        return audioDevice->getStreamTime();
    }


    virtual void setStreamTime(double time) override {
        if (!audioDevice)
            return;
        return audioDevice->setStreamTime(time);
    }


    virtual long getOutputStreamLatency() override {
        if (!audioDevice)
            return 0;
        return audioDevice->getStreamLatency();
    }

    virtual long getInputStreamLatency() override {
        if (!audioDevice)
            return 0;
        return audioDevice->getStreamLatency();
    }


    virtual unsigned int getStreamSampleRate() override {
        if (!audioDevice)
            return 0;
        return audioDevice->getStreamSampleRate();
    }


    virtual void setErrorCallback(AudioErrorCallback errorCallback) override {
        if (!audioDevice)
            return;
        return audioDevice->setErrorCallback([errorCallback](RtAudioErrorType t, const std::string& m) {
            errorCallback(rtAudioErrorTypeToAudioErrorType(t), m);
            });
    }


    virtual void setShouldShowWarnings(bool value = true) override {
        if (!audioDevice)
            return;
        return audioDevice->showWarnings(value);
    }


};
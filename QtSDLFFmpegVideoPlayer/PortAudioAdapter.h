#pragma once
#include "AudioAdapter.h"

#include <portaudio.h>
#include <string>

class PortAudioAdapter : public AudioAdapter
{
private:
    std::unique_ptr<PaStream, decltype(&Pa_CloseStream)> audioStream{ nullptr, &Pa_CloseStream };
public:
    struct PaStreamCallbackArgs{
        const void* input;
        void* output;
        unsigned long frameCount;
        const PaStreamCallbackTimeInfo* timeInfo;
        PaStreamCallbackFlags statusFlags;
        void* userData;
    };

    static constexpr AudioStreamStatuses paStreamCallbackFlagsToAudioStreamStatuses(PaStreamCallbackFlags flags) {
        AudioStreamStatuses statuses{ AudioStreamStatus{ 0 } };
        if (flags & paInputUnderflow)
            statuses |= AudioStreamStatus::InputUnderflow;
        if (flags & paInputOverflow)
            statuses |= AudioStreamStatus::InputOverflow;
        if (flags & paOutputUnderflow)
            statuses |= AudioStreamStatus::OutputUnderflow;
        if (flags & paOutputOverflow)
            statuses |= AudioStreamStatus::OutputOverflow;
        if (flags & paPrimingOutput)
            statuses |= AudioStreamStatus::PrimingOutput;
        return statuses;
    }

    static constexpr AudioErrorType paErrorToAudioErrorType(PaError err) {
        switch (err)
        {
        case paNoError:
            return AudioErrorType::NoError;
        case paUnanticipatedHostError:
            return AudioErrorType::DriverError;
        case paInsufficientMemory:
            return AudioErrorType::MemoryError;
        case paInvalidDevice:
            return AudioErrorType::InvalidDevice;
        case paDeviceUnavailable:
            return AudioErrorType::DeviceDisconnect;
        case paInvalidFlag:
        case paInvalidSampleRate:
        case paInvalidChannelCount:
        case paBadIODeviceCombination:
            return AudioErrorType::InvalidParameter;
        case paStreamIsStopped:
        case paStreamIsNotStopped:
        case paInputOverflowed:
        case paOutputUnderflowed:
            return AudioErrorType::InvalidUse;
        case paInternalError:
            return AudioErrorType::SystemError;
        default:
            return AudioErrorType::Unknown;
        }
    }

    static constexpr int audioCallbackResultToPaStreamCallbackResult(AudioCallbackResult result) {
        switch (result)
        {
        case AudioCallbackResult::Continue:
            return paContinue;
        case AudioCallbackResult::Complete:
            return paComplete;
        case AudioCallbackResult::Abort:
            return paAbort;
        default:
            return paContinue;
        }
    }

    static constexpr PaStreamFlags audioStreamFlagsToPaStreamFlags(AudioStreamFlags flags) {
        PaStreamFlags paFlags = paNoFlag;
        if (flags.testFlag(AudioStreamFlag::ClipOff))
            paFlags |= paClipOff;
        if (flags.testFlag(AudioStreamFlag::DitherOff))
            paFlags |= paDitherOff;
        if (flags.testFlag(AudioStreamFlag::NeverDropInput))
            paFlags |= paNeverDropInput;
        if (flags.testFlag(AudioStreamFlag::PrimeOutputBuffersUsingStreamCallback))
            paFlags |= paPrimeOutputBuffersUsingStreamCallback;
        if (flags.testFlag(AudioStreamFlag::PlatformSpecificFlags))
            paFlags |= paPlatformSpecificFlags;
        return paFlags;
    }
private:
    static size_t s_ref;
public:
    PortAudioAdapter(AudioApi api, AudioErrorCallback errorCallback) {
        if (s_ref == 0)
        {
            auto err = Pa_Initialize();
            if (err != paNoError)
            {
                delete this;
                throw std::runtime_error("Pa_Initialize error: code: " + std::to_string(err) + ", message: " + Pa_GetErrorText(err));
                return;
            }
        }
        ++s_ref;
    }
    ~PortAudioAdapter() {
        --s_ref;
        if (s_ref == 0)
        {
            auto err = Pa_Terminate();
            if (err != paNoError)
                throw std::runtime_error("Pa_Terminate error: code: " + std::to_string(err) + ", message: " + Pa_GetErrorText(err));
        }
    }
    static AudioAdapter* create(AudioApi api, AudioErrorCallback errorCallback) {
        return new PortAudioAdapter(api, errorCallback);
    }

    constexpr static PaSampleFormat audioFormatToPaSampleFormat(AudioFormats format) {
        PaSampleFormat paFormat = 0;
        if (format & AudioFormat::AFSignedInt8)
            paFormat |= paInt8;
        if (format & AudioFormat::AFSignedInt16)
            paFormat |= paInt16;
        if (format & AudioFormat::AFSignedInt24)
            paFormat |= paInt24;
        if (format & AudioFormat::AFSignedInt32)
            paFormat |= paInt32;
        if (format & AudioFormat::AFFloat32)
            paFormat |= paFloat32;
        if (format & AudioFormat::AFFloat64)
            throw std::runtime_error("PortAudio does not support AFFloat64 format.");
        return paFormat;
    }
public:
    // 重写 AudioAdapter 的纯虚函数
    virtual unsigned int getDeviceCount() override {
        return Pa_GetDeviceCount();
    }
    virtual AudioDeviceInfo getDeviceInfo(unsigned int deviceId) override {
        int dId = static_cast<int>(deviceId);
        AudioDeviceInfo info;
        if (!audioStream)
            return info;
        const PaDeviceInfo* paInfo = Pa_GetDeviceInfo(dId);
        
        info.ID = deviceId;
        info.name = paInfo->name;
        info.outputChannels = paInfo->maxOutputChannels;
        info.inputChannels = paInfo->maxInputChannels;
        // 双工通道数
        info.duplexChannels = std::min(paInfo->maxOutputChannels, paInfo->maxInputChannels); // 支持的最大输入输出通道数取最小值
        info.isDefaultOutput = Pa_GetDefaultOutputDevice() == dId;
        info.isDefaultInput = Pa_GetDefaultInputDevice() == dId;
        info.sampleRates = { (unsigned int)paInfo->defaultSampleRate };
        info.currentSampleRate = paInfo->defaultSampleRate;
        info.preferredSampleRate = paInfo->defaultSampleRate;
        info.nativeFormats = AudioFormat{ 0 };
        for (size_t i = 0; i < AudioFormatCount; ++i)
        {
            PaStreamParameters p{};
            p.device = dId;
            p.channelCount = paInfo->maxOutputChannels;
            p.sampleFormat = AudioFormatList[i];
            p.suggestedLatency = paInfo->defaultLowOutputLatency;
            PaError err = Pa_IsFormatSupported(0, &p, paInfo->defaultSampleRate);
            if (err == paFormatIsSupported)
                info.nativeFormats |= AudioFormatList[i];
        }
        return info;
    }

    virtual unsigned int getDefaultOutputDevice() override {
        return Pa_GetDefaultOutputDevice();
    }

    virtual AudioErrorType openStream(AudioStreamParameters* outputParameters,
        AudioStreamParameters* inputParameters,
        AudioFormats format, unsigned int sampleRate,
        unsigned int* bufferFrames, AudioCallbackFunction callback,
        std::any userData = 0, AudioStreamOptions* options = 0) override {
        PaStreamParameters outputParams{};
        auto outputDeviceInfo = (outputParameters ? Pa_GetDeviceInfo(outputParameters->deviceId) : 0);
        auto inputDeviceInfo = (inputParameters ? Pa_GetDeviceInfo(inputParameters->deviceId) : 0);
        if (outputParameters)
        {
            outputParams.device = outputParameters->deviceId;
            outputParams.channelCount = outputParameters->nChannels;
            outputParams.sampleFormat = audioFormatToPaSampleFormat(format);
            outputParams.suggestedLatency = (outputDeviceInfo ? outputDeviceInfo->defaultLowOutputLatency : 0);
        }
        PaStreamParameters inputParams{};
        if (inputParameters)
        {
            inputParams.device = inputParameters->deviceId;
            inputParams.channelCount = inputParameters->nChannels;
            inputParams.sampleFormat = audioFormatToPaSampleFormat(format);
            inputParams.suggestedLatency = (inputDeviceInfo ? inputDeviceInfo->defaultLowOutputLatency : 0);
        }
        struct CallbackUserData {
            PortAudioAdapter* self;
            AudioCallbackFunction callback;
            std::any userData;
        } *pUserData = new CallbackUserData{
            this, callback, userData
        };
        PaStreamFlags flags = audioStreamFlagsToPaStreamFlags(options->flags);
        PaStream* stream = nullptr;
        auto rst = paErrorToAudioErrorType(Pa_OpenStream(&stream,
            inputParameters ? &inputParams : nullptr,
            outputParameters ? &outputParams : nullptr,
            sampleRate, *bufferFrames, flags,
            [](const void* input, void* output, unsigned long frameCount, const PaStreamCallbackTimeInfo* timeInfo, PaStreamCallbackFlags statusFlags, void* userData) ->int {
                CallbackUserData* p = static_cast<CallbackUserData*>(userData);
                PaStreamCallbackArgs args{
                    input,
                    output,
                    frameCount,
                    timeInfo,
                    statusFlags,
                    userData
                };
                auto rst = p->callback(output, const_cast<void*>(input), frameCount, timeInfo->currentTime, paStreamCallbackFlagsToAudioStreamStatuses(statusFlags), args, userData);
                if (rst != Continue)
                    delete userData;
                return audioCallbackResultToPaStreamCallbackResult(rst);
            }
            , pUserData));
        if (rst == AudioErrorType::NoError)
            audioStream.reset(stream);
        else
            audioStream.reset(nullptr);
        return rst;
    }


    virtual void closeStream() override {
        if (!audioStream)
            return;
        Pa_CloseStream(audioStream.get());
        audioStream.reset(nullptr);
    }


    virtual AudioErrorType startStream() override {
        if (!audioStream)
            return AudioErrorType::MemoryError;
        return paErrorToAudioErrorType(Pa_StartStream(audioStream.get()));
    }


    virtual AudioErrorType stopStream() override {
        if (!audioStream)
            return AudioErrorType::MemoryError;
        return paErrorToAudioErrorType(Pa_StopStream(audioStream.get()));
    }


    virtual AudioErrorType abortStream() override {
        if (!audioStream)
            return AudioErrorType::MemoryError;
        return paErrorToAudioErrorType(Pa_AbortStream(audioStream.get()));
    }


    virtual const std::string getLastErrorText() override {
        auto err = Pa_GetLastHostErrorInfo();
        return err->errorText;
    }


    virtual bool isStreamOpen() const override {
        if (!audioStream)
            return false;
        return true;
    }


    virtual bool isStreamRunning() const override {
        if (!audioStream)
            return false;
        return Pa_IsStreamActive(audioStream.get());
    }


    virtual double getStreamTime() override {
        if (!audioStream)
            return 0.0;
        return Pa_GetStreamTime(audioStream.get());
    }


    virtual void setStreamTime(double time) override {
        if (!audioStream)
            return;
        throw std::runtime_error("PortAudioAdapter::setStreamTime not implemented.");
    }


    virtual long getOutputStreamLatency() override {
        if (!audioStream)
            return 0;
        return Pa_GetStreamInfo(audioStream.get())->outputLatency;
    }
    virtual long getInputStreamLatency() override {
        if (!audioStream)
            return 0;
        return Pa_GetStreamInfo(audioStream.get())->inputLatency;
    }


    virtual unsigned int getStreamSampleRate() override {
        if (!audioStream)
            return 0;
        return Pa_GetStreamInfo(audioStream.get())->sampleRate;
    }


    virtual void setErrorCallback(AudioErrorCallback errorCallback) override {
        //if (!audioStream)
        //    return;
        //([errorCallback](RtAudioErrorType t, const std::string& m) {
        //    errorCallback(paErrorToAudioErrorType(t), m);
        //    });
        throw std::runtime_error("PortAudioAdapter::setErrorCallback not implemented.");
    }


    virtual void setShouldShowWarnings(bool value = true) override {
        //if (!audioStream)
        //    return;
        throw std::runtime_error("PortAudioAdapter::setShouldShowWarnings not implemented.");
    }

};

inline size_t PortAudioAdapter::s_ref = 0;

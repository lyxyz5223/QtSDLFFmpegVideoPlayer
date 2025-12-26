#pragma once
#include <functional>
#include <atomic>
#include <memory>
#include <string>
#include <vector>
#include <any>
#include <unordered_map>
#include <exception>
#include <stdexcept>

#include "MultiEnumTypeDefine.h"

extern "C"
{
#include <libavutil/samplefmt.h>
}

class AudioAdapterFactory;

class AudioAdapter
{
private:
    friend class AudioAdapterFactory;

public:
    enum AudioErrorType {
        NoError = 0,      /*!< No error. */
        Warning,           /*!< A non-critical error. */
        Unknown,     /*!< An unspecified error type. */
        NoDevicesFound,  /*!< No devices found on system. */
        InvalidDevice,    /*!< An invalid device ID was specified. */
        DeviceDisconnect, /*!< A device in use was disconnected. */
        MemoryError,      /*!< An error occurred during memory allocation. */
        InvalidParameter, /*!< An invalid parameter was specified to a function. */
        InvalidUse,       /*!< The function was called incorrectly. */
        DriverError,      /*!< A system driver error occurred. */
        SystemError,      /*!< A system error occurred. */
        ThreadError       /*!< A thread error occurred. */
    };
    using AudioErrorCallback = std::function<void(AudioErrorType type, const std::string& errorText)>;

    struct AudioStreamParameters {
        //std::string deviceName{};     /*!< Device name from device list. */
        unsigned int deviceId = 0;     /*!< Device id as provided by getDeviceIds(). */
        unsigned int nChannels = 0;    /*!< Number of channels. */
        unsigned int firstChannel = 0; /*!< First channel index on device (default = 0). */
    };

    enum AudioStreamFlag : unsigned int {
        None = 0x0,
        // RtAudio specific flags
        NonInterleaved = 0x1,
        MinimizeLatency = 0x2,
        HogDevice = 0x4,
        ScheduleRealtime = 0x8,
        AlsaUseDefault = 0x10,
        JackDontConnect = 0x20,

        // PortAudio compatible flags
        ClipOff = 0x1,
        DitherOff = 0x2,
        NeverDropInput = 0x4,
        PrimeOutputBuffersUsingStreamCallback = 0x8,
        PlatformSpecificFlags = 0xFFFF0000,

    };
    using AudioStreamFlags = MultiEnumTypeDefine<AudioStreamFlag>;

    struct AudioStreamOptions {
        AudioStreamFlags flags{};      /*!< A bit-mask of stream flags (RTAUDIO_NONINTERLEAVED, RTAUDIO_MINIMIZE_LATENCY, RTAUDIO_HOG_DEVICE, RTAUDIO_ALSA_USE_DEFAULT). */
        unsigned int numberOfBuffers{};  /*!< Number of stream buffers. */
        std::string streamName;        /*!< A stream name (currently used only in Jack). */
        int priority{};                  /*!< Scheduling priority of callback thread (only used with flag RTAUDIO_SCHEDULE_REALTIME). */
    };

    enum AudioFormat : unsigned long {
        AFSignedInt8 = 0x1,
        AFSignedInt16 = 0x2,
        AFSignedInt24 = 0x4,
        AFSignedInt32 = 0x8,
        AFFloat32 = 0x10,
        AFFloat64 = 0x20
    };
    constexpr static AudioFormat AudioFormatList[] = {
        AFSignedInt8,
        AFSignedInt16,
        AFSignedInt24,
        AFSignedInt32,
        AFFloat32,
        AFFloat64
    };
    constexpr static size_t AudioFormatCount = sizeof(AudioFormatList) / sizeof(AudioFormatList[0]);

    using AudioFormats = MultiEnumTypeDefine<AudioFormat>;

    enum AudioCallbackResult {
        Continue = 0,
        Complete = 1,
        Abort = 2
    };

    enum AudioStreamStatus : unsigned long {
        InputUnderflow = 0x1,
        InputOverflow = 0x2,
        OutputUnderflow = 0x4,
        OutputOverflow = 0x8,
        PrimingOutput = 0x10
    };
    using AudioStreamStatuses = MultiEnumTypeDefine<AudioStreamStatus>;

    using UserDataType = std::any;
    using RawArgsType = std::any;
    // rawArgs: The raw arguments type depends on the underlying audio API implementation.
    // this is a struct of args of a audio adapter, for example, RtAudioCallback for RtAudio.
    using AudioCallbackFunction = std::function<AudioCallbackResult(void* outputBuffer, void* inputBuffer, unsigned int nFrames, double streamTime, AudioStreamStatuses status, RawArgsType rawArgs, UserDataType userData)>;

    enum AudioApi {
        Unspecified = 0,
        WindowsWasapi,
        WindowsDirectSound,
        WindowsAsio,
        WindowsWmme = WindowsAsio,
        MacOsxCore,
        LinuxAlsa,
        LinuxOss,
        UnixJack,
        Dummy
    };

    struct AudioDeviceInfo {
        unsigned int ID{};              /*!< Device ID used to specify a device to RtAudio. */
        std::string name;               /*!< Character string device name. */
        unsigned int outputChannels{};  /*!< Maximum output channels supported by device. */
        unsigned int inputChannels{};   /*!< Maximum input channels supported by device. */
        unsigned int duplexChannels{};  /*!< Maximum simultaneous input/output channels supported by device. */
        bool isDefaultOutput{ false };         /*!< true if this is the default output device. */
        bool isDefaultInput{ false };          /*!< true if this is the default input device. */
        std::vector<unsigned int> sampleRates; /*!< Supported sample rates (queried from list of standard rates). */
        unsigned int currentSampleRate{};   /*!< Current sample rate, system sample rate as currently configured. */
        unsigned int preferredSampleRate{}; /*!< Preferred sample rate, e.g. for WASAPI the system sample rate. */
        AudioFormats nativeFormats{ AudioFormat{0} };  /*!< Bit mask of supported data formats. */
    };

private:
    using CreateFunction = std::function<AudioAdapter*(AudioApi api, AudioErrorCallback errorCallback)>;

public:
    static AudioFormat avSampleFormatToTargetAudioFormat(AVSampleFormat sampleFmt, AudioFormat defaultValue = AudioFormat::AFSignedInt16, bool bThrowIfError = false) noexcept(false) {
        AudioFormat audioFmt = defaultValue;
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
            audioFmt = AFSignedInt8;
            break;
        case AV_SAMPLE_FMT_S16:
            audioFmt = AFSignedInt16;
            break;
        case AV_SAMPLE_FMT_S32:
            audioFmt = AFSignedInt16;
            break;
        case AV_SAMPLE_FMT_FLT:
            audioFmt = AFFloat32;
            break;
        case AV_SAMPLE_FMT_DBL:
            audioFmt = AFFloat64;
            break;
        case AV_SAMPLE_FMT_U8P: // planar formats 即分离格式，处理的时候AVFrame::data数组中每个通道的数据是分开存储的
            audioFmt = AFSignedInt8;
            //throwErr("AV_SAMPLE_FMT_U8P");
            break;
        case AV_SAMPLE_FMT_S16P:
            audioFmt = AFSignedInt16;
            //throwErr("AV_SAMPLE_FMT_S16P");
            break;
        case AV_SAMPLE_FMT_S32P:
            audioFmt = AFSignedInt32;
            //throwErr("AV_SAMPLE_FMT_S32P");
            break;
        case AV_SAMPLE_FMT_FLTP:
            audioFmt = AFFloat32;
            //throwErr("AV_SAMPLE_FMT_FLTP");
            break;
        case AV_SAMPLE_FMT_DBLP:
            audioFmt = AFFloat64;
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
        return audioFmt;
    }

    // Get the current used audio API, nullptr if not match
    template <typename T = AudioAdapter>
    T*& getCurrentApi() {
        return dynamic_cast<T*>(this);
    }

    virtual unsigned int getDeviceCount() = 0;

    virtual AudioDeviceInfo getDeviceInfo(unsigned int deviceId) = 0;

    virtual unsigned int getDefaultOutputDevice() = 0;

    //! A public function for opening a stream with the specified parameters.
    /*!
      An RTAUDIO_SYSTEM_ERROR is returned if a stream cannot be
      opened with the specified parameters or an error occurs during
      processing.  An RTAUDIO_INVALID_USE is returned if a stream
      is already open or any invalid stream parameters are specified.

      \param outputParameters Specifies output stream parameters to use
             when opening a stream, including a device ID, number of channels,
             and starting channel number.  For input-only streams, this
             argument should be NULL.  The device ID is a value returned by
             getDeviceIds().
      \param inputParameters Specifies input stream parameters to use
             when opening a stream, including a device ID, number of channels,
             and starting channel number.  For output-only streams, this
             argument should be NULL.  The device ID is a value returned by
             getDeviceIds().
      \param format An RtAudioFormat specifying the desired sample data format.
      \param sampleRate The desired sample rate (sample frames per second).
      \param bufferFrames A pointer to a value indicating the desired
             internal buffer size in sample frames.  The actual value
             used by the device is returned via the same pointer.  A
             value of zero can be specified, in which case the lowest
             allowable value is determined.
      \param callback A client-defined function that will be invoked
             when input data is available and/or output data is needed.
      \param userData An optional pointer to data that can be accessed
             from within the callback function.
      \param options An optional pointer to a structure containing various
             global stream options, including a list of OR'ed RtAudioStreamFlags
             and a suggested number of stream buffers that can be used to
             control stream latency.  More buffers typically result in more
             robust performance, though at a cost of greater latency.  If a
             value of zero is specified, a system-specific median value is
             chosen.  If the RTAUDIO_MINIMIZE_LATENCY flag bit is set, the
             lowest allowable value is used.  The actual value used is
             returned via the structure argument.  The parameter is API dependent.
    */
    virtual AudioErrorType openStream(AudioStreamParameters* outputParameters,
        AudioStreamParameters* inputParameters,
        AudioFormats format, unsigned int sampleRate,
        unsigned int* bufferFrames, AudioCallbackFunction callback,
        std::any userData = 0, AudioStreamOptions* options = 0) = 0;

    //! A function that closes a stream and frees any associated stream memory.
    /*!
      If a stream is not open, an RTAUDIO_WARNING will be passed to the
      user-provided errorCallback function (or otherwise printed to
      stderr).
    */
    virtual void closeStream() = 0;

    //! A function that starts a stream.
    /*!
      An RTAUDIO_SYSTEM_ERROR is returned if an error occurs during
      processing. An RTAUDIO_WARNING is returned if a stream is not open
      or is already running.
    */
    virtual AudioErrorType startStream() = 0;

    //! Stop a stream, allowing any samples remaining in the output queue to be played.
    /*!
      An RTAUDIO_SYSTEM_ERROR is returned if an error occurs during
      processing.  An RTAUDIO_WARNING is returned if a stream is not
      open or is already stopped.
    */
    virtual AudioErrorType stopStream() = 0;

    //! Stop a stream, discarding any samples remaining in the input/output queue.
    /*!
      An RTAUDIO_SYSTEM_ERROR is returned if an error occurs during
      processing.  An RTAUDIO_WARNING is returned if a stream is not
      open or is already stopped.
    */
    virtual AudioErrorType abortStream() = 0;

    //! Retrieve the error message corresponding to the last error or warning condition.
    /*!
      This function can be used to get a detailed error message when a
      non-zero RtAudioErrorType is returned by a function. This is the
      same message sent to the user-provided errorCallback function.
    */
    virtual const std::string getLastErrorText() = 0;

    //! Returns true if a stream is open and false if not.
    virtual bool isStreamOpen() const = 0;

    //! Returns true if the stream is running and false if it is stopped or not open.
    virtual bool isStreamRunning() const = 0;

    //! Returns the number of seconds of processed data since the stream was started.
    /*!
      The stream time is calculated from the number of sample frames
      processed by the underlying audio system, which will increment by
      units of the audio buffer size. It is not an absolute running
      time. If a stream is not open, the returned value may not be
      valid.
    */
    virtual double getStreamTime() = 0;

    //! Set the stream time to a time in seconds greater than or equal to 0.0.
    virtual void setStreamTime(double time) = 0;

    //! Returns the internal stream latency in sample frames.
    /*!
      The stream latency refers to delay in audio input and/or output
      caused by internal buffering by the audio system and/or hardware.
      For duplex streams, the returned value will represent the sum of
      the input and output latencies.  If a stream is not open, the
      returned value will be invalid.  If the API does not report
      latency, the return value will be zero.
    */
    virtual long getOutputStreamLatency() = 0;
    //! Returns the internal stream latency in sample frames.
    /*!
      The stream latency refers to delay in audio input and/or output
      caused by internal buffering by the audio system and/or hardware.
      For duplex streams, the returned value will represent the sum of
      the input and output latencies.  If a stream is not open, the
      returned value will be invalid.  If the API does not report
      latency, the return value will be zero.
    */
    virtual long getInputStreamLatency() = 0;

    //! Returns actual sample rate in use by the (open) stream.
    /*!
      On some systems, the sample rate used may be slightly different
      than that specified in the stream parameters.  If a stream is not
      open, a value of zero is returned.
    */
    virtual unsigned int getStreamSampleRate() = 0;

    //! Set a client-defined function that will be invoked when an error or warning occurs.
    virtual void setErrorCallback(AudioErrorCallback errorCallback) = 0;

    //! Specify whether warning messages should be output or not.
    /*!
      The default behaviour is for warning messages to be output,
      either to a client-defined error callback function (if specified)
      or to stderr.
    */
    virtual void setShouldShowWarnings(bool value = true) = 0;

};


class AudioAdapterFactory
{
public:
    enum AudioAdapterAdapter {
        RtAudioAdapter,
        PortAudioAdapter
    };
private:
    //friend class AudioAdapter;
    static std::unordered_map<AudioAdapterAdapter, AudioAdapter::CreateFunction> compiledAudioAdapterAdapters;
public:
    static AudioAdapter* create(AudioAdapterAdapter adapter, AudioAdapter::AudioApi audioApi = AudioAdapter::Unspecified, AudioAdapter::AudioErrorCallback errorCallback = nullptr);

};
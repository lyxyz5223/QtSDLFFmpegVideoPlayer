#pragma once

#include "PlayerPredefine.h"
#include "VideoPlayer.h"
#include "AudioPlayer.h"

class MediaPlayer : public PlayerInterface
{
public:
    enum class AVSyncMode {
        //AudioMaster = 0,
        //VideoMaster = 1,
        VideoSyncToAudio = 0, // 视频同步到音频
        AudioSyncToVideo = 1, // 音频同步到视频
        AudioVideoSyncToClock = 2, // 音视频都与时钟同步
        OnlyVideoSyncToClock = 3, // 仅视频同步时钟
    };

    using VideoPlayOptions = VideoPlayer::VideoPlayOptions;
    using AudioPlayOptions = AudioPlayer::AudioPlayOptions;
    using VideoRenderFunction = VideoPlayer::VideoRenderFunction;
    using VideoUserDataType = VideoPlayer::UserDataType;
    using VideoFrameSwitchOptions = VideoPlayer::VideoFrameSwitchOptions;
    using VideoFrameContext = VideoPlayer::FrameContext;
    using VideoFrameSwitchOptionsCallback = VideoPlayer::VideoFrameSwitchOptionsCallback;
    using VideoClockSyncFunction = VideoPlayer::VideoClockSyncFunction;
    using AudioClockSyncFunction = AudioPlayer::AudioClockSyncFunction;

    struct MediaPlayOptions {
        StreamIndexSelector streamIndexSelector{ nullptr };
        VideoClockSyncFunction clockSyncFunction{ nullptr };
        VideoRenderFunction renderer{ nullptr };
        VideoUserDataType rendererUserData{ VideoUserDataType{} };
        VideoFrameSwitchOptions frameSwitchOptions;
        bool enableHardwareDecoding{ false };
    };

private:
    DefinePlayerLoggerSinks(loggerSinks, "MediaPlayer.class.log");
    Logger logger{ "MediaPlayer", loggerSinks };
protected:
    AudioPlayer audioPlayer;
    VideoPlayer videoPlayer;
    Atomic<AVSyncMode> avSyncMode{ AVSyncMode::VideoSyncToAudio };
    std::string filePath;

    SharedMutex mtxVideoSeekCount;
    SharedMutex mtxAudioSeekCount;
    AtomicInt videoSeekProcessingCount{ 0 };
    AtomicInt audioSeekProcessingCount{ 0 };
    RequestTaskProcessCallbacks videoRequestTaskProcessCallbacks{
        [&] (const RequestTaskItem& taskItem) {
            if (taskItem.type == RequestTaskType::Seek)
            {
                std::unique_lock lock(mtxVideoSeekCount);
                videoSeekProcessingCount.fetch_add(1);
            }
        },
        [&] (const RequestTaskItem& taskItem) {
            if (taskItem.type == RequestTaskType::Seek)
            {
                std::unique_lock lock(mtxVideoSeekCount);
                videoSeekProcessingCount.fetch_sub(1);
            }
        }
    };
    RequestTaskProcessCallbacks audioRequestTaskProcessCallbacks{
        [&] (const RequestTaskItem& taskItem) {
            if (taskItem.type == RequestTaskType::Seek)
            {
                std::unique_lock lock(mtxAudioSeekCount);
                audioSeekProcessingCount.fetch_add(1);
            }
        },
        [&] (const RequestTaskItem& taskItem) {
            if (taskItem.type == RequestTaskType::Seek)
            {
                std::unique_lock lock(mtxAudioSeekCount);
                audioSeekProcessingCount.fetch_sub(1);
            }
        }
    };

    inline RequestTaskProcessCallbacks combineRequestTaskProcessCallbacks(RequestTaskProcessCallbacks& cb1, RequestTaskProcessCallbacks& cb2) {
        return {
            [cb1=cb1.beforeProcess, cb2=cb2.beforeProcess](const RequestTaskItem& item) {
                if (cb1)
                    cb1(item);
                if (cb2)
                    cb2(item);
            },
            [cb1 = cb1.afterProcess, cb2 = cb2.afterProcess](const RequestTaskItem& item) {
                if (cb1)
                    cb1(item);
                if (cb2)
                    cb2(item);
            }
        };
    }

    StreamIndexSelector streamIndexSelector = [](StreamIndexType& outStreamIndex, MediaType type, const std::vector<StreamIndexType>& streamIndicesList, const AVFormatContext* fmtCtx, const AVCodecContext* codecCtx) {
        if (streamIndicesList.empty())
            return false;
        else
            outStreamIndex = streamIndicesList[0];
        return true;
        };

    AtomicDouble audioClock{ 0.0 };
    //AtomicDouble videoClock{ 0.0 };
    AtomicBool isAudioClockStable{ false };
    AudioClockSyncFunction audioClockSyncFunction = [&](const AtomicDouble& audioClock, const AtomicBool& isClockStable, const double& audioRealtimeClock, int64_t& sleepTime) {
        this->audioClock.store(audioClock.load());
        this->isAudioClockStable.store(isClockStable.load());
        return false;
        };
    VideoClockSyncFunction videoClockSyncFunction = [&](const AtomicDouble& videoClock, const AtomicBool& isClockStable, const double& videoRealtimeClock, int64_t& sleepTime) {
        /*
        // 先进行视频帧提前/落后计算
        if (!audioPlaybackFinished) // 如果音频还在播放
        {
            // 精确的帧率控制
            double sleepTime = videoClock - audioClock.load();
            // 误差低通滤波
            avgDiff = avgDiff * 0.9 + sleepTime * 0.1;   // 简单 IIR 滤波

            if (avgDiff > 0.01)
            { // 10 ms 以上才处理
                size_t delay = (avgDiff * 1000);
                if (delay > 0) // 如果avgDiff值很小，delay可能为0
                {
                    ThreadSleepMs(delay);
                    logger.info("Video ahead: {} ms", delay);
                }
            }
            else if (avgDiff < -0.1)
            { // 落后 100 ms 再跳帧
                logger.info("Video behind: {} ms", -avgDiff * 1000);
                continue;
            }
            //SDL_Log("视频时间戳: %.3f, 实际时间: %.3f", videoClock, actualTime);
        }
        else // 音频播放结束
        {
            // 如果音频播放结束，则按固定帧率播放视频
            ThreadSleepMs(frameDuration * 1000);
        }
        */
        std::shared_lock lockMtxVideoSeek(mtxVideoSeekCount, std::defer_lock);
        std::shared_lock lockMtxAudioSeek(mtxAudioSeekCount, std::defer_lock);
        std::lock(lockMtxVideoSeek, lockMtxAudioSeek);
        if (0 == audioSeekProcessingCount && videoSeekProcessingCount == 0
            && isAudioClockStable.load() && isClockStable.load())
        {
            if (audioClock.load())
            {
                double sleepTimeS = videoClock.load() - audioClock.load(); // 单位：秒
                sleepTime = static_cast<int64_t>(sleepTimeS * 1000); // 转换为毫秒
            }
            else
            {
                double sleepTimeS = videoClock.load() - videoRealtimeClock; // 单位：秒
                sleepTime = static_cast<int64_t>(sleepTimeS * 1000); // 转换为毫秒
            }
            return true; // 直接返回true，交给调用者处理
            // 返回值true && (sleepTime != 0)表示需要睡眠/丢帧，false || (sleepTime == 0)表示不需要
        }
        return false;
        };

public:
    ~MediaPlayer() {
        if (!this->isStopped())
            this->stop();
    }


    // 如果使用VideoPlayOptions，则enableHardwareDecoder会失效
    bool play(const std::string& filePath, MediaPlayOptions options) {

    }
    bool play(const std::string& filePath, VideoPlayOptions videoOptions, AudioPlayOptions audioOptions) {
        this->setFilePath(filePath);
        bool ar = true;
        bool vr = true;
        std::thread videoThread([&] { vr = this->videoPlayer.play(filePath, videoOptions); });
        std::thread audioThread([&] { ar = this->audioPlayer.play(filePath, audioOptions); });
        if (audioThread.joinable())
            audioThread.join();
        if (videoThread.joinable())
            videoThread.join();
        return ar && vr;
    }
    bool play() override {
        bool ar = true;
        bool vr = true;
        std::thread videoThread([&] { vr = this->videoPlayer.play(); });
        std::thread audioThread([&] { ar = this->audioPlayer.play(); });
        if (audioThread.joinable())
            audioThread.join();
        if (videoThread.joinable())
            videoThread.join();
        return ar && vr;
    }
    void resume() override {
        this->audioPlayer.resume();
        this->videoPlayer.resume();
    }
    void pause() override {
        this->audioPlayer.pause();
        this->videoPlayer.pause();
    }
    void notifyStop() override {
        std::thread videoThread([&] { this->videoPlayer.notifyStop(); });
        std::thread audioThread([&] { this->audioPlayer.notifyStop(); });
        if (audioThread.joinable())
            audioThread.join();
        if (videoThread.joinable())
            videoThread.join();
    }
    void stop() override {
        std::thread videoThread([&] { this->videoPlayer.stop(); });
        std::thread audioThread([&] { this->audioPlayer.stop(); });
        if (audioThread.joinable())
            audioThread.join();
        if (videoThread.joinable())
            videoThread.join();
    }
    // 跳转到指定pts位置
    // \param streamIndex -1表示使用AV_TIME_BASE计算，否则使用streamIndex指定的流的time_base
    void notifySeek(uint64_t pts, StreamIndexType streamIndex = -1, RequestTaskProcessCallbacks callbacks = RequestTaskProcessCallbacks{}) override {
        RequestTaskProcessCallbacks videoCallbacks{ combineRequestTaskProcessCallbacks(videoRequestTaskProcessCallbacks, callbacks) };
        RequestTaskProcessCallbacks audioCallbacks{ combineRequestTaskProcessCallbacks(audioRequestTaskProcessCallbacks, callbacks) };
        std::thread videoThread([&] { this->videoPlayer.notifySeek(pts, streamIndex, videoCallbacks); });
        std::thread audioThread([&] { this->audioPlayer.notifySeek(pts, streamIndex, audioCallbacks); });
        if (audioThread.joinable())
            audioThread.join();
        if (videoThread.joinable())
            videoThread.join();
    }
    void seek(uint64_t pts, StreamIndexType streamIndex = -1, RequestTaskProcessCallbacks callbacks = RequestTaskProcessCallbacks{}) override {
        RequestTaskProcessCallbacks videoCallbacks{ combineRequestTaskProcessCallbacks(videoRequestTaskProcessCallbacks, callbacks) };
        RequestTaskProcessCallbacks audioCallbacks{ combineRequestTaskProcessCallbacks(audioRequestTaskProcessCallbacks, callbacks) };
        std::thread videoThread([&] { this->videoPlayer.seek(pts, streamIndex, videoCallbacks); });
        std::thread audioThread([&] { this->audioPlayer.seek(pts, streamIndex, audioCallbacks); });
        if (audioThread.joinable())
            audioThread.join();
        if (videoThread.joinable())
            videoThread.join();
    }

    bool isPlaying() const override {
        return this->audioPlayer.isPlaying() && this->videoPlayer.isPlaying();
    }
    bool isPaused() const override {
        return this->audioPlayer.isPaused() && this->videoPlayer.isPaused();
    }
    bool isStopped() const override {
        return this->audioPlayer.isStopped() && this->videoPlayer.isStopped();
    }
    void setFilePath(const std::string& filePath) {
        this->filePath = filePath;
        this->videoPlayer.setFilePath(filePath);
        this->audioPlayer.setFilePath(filePath);
    }
    std::string getFilePath() const {
        return this->filePath;
    }
    void setAVSyncMode(AVSyncMode mode) {
        //this->avSyncMode = mode;
        this->avSyncMode.store(mode);
    }


    void setStreamIndexSelector(const StreamIndexSelector& selector) override {
        this->videoPlayer.setStreamIndexSelector(selector);
        this->audioPlayer.setStreamIndexSelector(selector);
    }
    void setStreamIndexSelector(const StreamIndexSelector& videoSelector, const StreamIndexSelector& audioSelector) {
        this->videoPlayer.setStreamIndexSelector(videoSelector);
        this->audioPlayer.setStreamIndexSelector(audioSelector);
    }

    void setClockSyncFunction(const VideoClockSyncFunction& videoSync, const AudioClockSyncFunction& audioSync) {
        this->videoPlayer.setClockSyncFunction(videoSync);
        this->audioPlayer.setClockSyncFunction(audioSync);
    }

    void setRenderer(const VideoRenderFunction& renderer, VideoUserDataType userData) {
        this->videoPlayer.setRenderer(renderer, userData);
    }

    void enableHardwareDecoder(bool enabled) {
        this->videoPlayer.enableHardwareDecoding(enabled);
    }

    void setFrameSwitchOptionsCallback(VideoFrameSwitchOptionsCallback callback, VideoUserDataType userData) {
        this->videoPlayer.setFrameSwitchOptionsCallback(callback, userData);
    }

    StreamTypes getStreamTypes() {
        AVFormatContext* fmtCtx = nullptr;
        // 打开文件
        if (!openInput(filePath, fmtCtx))
            return StreamType::STNone;
        // 查找流信息
        if (!findStreamInfo(fmtCtx))
            return StreamType::STNone;
        // 查找流
        auto t = findStreams(fmtCtx);
        avformat_close_input(&fmtCtx);
        return t;
    }

private:
    // 打开文件
    bool openInput(const std::string& filePath, AVFormatContext*& fmtCtx) {
        return MediaDecodeUtils::openFile(&logger, fmtCtx, filePath);
    }

    // 查找流信息
    bool findStreamInfo(AVFormatContext*& fmtCtx) {
        return MediaDecodeUtils::findStreamInfo(&logger, fmtCtx);
    }

    StreamTypes findStreams(AVFormatContext* formatCtx);
};


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
    using AudioFrameContext = AudioPlayer::FrameContext;
    using VideoFrameSwitchOptionsCallback = VideoPlayer::VideoFrameSwitchOptionsCallback;
    using VideoClockSyncFunction = VideoPlayer::VideoClockSyncFunction;
    using AudioClockSyncFunction = AudioPlayer::AudioClockSyncFunction;
    using MediaStreamIndexSelector = std::function<bool(StreamIndexType& videoStreamIndex, StreamIndexType& audioStreamIndex, const std::unordered_multimap<StreamType, StreamIndexType>& multimapStreamIndices, const AVFormatContext* fmtCtx, const AVCodecContext* codecCtx)>;
    using VideoRenderEvent = VideoPlayer::VideoRenderEvent;
    using AudioRenderEvent = AudioPlayer::AudioRenderEvent;


    struct MediaPlayOptions {
        VideoRenderFunction renderer{ nullptr }; // Video
        VideoUserDataType rendererUserData{ VideoUserDataType{} }; // Video
        VideoFrameSwitchOptionsCallback frameSwitchOptionsCallback{ nullptr }; // Video
        VideoUserDataType frameSwitchOptionsCallbackUserData{ VideoUserDataType{} }; // Video
        bool enableHardwareDecoding{ false }; // Video
    };

    class MediaVideoPlayer : public VideoPlayer {
        MediaPlayer& player;
    public:
        MediaVideoPlayer(MediaPlayer& player) : player(player) {}
        virtual void playbackStateChangeEvent(MediaPlaybackStateChangeEvent* e) override {
            //PlayerState old = PlayerState::Stopped;
            //if (player.playerState.trySet(e->state(), old))
            //{
            //    if (e->state() == old) return;
            //    MediaPlaybackStateChangeEvent mediaEvent{ player.playerState.load(), player.playerState.prev() };
            //    player.event(&mediaEvent);
            //}
            auto mediaEvent = e->clone();
            player.event(mediaEvent.get());
        }
        virtual void startEvent(MediaPlaybackStateChangeEvent* e) override {
            player.logger.info("Video playback started.");
        }
        virtual void pauseEvent(MediaPlaybackStateChangeEvent* e) override {
            player.logger.info("Video playback paused.");
        }
        virtual void resumeEvent(MediaPlaybackStateChangeEvent* e) override {
            player.logger.info("Video playback resumed.");
        }
        virtual void stopEvent(MediaPlaybackStateChangeEvent* e) override {
            player.logger.info("Video playback stopped.");
        }
        virtual void requestHandleEvent(MediaRequestHandleEvent* e) override {
            player.event(e);
        }
        // seek进度调整事件
        virtual void seekEvent(MediaSeekEvent* e) override {
            switch (e->handleState())
            {
            case RequestHandleState::BeforeEnqueue:
                player.videoSeekingCount.fetch_add(1);
                break;
            case RequestHandleState::AfterEnqueue:
                break;
            case RequestHandleState::BeforeHandle:
                player.logger.info("Video seek requested to pts: {}, stream index: {}", e->timestamp(), e->streamIndex());
                break;
            case RequestHandleState::AfterHandle:
                player.videoSeekingCount.fetch_sub(1);
                break;
            default:
                break;
            }
        }
        virtual void renderEvent(VideoRenderEvent* e) override {
            // 转发渲染事件
            player.event(e);
        }
    };
    class MediaAudioPlayer : public AudioPlayer {
        MediaPlayer& player;
    public:
        MediaAudioPlayer(MediaPlayer& player) : player(player) {}
        virtual void playbackStateChangeEvent(MediaPlaybackStateChangeEvent* e) override {
            auto mediaEvent = e->clone();
            player.event(mediaEvent.get());
        }
        virtual void startEvent(MediaPlaybackStateChangeEvent* e) override {
            player.logger.info("Audio playback started.");
        }
        virtual void pauseEvent(MediaPlaybackStateChangeEvent* e) override {
            player.logger.info("Audio playback paused.");
        }
        virtual void resumeEvent(MediaPlaybackStateChangeEvent* e) override {
            player.logger.info("Audio playback resumed.");
        }
        virtual void stopEvent(MediaPlaybackStateChangeEvent* e) override {
            player.logger.info("Audio playback stopped.");
        }
        virtual void requestHandleEvent(MediaRequestHandleEvent* e) override {
            player.event(e);
        }
        virtual void seekEvent(MediaSeekEvent* e) override {
            switch (e->handleState())
            {
            case RequestHandleState::BeforeEnqueue:
                player.audioSeekingCount.fetch_add(1);
                break;
            case RequestHandleState::AfterEnqueue:
                break;
            case RequestHandleState::BeforeHandle:
                player.logger.info("Audio seek requested to pts: {}, stream index: {}", e->timestamp(), e->streamIndex());
                break;
            case RequestHandleState::AfterHandle:
                player.audioSeekingCount.fetch_sub(1);
                break;
            default:
                break;
            }
        }
        virtual void renderEvent(AudioRenderEvent* e) override {
            // 转发渲染事件
            player.event(e);
        }
    };
    friend class MediaVideoPlayer;
    friend class MediaAudioPlayer;


private:
    DefinePlayerLoggerSinks(loggerSinks, "MediaPlayer.class.log");
    Logger logger{ "MediaPlayer", loggerSinks };
//protected:
    std::string filePath;
    Atomic<AVSyncMode> avSyncMode{ AVSyncMode::VideoSyncToAudio };
    // 默认的流索引选择器：选择第一个流
    StreamIndexSelector streamIndexSelector = [](StreamIndexType& outStreamIndex, MediaType type, const std::vector<StreamIndexType>& streamIndicesList, const AVFormatContext* fmtCtx, const AVCodecContext* codecCtx) -> bool {
        if (streamIndicesList.empty())
            return false;
        else
            outStreamIndex = streamIndicesList[0];
        return true;
        };

    AtomicDouble audioClock{ 0.0 }; // 音频时钟
    AtomicBool isAudioClockStable{ false }; // 音频时钟是否稳定（seek设置后会不稳定，正常播放才会趋于稳定）
    AtomicStateMachine<PlayerState> playerState{ PlayerState::Stopped };
    AtomicInt videoSeekingCount{ 0 }; // 视频seek处理中计数
    AtomicInt audioSeekingCount{ 0 }; // 音频seek处理中计数

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
        if (0 == videoSeekingCount && audioSeekingCount == 0
            && isAudioClockStable.load() && isClockStable.load())
        {
            if (audioClock.load())
            {
                double sleepTimeS = videoClock.load() - audioClock.load(); // 单位：秒
                sleepTime = static_cast<int64_t>(sleepTimeS * 1000); // 转换为毫秒
            }
            return true; // 直接返回true，交给调用者处理
            // 返回值true && (sleepTime != 0)表示需要睡眠/丢帧，false || (sleepTime == 0)表示不需要
        }
        logger.trace() << "videoSeekingCount:" << videoSeekingCount.load() << ", audioSeekingCount:" << audioSeekingCount.load()
            << ", isAudioClockStable:" << isAudioClockStable.load() << ", isClockStable:" << isClockStable.load();
        return false;
        };

    MediaVideoPlayer videoPlayer{ *this };
    MediaAudioPlayer audioPlayer{ *this };

public:
    MediaPlayer() : PlayerInterface(logger) {}
    ~MediaPlayer() {
        if (!this->isStopped())
            this->stop();
    }

    // 如果使用VideoPlayOptions，则enableHardwareDecoder会失效
    bool play(const std::string& filePath, MediaPlayOptions options) {
        VideoPlayOptions videoOptions{
            streamIndexSelector,
            videoClockSyncFunction,
            options.renderer,
            options.rendererUserData,
            options.frameSwitchOptionsCallback,
            options.frameSwitchOptionsCallbackUserData,
            options.enableHardwareDecoding
        };
        AudioPlayOptions audioOptions{
            streamIndexSelector,
            audioClockSyncFunction
        };
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
    virtual bool play() override {
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
    virtual void resume() override {
        this->audioPlayer.resume();
        this->videoPlayer.resume();
    }
    virtual void pause() override {
        this->audioPlayer.pause();
        this->videoPlayer.pause();
    }
    virtual void notifyStop() override {
        std::thread videoThread([&] { this->videoPlayer.notifyStop(); });
        std::thread audioThread([&] { this->audioPlayer.notifyStop(); });
        if (audioThread.joinable())
            audioThread.join();
        if (videoThread.joinable())
            videoThread.join();
    }
    virtual void stop() override {
        std::thread videoThread([&] { this->videoPlayer.stop(); });
        std::thread audioThread([&] { this->audioPlayer.stop(); });
        if (audioThread.joinable())
            audioThread.join();
        if (videoThread.joinable())
            videoThread.join();
    }

    // 跳转到指定pts位置
    // \param streamIndex -1表示使用AV_TIME_BASE计算，否则使用streamIndex指定的流的time_base
    virtual void notifySeek(uint64_t pts, StreamIndexType streamIndex = -1) override {
        videoSeekingCount.fetch_add(1); // Fix the problem that audio haven't seeked yet when video seeked complete
        audioSeekingCount.fetch_add(1); // 修复视频seek完成时音频还没有seek的问题
        std::thread videoThread([&] { this->videoPlayer.notifySeek(pts, streamIndex); });
        std::thread audioThread([&] { this->audioPlayer.notifySeek(pts, streamIndex); });
        if (audioThread.joinable())
            audioThread.join();
        if (videoThread.joinable())
            videoThread.join();
        videoSeekingCount.fetch_sub(1); // Fix
        audioSeekingCount.fetch_sub(1); // Fix
    }
    virtual void seek(uint64_t pts, StreamIndexType streamIndex = -1) override {
        videoSeekingCount.fetch_add(1); // Fix the problem that audio haven't seeked yet when video seeked complete
        audioSeekingCount.fetch_add(1); // 修复视频seek完成时音频还没有seek的问题
        std::thread videoThread([&] { this->videoPlayer.seek(pts, streamIndex); });
        std::thread audioThread([&] { this->audioPlayer.seek(pts, streamIndex); });
        if (audioThread.joinable())
            audioThread.join();
        if (videoThread.joinable())
            videoThread.join();
        videoSeekingCount.fetch_sub(1); // Fix
        audioSeekingCount.fetch_sub(1); // Fix
    }

    virtual bool isPlaying() const override {
        return this->audioPlayer.isPlaying() && this->videoPlayer.isPlaying();
    }
    virtual bool isPaused() const override {
        return this->audioPlayer.isPaused() && this->videoPlayer.isPaused();
    }
    virtual bool isStopped() const override {
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

    void setStreamIndexSelector(const StreamIndexSelector& selector) {
        this->streamIndexSelector = selector;
    }
    
    void setRenderer(const VideoRenderFunction& renderer, VideoUserDataType userData) {
        this->videoPlayer.setRenderer(renderer, userData);
    }

    void setFrameSwitchOptionsCallback(VideoFrameSwitchOptionsCallback callback, VideoUserDataType userData) {
        this->videoPlayer.setFrameSwitchOptionsCallback(callback, userData);
    }

    void enableHardwareDecoding(bool enabled) {
        this->videoPlayer.enableHardwareDecoding(enabled);
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


protected:
    virtual bool event(MediaEvent* e) override {
        // 分发事件
        if (e->type() == MediaEventType::Render)
        {
            if (e->streamType() & StreamType::STVideo)
                videoRenderEvent(static_cast<VideoRenderEvent*>(e));
            if (e->streamType() & StreamType::STAudio)
                audioRenderEvent(static_cast<AudioRenderEvent*>(e));
        }
        return PlayerInterface::event(e);
    }

    // 视频渲染事件
    virtual void videoRenderEvent(VideoRenderEvent* e) {
        auto ctx = e->frameContext();
        logger.trace("Video render event: pts={}, width={}, height={}", ctx->swRawFrame->pts, ctx->swRawFrame->width, ctx->swRawFrame->height);
    }

    virtual void audioRenderEvent(AudioRenderEvent* e) {
        auto ctx = e->frameContext();
        logger.trace("Audio render event: pts={}, streamTime={}, numberOfChannels={}, sampleRate={}, dataSize={}", ctx->frameTime, ctx->streamTime, ctx->numberOfChannels, ctx->sampleRate, ctx->dataSize);
    }

    // 重写事件处理函数
    // 播放状态改变事件（重写该方法不会影响其子事件（start,pause,resume,stop）的事件分发）
    virtual void playbackStateChangeEvent(MediaPlaybackStateChangeEvent* e) override {
        
    }
    // 已经开始播放事件
    virtual void startEvent(MediaPlaybackStateChangeEvent* e) override {
    }
    // 暂停播放事件
    virtual void pauseEvent(MediaPlaybackStateChangeEvent* e) override {
    }
    // 恢复播放事件
    virtual void resumeEvent(MediaPlaybackStateChangeEvent* e) override {
    }
    // 播放已经停止事件
    virtual void stopEvent(MediaPlaybackStateChangeEvent* e) override{
        }
    // 请求处理事件（重写该方法不会影响其子事件（seek）的事件分发）
    virtual void requestHandleEvent(MediaRequestHandleEvent* e) override {
    }
    // seek进度调整事件
    virtual void seekEvent(MediaSeekEvent* e) override {
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


#pragma once

#include "PlayerPredefine.h"
#include "VideoPlayer.h"
#include "AudioPlayer.h"

class MediaPlayer : public PlayerInterface
{
public:
    StreamTypes STREAM_TYPES = StreamType::STAll;

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
            if (player.demuxerMode == ComponentWorkMode::External)
                player.demuxer->start();
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
        // 清空缓冲区，包括队列和codecCtx中的buffer
        void clearBuffers() {
            VideoPlayer::clearBuffers();
        }
        int64_t clockSync(size_t pts, StreamIndexType streamIndex, bool isStable) {
            return VideoPlayer::clockSync(pts, streamIndex, isStable);
        }
    };
    class MediaAudioPlayer : public AudioPlayer {
        MediaPlayer& player;
    public:
        // 清空缓冲区，包括队列和codecCtx中的buffer
        void clearBuffers() {
            AudioPlayer::clearBuffers();
        }
        int64_t clockSync(size_t pts, StreamIndexType streamIndex, bool isStable) {
            return AudioPlayer::clockSync(pts, streamIndex, isStable);
        }
        MediaAudioPlayer(MediaPlayer& player) : player(player) {}
        virtual void playbackStateChangeEvent(MediaPlaybackStateChangeEvent* e) override {
            auto mediaEvent = e->clone();
            player.event(mediaEvent.get());
        }
        virtual void startEvent(MediaPlaybackStateChangeEvent* e) override {
            player.logger.info("Audio playback started.");
            if (player.demuxerMode == ComponentWorkMode::External)
                player.demuxer->start();
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
    const std::string loggerName{ "MediaPlayer" };
    DefinePlayerLoggerSinks(loggerSinks, loggerName);
    Logger logger{ loggerName, loggerSinks };
//protected:
    std::string filePath;
    Atomic<AVSyncMode> avSyncMode{ AVSyncMode::VideoSyncToAudio };
    // 默认的流索引选择器：选择第一个流
    StreamIndexSelector streamIndexSelector = [](StreamIndexType& outStreamIndex, StreamType type, const std::span<AVStream*>& streamsSpan, const AVFormatContext* fmtCtx) -> bool {
        for (auto& stream : streamsSpan)
        {
            AVMediaType mediaType = streamTypeToAVMediaType(type);
            if (mediaType != AVMEDIA_TYPE_NB && stream->codecpar->codec_type == mediaType)
            {
                outStreamIndex = stream->index;
                return true;
            }
        }
        return false;
        };

    AtomicDouble audioClock{ 0.0 }; // 音频时钟
    AtomicBool isAudioClockStable{ false }; // 音频时钟是否稳定（seek设置后会不稳定，正常播放才会趋于稳定）
    //AtomicStateMachine<PlayerState> playerState{ PlayerState::Stopped };
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

    //std::vector<UniquePtrD<PlayerInterface>> players{
    //    std::make_unique<MediaVideoPlayer>(*this), // 视频播放器
    //    std::make_unique<MediaAudioPlayer>(*this)  // 音频播放器
    //}; // 播放器列表
    //MediaVideoPlayer* videoPlayer{ static_cast<MediaVideoPlayer*>(players[0].get()) };
    //MediaAudioPlayer* audioPlayer{ static_cast<MediaAudioPlayer*>(players[1].get()) };
    UniquePtrD<MediaVideoPlayer> videoPlayer{ std::make_unique<MediaVideoPlayer>(*this) }; // 视频播放器
    UniquePtrD<MediaAudioPlayer> audioPlayer{ std::make_unique<MediaAudioPlayer>(*this) }; // 音频播放器

    // 状态，用于中转处理
    Atomic<PlayerState> playerState{ PlayerState::Stopped };
    // 解复用器
    SharedPtr<UnifiedDemuxer> demuxer{ std::make_shared<UnifiedDemuxer>(loggerName) };
    StreamTypes demuxerStreamTypes{ static_cast<StreamType>(STVideo | STAudio) };
    ComponentWorkMode demuxerMode{ ComponentWorkMode::External };
    ComponentWorkMode requestTaskQueueHandlerMode{ ComponentWorkMode::External };
    SharedPtr<RequestTaskQueueHandler> requestTaskQueueHandler{ std::make_shared<RequestTaskQueueHandler>(this) };

    // 无论是否异步执行，函数对象均进行拷贝，保证对象有效
    void execPlayerWithThreads(const std::vector<std::function<void()>>& functions, bool wait = true) {
        std::vector<std::thread> threads;
        for (const auto& func : functions)
            threads.emplace_back([func]() { func(); }); // 拷贝函数对象，以防止引用失效
        if (wait)
        {
            for (auto& thread : threads)
                if (thread.joinable())
                    thread.join();
        }
        else
        {
            for (auto& thread : threads)
                if (thread.joinable())
                    thread.detach();
        }
    }

    bool playMediaFile(const MediaPlayOptions& options) {
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
        bool ar = true;
        bool vr = true;
        execPlayerWithThreads({ [&] { waitComponentsStop(); }, [&] { vr = videoPlayer->play(filePath, videoOptions); }, [&] { ar = audioPlayer->play(filePath, audioOptions); } });
        return ar && vr;
    }
    bool playMediaFile() {
        bool ar = true;
        bool vr = true;
        execPlayerWithThreads({ [&] { waitComponentsStop(); }, [&] { vr = videoPlayer->play(); }, [&] { ar = audioPlayer->play(); }});
        return ar && vr;
    }
    bool prepareToPlay() {
        if (demuxerMode == ComponentWorkMode::External)
        {
            try {
                demuxer->openAndSelectStreams(filePath, demuxerStreamTypes, streamIndexSelector);
            }
            catch (std::exception e) {
                return false;
            }
        }
        if (requestTaskQueueHandlerMode == ComponentWorkMode::External)
            requestTaskQueueHandler->start();
        return true;
    }
    void waitComponentsStop() {
        if (demuxerMode == ComponentWorkMode::External)
            demuxer->waitStop();
        if (requestTaskQueueHandlerMode == ComponentWorkMode::External)
            requestTaskQueueHandler->waitStop();
    }

    bool shouldStop() const {
        auto s = getPlayerState();
        if (s == PlayerState::Stopped || s == PlayerState::Stopping)
            return true;
        return false;
    }
public:
    MediaPlayer() : PlayerInterface(logger) {
        setDemuxerMode(demuxerMode); // setDemuxerMode方法设置音视频使用外部解复用器
        setExternalDemuxer(demuxer); // 设置外部解复用器，由MediaPlayer管理解复用器
        setRequestTaskQueueHandlerMode(requestTaskQueueHandlerMode); // 设置音视频使用外部请求任务队列处理器
        setExternalRequestTaskQueueHandler(requestTaskQueueHandler);
    }
    ~MediaPlayer() {
        if (!this->isStopped())
            this->stop();
    }

    // 如果使用VideoPlayOptions，则enableHardwareDecoder会失效
    bool play(const std::string& filePath, const MediaPlayOptions& options) {
        this->setFilePath(filePath);
        if (!prepareToPlay())
            return false;
        return playMediaFile(options);
    }
    virtual bool play() override {
        if (!prepareToPlay())
            return false;
        return playMediaFile();
    }
    virtual void resume() override {
        execPlayerWithThreads({ [&] { videoPlayer->resume(); }, [&] { audioPlayer->resume(); } });
    }
    virtual void pause() override {
        execPlayerWithThreads({ [&] { videoPlayer->pause(); }, [&] { audioPlayer->pause(); } });
    }
    virtual void notifyStop() override {
        if (demuxerMode == ComponentWorkMode::External)
            demuxer->stop();
        execPlayerWithThreads({ [&] { videoPlayer->notifyStop(); }, [&] { audioPlayer->notifyStop(); } });
        if (requestTaskQueueHandlerMode == ComponentWorkMode::External)
            requestTaskQueueHandler->stop();
    }
    virtual void stop() override {
        if (demuxerMode == ComponentWorkMode::External)
            demuxer->stop();
        execPlayerWithThreads({ [&] { videoPlayer->stop(); }, [&] { audioPlayer->stop(); } });
        if (requestTaskQueueHandlerMode == ComponentWorkMode::External)
            requestTaskQueueHandler->stop();
    }

    // 跳转到指定pts位置，非精确跳转，使用AVSEEK_FLAG_BACKWARD标志从pts向前(回退)查找最近的关键帧
    // \param pts 跳转的时间戳，单位为streamIndex对应的time_base，若streamIndex为-1，则单位为1/AV_TIME_BASE
    // \param streamIndex -1表示使用AV_TIME_BASE计算，否则使用streamIndex指定的流的time_base
    virtual void notifySeek(uint64_t pts, StreamIndexType streamIndex = -1) override {
        videoSeekingCount.fetch_add(1); // Fix the problem that audio haven't seeked yet when video seeked complete
        audioSeekingCount.fetch_add(1); // 修复视频seek完成时音频还没有seek的问题
        if (demuxerMode == ComponentWorkMode::Internal)
            execPlayerWithThreads({ [&] { videoPlayer->notifySeek(pts, streamIndex); }, [&] { audioPlayer->notifySeek(pts, streamIndex); } });
        else
            requestTaskQueueHandler->push(RequestTaskType::Seek, { ThreadIdentifier::Demuxer, ThreadIdentifier::Decoder, ThreadIdentifier::Renderer }, new MediaSeekEvent{ StreamType::STAll, pts, streamIndex }, std::bind(&MediaPlayer::requestTaskHandlerSeek, this, std::placeholders::_1, std::placeholders::_2));
        videoSeekingCount.fetch_sub(1); // Fix
        audioSeekingCount.fetch_sub(1); // Fix
    }
    virtual void seek(uint64_t pts, StreamIndexType streamIndex = -1) override {
        videoSeekingCount.fetch_add(1); // Fix the problem that audio haven't seeked yet when video seeked complete
        audioSeekingCount.fetch_add(1); // 修复视频seek完成时音频还没有seek的问题
        if (demuxerMode == ComponentWorkMode::Internal)
            execPlayerWithThreads({ [&] { videoPlayer->seek(pts, streamIndex); }, [&] { audioPlayer->seek(pts, streamIndex); } });
        else
            requestTaskQueueHandler->push(RequestTaskType::Seek, { ThreadIdentifier::Demuxer, ThreadIdentifier::Decoder, ThreadIdentifier::Renderer }, new MediaSeekEvent{ StreamType::STAll, pts, streamIndex }, std::bind(&MediaPlayer::requestTaskHandlerSeek, this, std::placeholders::_1, std::placeholders::_2));
        videoSeekingCount.fetch_sub(1); // Fix
        audioSeekingCount.fetch_sub(1); // Fix
    }

    virtual bool isPlaying() const override {
        return audioPlayer->isPlaying() && videoPlayer->isPlaying();
    }
    virtual bool isPaused() const override {
        return audioPlayer->isPaused() && videoPlayer->isPaused();
    }
    virtual bool isStopped() const override {
        return audioPlayer->isStopped() && videoPlayer->isStopped();
    }
    virtual PlayerState getPlayerState() const override {
        PlayerState vs = videoPlayer->getPlayerState();
        PlayerState as = audioPlayer->getPlayerState();
        if (vs == as || vs != PlayerState::Stopped) // 优先返回非Stopped状态
            return vs;
        return as;
    }

    virtual void setFilePath(const std::string& filePath) override {
        this->filePath = filePath;
        this->videoPlayer->setFilePath(filePath);
        this->audioPlayer->setFilePath(filePath);
    }
    virtual std::string getFilePath() const override {
        return this->filePath;
    }
private:
    virtual void setDemuxerMode(ComponentWorkMode mode) override {
        this->videoPlayer->setDemuxerMode(mode);
        this->audioPlayer->setDemuxerMode(mode);
        this->demuxerMode = mode;
    }
    virtual ComponentWorkMode getDemuxerMode() const override {
        return this->demuxerMode;
    }
    virtual void setExternalDemuxer(const SharedPtr<UnifiedDemuxer>& demuxer) override {
        this->videoPlayer->setExternalDemuxer(demuxer);
        this->audioPlayer->setExternalDemuxer(demuxer);
    }
    virtual void setRequestTaskQueueHandlerMode(ComponentWorkMode mode) override {
        this->videoPlayer->setRequestTaskQueueHandlerMode(mode);
        this->audioPlayer->setRequestTaskQueueHandlerMode(mode);
        this->requestTaskQueueHandlerMode = mode;
    }
    virtual ComponentWorkMode getRequestTaskQueueHandlerMode() const override {
        return this->requestTaskQueueHandlerMode;
    }
    virtual void setExternalRequestTaskQueueHandler(const SharedPtr<RequestTaskQueueHandler>& handler) override {
        this->videoPlayer->setExternalRequestTaskQueueHandler(handler);
        this->audioPlayer->setExternalRequestTaskQueueHandler(handler);
    }

public:

    void setAVSyncMode(AVSyncMode mode) {
        //this->avSyncMode = mode;
        this->avSyncMode.store(mode);
    }

    void setStreamIndexSelector(const StreamIndexSelector& selector) {
        this->streamIndexSelector = selector;
    }
    
    void setRenderer(const VideoRenderFunction& renderer, VideoUserDataType userData) {
        this->videoPlayer->setRenderer(renderer, userData);
    }

    void setFrameSwitchOptionsCallback(VideoFrameSwitchOptionsCallback callback, VideoUserDataType userData) {
        this->videoPlayer->setFrameSwitchOptionsCallback(callback, userData);
    }

    void enableHardwareDecoding(bool enabled) {
        this->videoPlayer->enableHardwareDecoding(enabled);
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
    void setPlayerState(PlayerState state) {
        PlayerState oldState = getPlayerState();
        playerState.set(state/*, oldState*/);
        auto e = MediaPlaybackStateChangeEvent{ STREAM_TYPES, state, oldState };
        event(&e);
    }

    // 打开文件
    bool openInput(const std::string& filePath, AVFormatContext*& fmtCtx) {
        return MediaDecodeUtils::openFile(&logger, fmtCtx, filePath);
    }

    // 查找流信息
    bool findStreamInfo(AVFormatContext*& fmtCtx) {
        return MediaDecodeUtils::findStreamInfo(&logger, fmtCtx);
    }

    StreamTypes findStreams(AVFormatContext* formatCtx);

    void requestTaskHandlerSeek(MediaRequestHandleEvent* e, std::any userData);

};


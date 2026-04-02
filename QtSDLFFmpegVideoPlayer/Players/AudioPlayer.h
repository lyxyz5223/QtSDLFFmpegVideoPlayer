#pragma once
#include "PlayerPredefine.h"

// 音频库
#include <AudioAdapter.h>

class AudioPlayer : public AbstractPlayer, private ConcurrentQueueOps
{
public:
    // 用于AudioAdapter音频缓冲区大小
    static constexpr unsigned int DEFAULT_AUDIO_OUTPUT_STREAM_BUFFER_SIZE = 1024;
    // 用于音频队列
    static constexpr int MAX_AUDIO_OUTPUT_STREAM_QUEUE_SIZE = 5;
    static constexpr int MIN_AUDIO_OUTPUT_STREAM_QUEUE_SIZE = 3;// MAX_AUDIO_OUTPUT_STREAM_QUEUE_SIZE / 2; // 1 / 2
    // 默认音频输出通道数
    static constexpr int DEFAULT_NUMBER_CHANNELS_AUDIO_OUTPUT = 1;
    // 下面两个常量需同时满足，解码才会暂停
    static constexpr uint64_t MAX_AUDIO_PACKET_QUEUE_SIZE = 200; // 最大音频帧队列数量
    //static constexpr uint64_t MAX_AUDIO_FRAME_QUEUE_SIZE = 200; // 最大音频帧队列数量
    // 低于下列值开始继续读取新的帧，取出新的值后<下列值开始通知读取线程
    static constexpr uint64_t MIN_AUDIO_PACKET_QUEUE_SIZE = 100; // 最小音频帧队列数量
    // 统一音频转换后用于播放的格式
    static constexpr AVSampleFormat AUDIO_OUTPUT_FORMAT = AVSampleFormat::AV_SAMPLE_FMT_S16;
    using AudioSampleFormatType = int16_t;
    // 用于播放的格式每单位样本大小
    static constexpr int AUDIO_OUTPUT_FORMAT_BYTES_PER_SAMPLE = sizeof(AudioSampleFormatType); // AV_SAMPLE_FMT_S16 每个样本2字节

    static constexpr StreamTypes STREAM_TYPES = StreamType::STAudio;


public:
    typedef std::any UserDataType;

    struct SampleFrameContext {
        AVFormatContext* formatCtx{ nullptr }; // 所属格式上下文
        AVCodecContext* codecCtx{ nullptr }; // 所属编解码上下文
        StreamIndexType streamIndex{ -1 }; // 所属流索引
        std::span<uint8_t> data;// { nullptr }; // 指向音频数据缓冲区的指针
        int dataSize{ 0 }; // 数据大小，单位字节
        unsigned int nFrames{ 0 }; // 音频帧数
        int sampleRate{ 0 }; // 采样率
        int numberOfChannels{ 0 }; // 通道数
        uint64_t pts{ 0 }; // Presentation Time Stamp
        AVRational timeBase{ 1, AV_TIME_BASE }; // 时间基，也可以从formatCtx->streams[streamIndex]->time_base获取
        double frameTime{ 0.0 }; // 帧对应的时间，单位s
        double streamTime{ 0.0 }; // 流时间，单位s
        double volume{ 1.0 }; // 音量，范围0.0 ~ 1.0
        bool isMute{ false }; // 静音
    };

    struct DecodedFrameContext {
        AVFormatContext* formatCtx{ nullptr }; // 所属格式上下文
        AVCodecContext* codecCtx{ nullptr }; // 所属编解码上下文
        StreamIndexType streamIndex{ -1 }; // 所属流索引

        AVFrame* frame{ nullptr }; // 当前帧
    };

    using AudioClockSyncFunction = std::function<bool(const AtomicDouble& audioClock, const AtomicBool& isClockStable, const double& audioRealtimeClock, int64_t& sleepTime)>;

    // @param outFilterGraphs 输出参数，返回要启用的滤波图列表
    // @param frameContext 当前帧的上下文信息，包含了当前帧的各种信息以及上一帧的格式转换选项
    // @param userData 用户数据
    // 返回值true表示启用滤波图，false表示不启用
    using AudioFrameFilterGraphCreator = std::function<bool(std::vector<IFrameFilterGraph*>& outFilterGraphs, bool& shouldResetSwrContext, const DecodedFrameContext& frameContext, UserDataType userData)>;

    struct AudioPlayOptions {
        StreamIndexSelector streamIndexSelector{ nullptr };
        AudioClockSyncFunction clockSyncFunction{ nullptr }; // Reserved for future use
        AudioFrameFilterGraphCreator frameFilterGraphCreator{ nullptr };
        UserDataType frameFilterGraphCreatorUserData{ UserDataType{} };

        void mergeFrom(const AudioPlayOptions& other) {
            if (other.streamIndexSelector) streamIndexSelector = other.streamIndexSelector;
            if (other.clockSyncFunction) clockSyncFunction = other.clockSyncFunction;
            if (other.frameFilterGraphCreator) this->frameFilterGraphCreator = other.frameFilterGraphCreator;
            if (other.frameFilterGraphCreatorUserData.has_value()) this->frameFilterGraphCreatorUserData = other.frameFilterGraphCreatorUserData;
        }
    };

    class AudioRenderEvent : public IMediaEvent {
        SampleFrameContext* frameCtx{ nullptr };
    public:
        AudioRenderEvent(SampleFrameContext* frameCtx)
            : IMediaEvent(MediaEventType::Render, StreamType::STAudio), frameCtx(frameCtx) {}
        virtual SampleFrameContext* frameContext() const {
            return frameCtx;
        }
        virtual UniquePtrD<IMediaEvent> clone() const override {
            return std::make_unique<AudioRenderEvent>(*this);
        }
    };


private:
    struct AudioStreamInfo {
        std::vector<uint8_t> dataBytes{}; // 如果是uint16_t格式的数据，nFrames表示播放缓冲区大小（在音频播放回调中使用），则dataBytes.size()应该是numberOfAudioOutputChannels * sizeof(uint16_t) * nFrames
        //std::span<uint8_t> dataBytes{}; // 如果是uint16_t格式的数据，nFrames表示播放缓冲区大小（在音频播放回调中使用），则dataBytes.size()应该是numberOfAudioOutputChannels * sizeof(uint16_t) * nFrames
        //std::vector<uint8_t> dataBytes{}; // 如果是uint16_t格式的数据，则dataBytes.size()应该是numberOfAudioOutputChannels * sizeof(uint16_t)
        uint64_t pts{ 0 }; // Presentation Time Stamp
        AVRational timeBase{ 1, AV_TIME_BASE }; // 时间基
        double frameTime{ 0.0 }; // 帧对应的时间，单位s
    };

    struct AudioPlaybackStateVariables { // 用于存储播放状态相关的数据
        // 用于回调时获取所属播放器对象
        AudioPlayer* owner{ nullptr };
        AudioPlaybackStateVariables(AudioPlayer* o) : owner(o) {}

        // 线程等待对象管理器
        ThreadStateManager threadStateManager;

        // 音频输出与设备
        UniquePtrD<AudioAdapter> audioDevice;
        AtomicInt numberOfAudioOutputChannels = DEFAULT_NUMBER_CHANNELS_AUDIO_OUTPUT;
        Atomic<unsigned int> audioOutputStreamBufferSize = DEFAULT_AUDIO_OUTPUT_STREAM_BUFFER_SIZE;
        //Mutex mtxStreamQueue; // 用于保证在写入一段的时候不被读取
        ConcurrentQueue<AudioStreamInfo> streamQueue;
        // 每次渲染音频修改的上下文
        //FrameContext renderFrameContext;

        // 解复用器
        StreamType demuxerStreamType{ STREAM_TYPES };
        Atomic<AbstractDemuxer*> demuxer{ nullptr };
        AVFormatContext* formatCtx{ nullptr };
        StreamIndexType streamIndex{ -1 };
        ConcurrentQueue<AVPacket*>* packetQueue{ nullptr };

        UniquePtr<AVCodecContext> codecCtx{ nullptr, constDeleterAVCodecContext };
        // 音频帧滤镜
        StreamType filterGraphStreamType{ STREAM_TYPES };

        // 音量与静音
        AtomicDouble volume{ 1.0 }; // 范围0.0 ~ 1.0
        AtomicBool isMute{ false };

        //// 播放速率（倍速）
        //AtomicDouble speed{ 1.0 };

        //// 均衡器参数
        //std::vector<IFFmpegFrameAudioEqualizerFilter::BandInfo> equalizerBandGains{ FFmpegFrameAudio10BandEqualizerFilter::defaultBandGains() };
        //AtomicBool isEqualizerEnabled{ false };

        // 时钟
        AtomicDouble audioClock{ 0.0 }; // 单位s
        double realtimeClock{ 0.0 };

        // 文件
        std::string filePath;
        AudioPlayOptions playOptions;
        // 请求任务队列
        RequestTaskQueueHandler* requestQueueHandler{ nullptr };

        void clearPktAndStreamQueues() {
            demuxer.load()->flushPacketQueue(demuxerStreamType);
            //std::unique_lock lockMtxStreamQueue(mtxStreamQueue); // 记得加锁
            //Queue<AudioStreamInfo> streamQueueNew;
            //streamQueue.swap(streamQueueNew);
            ConcurrentQueue<AudioStreamInfo> streamQueueNew;
            streamQueue.swap(streamQueueNew);
        }
        // 重置所有变量，除了playOptions和filePath
        void reset() {
            owner->stopAndCloseOutputAudioStream();

            // 清空队列
            clearPktAndStreamQueues();
            // 重置其他变量
            streamIndex = -1;
            formatCtx = nullptr;
            codecCtx.reset();
            audioClock.store(0.0);
            realtimeClock = 0.0;
            // 清空请求任务队列
            requestQueueHandler = nullptr;
            //requestQueueHandler.reset();
            // 清空线程等待对象
            threadStateManager.reset();
        }

    };

    // 日志记录器
    const std::string loggerName{ "AudioPlayer" };
    DefinePlayerLoggerSinks(loggerSinks, loggerName);
    Logger logger{ loggerName, loggerSinks };

    // 播放器状态
    Mutex mtxSinglePlayback;
    Atomic<PlayerState> playerState{ PlayerState::Stopped };
    AtomicWaitObject<bool> waitStopped{ false }; // true表示已停止，false表示未停止
    AudioPlaybackStateVariables playbackStateVariables{ this };
    ComponentWorkMode demuxerMode{ ComponentWorkMode::Internal };
    SharedPtr<SingleDemuxer> internalDemuxer{ std::make_shared<SingleDemuxer>(loggerName, playbackStateVariables.demuxerStreamType) };
    SharedPtr<UnifiedDemuxer> externalDemuxer{ nullptr };
    ComponentWorkMode requestTaskQueueHandlerMode{ ComponentWorkMode::Internal };
    SharedPtr<RequestTaskQueueHandler> internalRequestTaskQueueHandler{ std::make_shared<RequestTaskQueueHandler>(this) };
    RequestTaskQueueHandler* externalRequestTaskQueueHandler{ nullptr };

    inline bool shouldStop() const {
        return playerState == PlayerState::Stopping || playerState == PlayerState::Stopped;
    }

public:
    AudioPlayer() : AbstractPlayer(logger) {}
    ~AudioPlayer() {
        stop();
    }
    // options如果非空则覆盖之前的选项
    bool play(const std::string& filePath, const AudioPlayOptions& options) {
        std::unique_lock lockMtxSinglePlayback(mtxSinglePlayback);
        if (!isStopped())
            return false;
        if (!trySetPlayerState(PlayerState::Preparing))
            return false;
        setFilePath(filePath);
        playbackStateVariables.playOptions.mergeFrom(options);
        if (!prepareBeforePlayback())
            return false;
        lockMtxSinglePlayback.unlock();
        bool rst = playAudioFile();
        lockMtxSinglePlayback.lock();
        cleanupAfterPlayback();
        return rst;
    }
    virtual bool play() override {
        std::unique_lock lockMtxSinglePlayback(mtxSinglePlayback);
        if (!isStopped())
            return false;
        if (!trySetPlayerState(PlayerState::Preparing))
            return false;
        if (playbackStateVariables.filePath.empty()) // 打开了文件才能播放
            return false;
        if (!prepareBeforePlayback())
            return false;
        lockMtxSinglePlayback.unlock();
        bool rst = playAudioFile();
        lockMtxSinglePlayback.lock();
        cleanupAfterPlayback();
        return rst;
    }

    virtual void resume() override { // 用于从暂停/停止状态恢复播放
        if (isPaused())
        {
            // 恢复播放
            setPlayerState(PlayerState::Playing);
            // 唤醒所有线程
            playbackStateVariables.threadStateManager.wakeUpAll();
        }
    }
    virtual void pause() override {
        if (playerState == PlayerState::Playing)
        {
            setPlayerState(PlayerState::Paused);
            // 唤醒所有线程
            playbackStateVariables.threadStateManager.wakeUpAll();
        }
    }
    virtual void notifyStop() override {
        std::unique_lock lockMtxSinglePlayback(mtxSinglePlayback);
        if (playerState != PlayerState::Stopping && !isStopped())
        {
            setPlayerState(PlayerState::Stopping);
            lockMtxSinglePlayback.unlock();
            // 唤醒解复用器线程
            if (demuxerMode == ComponentWorkMode::Internal)
            {
                playbackStateVariables.demuxer.load()->stop();
                //playbackStateVariables.demuxer->stop(); // 这里不调用stop，因为start的时候传递了stopCondition用于决定何时退出，所以这里只需要唤醒即可
                //playbackStateVariables.demuxer->wakeUp();
            }
            // 唤醒所有线程
            playbackStateVariables.threadStateManager.wakeUpAll();
            // 唤醒请求任务处理线程
            if (requestTaskQueueHandlerMode == ComponentWorkMode::Internal)
                playbackStateVariables.requestQueueHandler->stop();
        }
    }
    virtual void stop() override {
        if (playerState != PlayerState::Stopping && !isStopped())
        {
            notifyStop();
            waitStopped.wait(true); // 等待停止完成
        }
    }

    //void mute() { setMute(true); }
    //void unmute() { setMute(false); }
    //virtual void setMute(bool state) override {
    //    playbackStateVariables.isMute.store(state);
    //}
    //virtual bool getMute() const override {
    //    return playbackStateVariables.isMute;
    //}
    //virtual void setVolume(double volume) override {
    //    playbackStateVariables.volume.store(volume);
    //}
    //virtual double getVolume() const override {
    //    return playbackStateVariables.volume.load();
    //}
    //virtual void setSpeed(double speed) override {
    //    playbackStateVariables.speed.store(speed);
    //}
    //virtual double getSpeed() const override {
    //    return playbackStateVariables.speed.load();
    //}
    //virtual void setEqualizerState(bool enabled) {
    //    playbackStateVariables.isEqualizerEnabled.store(enabled);
    //}
    //virtual bool getEqualizerState() const {
    //    return playbackStateVariables.isEqualizerEnabled.load();
    //}
    //virtual void setEqualizerGains(const std::vector<IFFmpegFrameAudioEqualizerFilter::BandInfo>& gains) {
    //    playbackStateVariables.equalizerBandGains = gains;
    //}
    //virtual void setEqualizerGain(uint64_t bandIndex, IFFmpegFrameAudioEqualizerFilter::BandInfo gain) {
    //    if (bandIndex >= playbackStateVariables.equalizerBandGains.size())
    //        return;
    //    playbackStateVariables.equalizerBandGains[bandIndex] = gain;
    //}
    //virtual std::vector<IFFmpegFrameAudioEqualizerFilter::BandInfo> getEqualizerGains() const {
    //    return playbackStateVariables.equalizerBandGains;
    //}

    // \param streamIndex -1表示使用AV_TIME_BASE计算，否则使用streamIndex指定的流的time_base
    virtual void notifySeek(uint64_t pts, StreamIndexType streamIndex = -1) override {
        if (shouldCommitRequest())
        {
            // 提交seek任务
            auto seekHandler = std::bind(&AudioPlayer::requestTaskHandlerSeek, this, std::placeholders::_1, std::placeholders::_2);
            // 阻塞三个线程（即所有与读包解包相关的线程）
            auto&& blockThreadIds = { ThreadIdentifier::Demuxer, ThreadIdentifier::Decoder, ThreadIdentifier::Renderer };
            playbackStateVariables.requestQueueHandler->push(RequestTaskType::Seek, blockThreadIds, new MediaSeekEvent{ STREAM_TYPES, pts, streamIndex }, seekHandler);
        }
    }
    virtual void seek(uint64_t pts, StreamIndexType streamIndex = -1) override {
        notifySeek(pts, streamIndex);
    }

    virtual bool isPlaying() const override {
        return playerState == PlayerState::Playing;
    }
    virtual bool isPaused() const override {
        return playerState == PlayerState::Paused;
    }
    virtual bool isStopped() const override {
        return playerState == PlayerState::Stopped;
    }
    virtual PlayerState getPlayerState() const override {
        return playerState.load();
    }


    virtual void setFilePath(const std::string& filePath) override {
        this->playbackStateVariables.filePath = filePath;
    }

    virtual std::string getFilePath() const override {
        return playbackStateVariables.filePath;
    }
    
    virtual void setDemuxerMode(ComponentWorkMode mode) override {
        this->demuxerMode = mode;
    }

    virtual ComponentWorkMode getDemuxerMode() const override {
        return this->demuxerMode;
    }
    virtual void setExternalDemuxer(const SharedPtr<UnifiedDemuxer>& demuxer) override {
        if (this->externalDemuxer)
        {
            this->externalDemuxer->setPacketEnqueueCallback(playbackStateVariables.demuxerStreamType, nullptr);
            this->externalDemuxer->removeStreamType(playbackStateVariables.demuxerStreamType);
        }
        demuxer->addStreamType(playbackStateVariables.demuxerStreamType);
        demuxer->setPacketEnqueueCallback(playbackStateVariables.demuxerStreamType, std::bind(&AudioPlayer::packetEnqueueCallback, this));
        this->externalDemuxer = demuxer;
    }

    virtual void setRequestTaskQueueHandlerMode(ComponentWorkMode mode) override {
        this->requestTaskQueueHandlerMode = mode;
    }

    virtual ComponentWorkMode getRequestTaskQueueHandlerMode() const override {
        return this->requestTaskQueueHandlerMode;
    }
    virtual void setExternalRequestTaskQueueHandler(const SharedPtr<RequestTaskQueueHandler>& handler) override {
        if (this->externalRequestTaskQueueHandler)
        {
            handler->stop();
            handler->removeThreadStateHandlersContext(STREAM_TYPES);
        }
        this->externalRequestTaskQueueHandler = handler.get();
        addThreadStateHandlersContext(handler.get());
    }

    void setStreamIndexSelector(const StreamIndexSelector& selector) {
        this->playbackStateVariables.playOptions.streamIndexSelector = selector;
    }

    void setClockSyncFunction(const AudioClockSyncFunction& func) {
        this->playbackStateVariables.playOptions.clockSyncFunction = func;
    }

protected:
    virtual bool event(IMediaEvent* e) override {
        if (e->type() == MediaEventType::Render)
        {
            AudioRenderEvent* re = static_cast<AudioRenderEvent*>(e);
            renderEvent(re);
        }
        return AbstractPlayer::event(e);
    }
    // 渲染事件处理函数，子类可重写以实现自定义渲染逻辑
    virtual void renderEvent(AudioRenderEvent* e) {

    }
    void clearBuffers() {
        // 清空队列
        playbackStateVariables.clearPktAndStreamQueues();
        // 刷新解码器buffer
        if (playbackStateVariables.codecCtx)
            avcodec_flush_buffers(playbackStateVariables.codecCtx.get());
    }
    int64_t clockSync(uint64_t pts, StreamIndexType streamIndex, bool isStable) {
        if (streamIndex >= 0 && streamIndex < playbackStateVariables.formatCtx->nb_streams)
            playbackStateVariables.audioClock.store(pts * av_q2d(playbackStateVariables.formatCtx->streams[streamIndex]->time_base));
        else
            playbackStateVariables.audioClock.store(pts / (double)AV_TIME_BASE);

        if (playbackStateVariables.playOptions.clockSyncFunction)
        {
            int64_t sleepTime = 0;
            //playbackStateVariables.isAudioClockStable.store(isStable);
            bool ret = playbackStateVariables.playOptions.clockSyncFunction(playbackStateVariables.audioClock, isStable, playbackStateVariables.realtimeClock, sleepTime);
            if (!ret) return 0; // 返回false表示不需要同步
            return sleepTime;
        }
        return 0;
    }


private:
    void setPlayerState(PlayerState state) {
        PlayerState oldState = playerState.load();
        playerState.set(state, oldState);
        notifyPlaybackStateChangeHandler(state, oldState);
    }
    bool trySetPlayerState(PlayerState state) {
        PlayerState oldState = playerState.load();
        bool r = playerState.trySet(state, oldState);
        notifyPlaybackStateChangeHandler(state, oldState);
        return r;
    }

    void notifyPlaybackStateChangeHandler(PlayerState newState, PlayerState oldState) {
        auto e = MediaPlaybackStateChangeEvent{ STREAM_TYPES, newState, oldState };
        event(&e);
    }

    void resetPlayer() {
        playbackStateVariables.reset();
        setPlayerState(PlayerState::Stopped);
    }


    bool shouldCommitRequest() {
        return !isStopped() && playerState != PlayerState::Stopping
            && requestTaskQueueHandlerMode == ComponentWorkMode::Internal;
    }

    void addThreadStateHandlersContext(RequestTaskQueueHandler* handler) {
        struct ThreadBlockerAwakenerContext {
            std::vector<ThreadStateManager::ThreadStateController> waitObjs{};
            bool demuxerPaused{ false };
        } threadBlockerAwakenerContext;
        handler->addThreadStateHandlersContext(STREAM_TYPES,
            [&](const RequestTaskItem& taskItem, std::any& userData) {
                //std::vector<ThreadStateManager::ThreadStateController> waitObjs;
                auto& ctx = std::any_cast<ThreadBlockerAwakenerContext&>(userData);
                ctx.waitObjs.clear();
                threadBlocker(logger, taskItem.blockTargetThreadIds, playbackStateVariables.threadStateManager, playbackStateVariables.demuxer.load(), ctx.waitObjs, ctx.demuxerPaused);
            },
            [&](const RequestTaskItem& taskItem, std::any& userData) {
                auto& ctx = std::any_cast<ThreadBlockerAwakenerContext&>(userData);
                threadAwakener(ctx.waitObjs, playbackStateVariables.demuxer.load(), ctx.demuxerPaused);
            },
            threadBlockerAwakenerContext);
    }

    bool prepareBeforePlayback();
    bool playAudioFile();
    // 播放完成后清理播放器
    void cleanupAfterPlayback();

    void audioOutputStreamErrorCallback(AudioAdapter::AudioErrorType type, const std::string& errorText);
    
    // 打开------------------------------------------------------
    // frameBufferSize = 0 时使用默认缓冲区大小（1024），或者 *frameBufferSize = 0 时，即最小允许值（根据RtAudio文档），该参数将返回实际使用的缓冲区大小
    bool openOutputAudioStream(int sampleRate, AVChannelLayout channelLayout, AVSampleFormat sampleFmt, unsigned int* frameBufferSize = 0);
    bool startOutputAudioStream();
    bool openAndStartOutputAudioStream(int sampleRate, AVChannelLayout channelLayout, AVSampleFormat sampleFmt, unsigned int* frameBufferSize = 0);
    // ----------------------------------------------------------
    // 关闭------------------------------------------------------
    // 如果流未打开则会有一个Warning发送到ErrorCallback
    void closeOutputAudioStream();
    void stopOutputAudioStream();
    void stopAndCloseOutputAudioStream();
    // -----------------------------------------------------------


    //// 打开文件
    //bool openInput() {
    //    return MediaDecodeUtils::openFile(&logger, playbackStateVariables.formatCtx, playbackStateVariables.filePath);
    //}

    //// 查找流信息
    //bool findStreamInfo() {
    //    return MediaDecodeUtils::findStreamInfo(&logger, playbackStateVariables.formatCtx);
    //}

    // 查找并选择音频流
    //bool findAndSelectAudioStream();

    // 查找并打开解码器
    bool findAndOpenAudioDecoder();

    // 从文件中读包
    //void readPackets();

    // （如果包队列少于或等于某个最小值）通知解码器有新包到达
    void packetEnqueueCallback() { // 该函数内不能轻易使用playbackStateVariables中的packetQueue,formatCtx,streamIndex,demuxer
        AbstractDemuxer* demuxer = nullptr;
        if (demuxerMode == ComponentWorkMode::External)
            demuxer = externalDemuxer.get(); // 此时可用playbackStateVariablesz中的demuxer
        else
            demuxer = playbackStateVariables.demuxer.load(); // 此时可用playbackStateVariablesz中的demuxer
        if (!demuxer) return;
        // 解复用器每次成功入队一个包后调用该回调函数，通知解码器继续解码
        if (getQueueSize(*demuxer->getPacketQueue(playbackStateVariables.demuxerStreamType)) <= demuxer->getMinPacketQueueSize(playbackStateVariables.demuxerStreamType))
            playbackStateVariables.threadStateManager.wakeUpById(ThreadIdentifier::Decoder);
    }

    // 音频帧调整
    
    // 将包转化为音频输出流
    void packet2AudioStreams();

    // 播放音频，预留用于同步输出，异步输出将使用renderAudioAsyncCallback
    void renderAudio();

    // 音频输出回调，用于异步输出
    // 内部处理音频的回调函数
    // \param userData 保留参数，暂时为指向当前VideoPlayer实例的指针
    AudioAdapter::AudioCallbackResult renderAudioAsyncCallback(void*& outputBuffer, void*& inputBuffer, unsigned int& nFrames, double& streamTime, AudioAdapter::AudioStreamStatuses& status, AudioAdapter::RawArgsType& rawArgs, AudioAdapter::UserDataType& userData);

    void requestTaskHandlerSeek(MediaRequestHandleEvent* e, std::any userData);
};


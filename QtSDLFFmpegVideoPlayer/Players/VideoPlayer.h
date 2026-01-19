#pragma once
#include "PlayerPredefine.h"

class VideoPlayer : public PlayerInterface, private ConcurrentQueueOps
{
public:
    // 用于视频帧队列
    static constexpr int MAX_VIDEO_FRAME_QUEUE_SIZE = 50; // n frames
    static constexpr int MIN_VIDEO_FRAME_QUEUE_SIZE = 20; // n frames
    // 用于ffmpeg视频解码和播放
    // 下面两个常量需同时满足，解码才会暂停
    static constexpr size_t MAX_VIDEO_PACKET_QUEUE_SIZE = 200; // 最大视频帧队列数量
    //static constexpr size_t MAX_AUDIO_FRAME_QUEUE_SIZE = 200; // 最大音频帧队列数量
    // 低于下列值开始继续读取新的帧，取出新的值后<下列值开始通知读取线程
    static constexpr size_t MIN_VIDEO_PACKET_QUEUE_SIZE = 100; // 最小视频帧队列数量

    static constexpr StreamTypes STREAM_TYPES = StreamType::STVideo;

public:// 这个区域用于定义公共类型
    typedef std::any UserDataType;
    
    struct VideoFrameSwitchOptions {
        bool enabled{ false };
        AVPixelFormat format{ AV_PIX_FMT_NONE };
        SizeI size;
        bool operator==(const VideoFrameSwitchOptions& other) const {
            return enabled == other.enabled
                && format == other.format
                && size == other.size;
        }
        bool operator!=(const VideoFrameSwitchOptions& other) const {
            return !(*this == other);
        }
    };
    struct FrameContext {
        // 总基本信息

        AVFormatContext* formatCtx{ nullptr };
        AVCodecContext* codecCtx{ nullptr };
        StreamIndexType streamIndex{ -1 };
        // 每一帧的信息

        AVFrame* hwRawFrame{ nullptr }; // 硬件解码帧，可能为nullptr
        AVFrame* swRawFrame{ nullptr }; // 软件解码帧，可能是从硬件帧转换来的（isHardwareDecoded为true），也可能是直接软件解码得到的
        AVFrame* newFormatFrame{ nullptr }; // 转换格式后的帧（如果转换启用），否则为nullptr，也可用于判断是否转换成功
        VideoFrameSwitchOptions frameSwitchOptions;
        bool isHardwareDecoded{ false }; // 是否为硬件解码
        AVPixelFormat hwFramePixelFormat{ AV_PIX_FMT_NONE }; // 硬件解码帧的像素格式，仅在isHardwareDecoded为true时有效，否则为AV_PIX_FMT_NONE
    };
    // FrameContext中的frameSwitchOptions成员表示上一帧的格式转换选项
    using VideoFrameSwitchOptionsCallback = std::function<void(VideoFrameSwitchOptions& to, const FrameContext& frameCtx, UserDataType userData)>;
    // FrameContext中的frameSwitchOptions成员表示当前帧的格式转换选项
    using VideoRenderFunction = std::function<void(const FrameContext& frameContext, UserDataType userData)>;

    // isClockStable 用于判断videoClock是否准确
    // sleepTime < 0 表示可能需要丢帧以追赶时钟
    // sleepTime > 0 表示可能需要睡眠以等待时钟
    // 返回值true && (sleepTime != 0)表示需要睡眠/丢帧，false || (sleepTime == 0)表示不需要
    using VideoClockSyncFunction = std::function<bool(const AtomicDouble& videoClock, const AtomicBool& isClockStable, const double& videoRealtimeClock, int64_t& sleepTime)>;

    enum class DecodeType {
        Unset = 0,
        Software = 1,
        Hardware = 2,
    };

    struct VideoPlayOptions {
        StreamIndexSelector streamIndexSelector{ nullptr };
        VideoClockSyncFunction clockSyncFunction{ nullptr };
        VideoRenderFunction renderer{ nullptr };
        UserDataType rendererUserData{ UserDataType{} };
        VideoFrameSwitchOptionsCallback frameSwitchOptionsCallback{ nullptr };
        UserDataType frameSwitchOptionsCallbackUserData{ UserDataType{} };
        DecodeType decodeType{ DecodeType::Software };

        void mergeFrom(const VideoPlayOptions& other) {
            if (other.streamIndexSelector) this->streamIndexSelector = other.streamIndexSelector;
            if (other.clockSyncFunction) this->clockSyncFunction = other.clockSyncFunction;
            if (other.renderer) this->renderer = other.renderer;
            if (other.rendererUserData.has_value()) this->rendererUserData = other.rendererUserData;
            if (other.frameSwitchOptionsCallback) this->frameSwitchOptionsCallback = other.frameSwitchOptionsCallback;
            if (other.frameSwitchOptionsCallbackUserData.has_value()) this->frameSwitchOptionsCallbackUserData = other.frameSwitchOptionsCallbackUserData;
            if (other.decodeType != DecodeType::Unset) this->decodeType = other.decodeType;
        }
    };

    class VideoRenderEvent : public MediaEvent {
        FrameContext* frameCtx{ nullptr };
    public:
        VideoRenderEvent(FrameContext* frameCtx)
            : MediaEvent(MediaEventType::Render, StreamType::STVideo), frameCtx(frameCtx) {}
        virtual FrameContext* frameContext() const {
            return frameCtx;
        }
        virtual UniquePtrD<MediaEvent> clone() const override {
            return std::make_unique<VideoRenderEvent>(*this);
        }
    };

private:
    struct VideoPlaybackStateVariables { // 用于存储播放状态相关的数据
        // 用于回调时获取所属播放器对象
        VideoPlayer* owner{ nullptr };
        VideoPlaybackStateVariables(VideoPlayer* o) : owner(o) {}
        // 线程等待对象管理器
        ThreadStateManager threadStateManager;
        // 解复用器
        StreamType demuxerStreamType{ STREAM_TYPES };
        Atomic<DemuxerInterface*> demuxer{ nullptr };
        AVFormatContext* formatCtx{ nullptr };
        StreamIndexType streamIndex{ -1 };
        ConcurrentQueue<AVPacket*>* packetQueue{ nullptr };

        ConcurrentQueue<AVFrame*> frameQueue;

        UniquePtr<AVCodecContext> codecCtx{ nullptr, constDeleterAVCodecContext };
        AVHWDeviceType hwDeviceType{ AV_HWDEVICE_TYPE_NONE };
        AVPixelFormat hwPixelFormat{ AV_PIX_FMT_NONE };
        // 时钟
        AtomicDouble videoClock{ 0.0 }; // 单位s
        AtomicBool isVideoClockStable{ false }; // 时钟是否稳定
        // 系统时钟
        double realtimeClock{ 0.0 };

        // 清空队列
        void clearPktAndFrameQueues() {
            demuxer.load()->flushPacketQueue(demuxerStreamType);
            AVFrame* frame = nullptr;
            while (tryDequeue(frameQueue, frame))
                av_frame_free(&frame);
        }
        // 重置所有变量，除了playOptions和filePath
        void reset() {
            // 清空队列
            clearPktAndFrameQueues();
            // 重置其他变量
            streamIndex = -1;
            formatCtx = nullptr;
            codecCtx.reset();
            hwDeviceType = AV_HWDEVICE_TYPE_NONE;
            hwPixelFormat = AV_PIX_FMT_NONE;
            videoClock.store(0.0);
            realtimeClock = 0.0;

            // 清空请求任务队列
            requestQueueHandler = nullptr;
            //requestQueueHandler.reset();
            threadStateManager.reset();
        }

        // 文件
        std::string filePath;
        VideoPlayOptions playOptions;
        // 请求任务队列
        RequestTaskQueueHandler* requestQueueHandler{ nullptr };
    };

    const std::string loggerName{ "VideoPlayer" };
    DefinePlayerLoggerSinks(loggerSinks, loggerName);
    Logger logger{ loggerName, loggerSinks };

    // 播放器状态
    Mutex mtxSinglePlayback;
    AtomicStateMachine<PlayerState> playerState{ PlayerState::Stopped };
    AtomicWaitObject<bool> waitStopped{ false }; // true表示已停止，false表示未停止
    VideoPlaybackStateVariables playbackStateVariables{ this };
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
    VideoPlayer() : PlayerInterface(logger) {
        //logger.setLevel(LogLevel::trace);
    }
    ~VideoPlayer() {
        stop();
    }
    // options会合并到已有的playOptions中，decodeType不为Unset时会被覆盖，其他选项如果非空则覆盖
    bool play(const std::string& filePath, VideoPlayOptions options) {
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
        bool rst = playVideoFile();
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
        bool rst = playVideoFile();
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
        if (isPlaying())
        {
            setPlayerState(PlayerState::Pausing);
            // 唤醒所有线程
            playbackStateVariables.threadStateManager.wakeUpAll();
        }
    }
    virtual void notifyStop() override {
        std::unique_lock lockSinglePlaybackMtx(mtxSinglePlayback);
        if (playerState != PlayerState::Stopping && !isStopped())
        {
            setPlayerState(PlayerState::Stopping);
            lockSinglePlaybackMtx.unlock(); // 释放锁，以便停止过程中调用的其他函数能获取锁
            // 唤醒解复用器
            if (demuxerMode == ComponentWorkMode::Internal)
            {
                playbackStateVariables.demuxer.load()->stop();
                //playbackStateVariables.demuxer->stop(); // 这里不调用stop，因为start的时候传递了stopCondition用于决定何时退出，所以这里只需要唤醒即可
                //playbackStateVariables.demuxer->wakeUp();
            }
            // 唤醒所有线程
            playbackStateVariables.threadStateManager.wakeUpAll();
            // 唤醒请求任务等待，以便处理停止请求，退出处理线程
            if (requestTaskQueueHandlerMode == ComponentWorkMode::Internal)
                playbackStateVariables.requestQueueHandler->stop();
        }
    }
    virtual void stop() override {
        if (playerState != PlayerState::Stopping && !isStopped())
        {
            notifyStop();
            waitStopped.wait(true); // 等待停止完成
            //resetPlayer(); // 播放结束会自动重置播放器
        }
    }

    virtual void setMute(bool state) override {

    }
    // 视频播放器不支持音频播放，因此始终返回true表示静音状态
    virtual bool getMute() const override {
        return true;
    }
    virtual void setVolume(double volume) override {

    }
    // 视频播放器不支持音频播放，因此始终返回0.0表示无音量
    virtual double getVolume() const override {
        return 0.0;
    }

    // \param streamIndex -1表示使用AV_TIME_BASE计算，否则使用streamIndex指定的流的time_base
    virtual void notifySeek(uint64_t pts, StreamIndexType streamIndex = -1) override {
        if (shouldCommitRequest())
        {
            // 提交seek任务
            auto seekHandler = std::bind(&VideoPlayer::requestTaskHandlerSeek, this, std::placeholders::_1, std::placeholders::_2);
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
        demuxer->setPacketEnqueueCallback(playbackStateVariables.demuxerStreamType, std::bind(&VideoPlayer::packetEnqueueCallback, this));
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

    void setClockSyncFunction(const VideoClockSyncFunction& func) {
        this->playbackStateVariables.playOptions.clockSyncFunction = func;
    }

    void setRenderer(const VideoRenderFunction& renderer, UserDataType userData) {
        this->playbackStateVariables.playOptions.renderer = renderer;
        this->playbackStateVariables.playOptions.rendererUserData = userData;
    }

    void enableHardwareDecoding(bool b) {
        if (b)
            this->playbackStateVariables.playOptions.decodeType = DecodeType::Hardware;
        else
            this->playbackStateVariables.playOptions.decodeType = DecodeType::Software;
    }

    void setFrameSwitchOptionsCallback(VideoFrameSwitchOptionsCallback callback, UserDataType userData) {
        this->playbackStateVariables.playOptions.frameSwitchOptionsCallback = callback;
        this->playbackStateVariables.playOptions.frameSwitchOptionsCallbackUserData = userData;
    }

protected:
    virtual bool event(MediaEvent* e) override {
        if (e->type() == MediaEventType::Render)
        {
            VideoRenderEvent* re = static_cast<VideoRenderEvent*>(e);
            renderEvent(re);
        }
        return PlayerInterface::event(e);
    }
    // 渲染事件处理函数（接口），子类可重写以实现自定义渲染逻辑
    virtual void renderEvent(VideoRenderEvent* e) {

    }

    void clearBuffers() {
        // 清空队列
        playbackStateVariables.clearPktAndFrameQueues();
        // 刷新解码器buffer
        if (playbackStateVariables.codecCtx)
            avcodec_flush_buffers(playbackStateVariables.codecCtx.get());
    }

    int64_t clockSync(size_t pts, StreamIndexType streamIndex, bool isStable) {
        if (streamIndex >= 0 && streamIndex < playbackStateVariables.formatCtx->nb_streams)
            playbackStateVariables.videoClock.store(pts * av_q2d(playbackStateVariables.formatCtx->streams[streamIndex]->time_base));
        else
            playbackStateVariables.videoClock.store(pts / (double)AV_TIME_BASE);

        if (playbackStateVariables.playOptions.clockSyncFunction)
        {
            int64_t sleepTime = 0;
            playbackStateVariables.isVideoClockStable.store(isStable);
            bool ret = playbackStateVariables.playOptions.clockSyncFunction(playbackStateVariables.videoClock, playbackStateVariables.isVideoClockStable, playbackStateVariables.realtimeClock, sleepTime);
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

    bool getIsHardwareDecodingEnabled() const {
        return playbackStateVariables.playOptions.decodeType == DecodeType::Hardware;
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
    bool playVideoFile();
    void cleanupAfterPlayback();

    //// 打开文件
    //bool openInput() {
    //    return MediaDecodeUtils::openFile(&logger, playbackStateVariables.formatCtx, playbackStateVariables.filePath);
    //}
    //
    //// 查找流信息
    //bool findStreamInfo() {
    //    return MediaDecodeUtils::findStreamInfo(&logger, playbackStateVariables.formatCtx.get());
    //}

    // 查找并选择视频流
    //bool findAndSelectVideoStream();

    // 查找并打开解码器
    bool findAndOpenVideoDecoder();

    // 从文件中读包，不依赖于解码器，因此不需要事先打开解码器
    //void readPackets();
    
    // （如果包队列少于或等于某个最小值）通知解码器有新包到达
    void packetEnqueueCallback() { // 该函数内不能轻易使用playbackStateVariables中的packetQueue,formatCtx,streamIndex
        if (!playbackStateVariables.demuxer.load()) return;
        // 解复用器每次成功入队一个包后调用该回调函数，通知解码器继续解码
        if (getQueueSize(*playbackStateVariables.demuxer.load()->getPacketQueue(playbackStateVariables.demuxerStreamType)) <= playbackStateVariables.demuxer.load()->getMinPacketQueueSize(playbackStateVariables.demuxerStreamType))
            playbackStateVariables.threadStateManager.wakeUpById(ThreadIdentifier::Decoder);
    }

    void packet2VideoFrames();

    void renderVideo();

    void requestTaskHandlerSeek(MediaRequestHandleEvent* e, std::any userData);

    enum class DeprecatedPixelFormat {
        YUVJ420P = AV_PIX_FMT_YUVJ420P,  ///< planar YUV 4:2:0, 12bpp, full scale (JPEG), deprecated in favor of AV_PIX_FMT_YUV420P and setting color_range
        YUVJ422P = AV_PIX_FMT_YUVJ422P,  ///< planar YUV 4:2:2, 16bpp, full scale (JPEG), deprecated in favor of AV_PIX_FMT_YUV422P and setting color_range
        YUVJ444P = AV_PIX_FMT_YUVJ444P,  ///< planar YUV 4:4:4, 24bpp, full scale (JPEG), deprecated in favor of AV_PIX_FMT_YUV444P and setting color_range
        YUVJ440P = AV_PIX_FMT_YUVJ440P,  ///< planar YUV 4:4:0 full scale (JPEG), deprecated in favor of AV_PIX_FMT_YUV440P and setting color_range
    };
    enum class SupportedPixelFormat {
        YUV420P = AV_PIX_FMT_YUV420P,  ///< planar YUV 4:2:0, 12bpp, full scale (JPEG), deprecated in favor of AV_PIX_FMT_YUV420P and setting color_range
        YUV422P = AV_PIX_FMT_YUV422P,  ///< planar YUV 4:2:2, 16bpp, full scale (JPEG), deprecated in favor of AV_PIX_FMT_YUV422P and setting color_range
        YUV444P = AV_PIX_FMT_YUV444P,  ///< planar YUV 4:4:4, 24bpp, full scale (JPEG), deprecated in favor of AV_PIX_FMT_YUV444P and setting color_range
        YUV440P = AV_PIX_FMT_YUV440P,  ///< planar YUV 4:4:0 full scale (JPEG), deprecated in favor of AV_PIX_FMT_YUV440P and setting color_range
    };
    struct DeprecatedSupportedPixelFormatHashType {
        size_t operator()(const DeprecatedPixelFormat& pixFmt) const {
            return static_cast<int>(pixFmt) % 7; // 12, 13, 14, 32映射到 5, 6, 7, 4
        }
    };
    std::unordered_map<DeprecatedPixelFormat, SupportedPixelFormat, DeprecatedSupportedPixelFormatHashType> mapDeprecatedSupportedPixelFormat{
        { DeprecatedPixelFormat::YUVJ420P, SupportedPixelFormat::YUV420P },
        { DeprecatedPixelFormat::YUVJ422P, SupportedPixelFormat::YUV422P },
        { DeprecatedPixelFormat::YUVJ444P, SupportedPixelFormat::YUV444P },
        { DeprecatedPixelFormat::YUVJ440P, SupportedPixelFormat::YUV440P },
    };
    bool isDeprecatedPixelFormat(AVPixelFormat pixFmt);

    AVPixelFormat getSupportedPixelFormat(AVPixelFormat pixFmt, bool& isDeprecated);

    SwsContext* checkAndGetCorrectSwsContext(SizeI srcSize, AVPixelFormat srcFmt, SizeI dstSize, AVPixelFormat dstFmt, SwsFlags flags);



};
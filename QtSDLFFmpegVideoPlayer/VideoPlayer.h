#pragma once
#include "PlayerPredefine.h"

class VideoPlayer : public PlayerInterface, private ConcurrentQueueOps
{
public:
    // 用于视频帧队列
    static constexpr int MAX_VIDEO_FRAME_QUEUE_SIZE = 20; // n frames
    static constexpr int MIN_VIDEO_FRAME_QUEUE_SIZE = 10; // n frames
    // 用于ffmpeg视频解码和播放
    // 下面两个常量需同时满足，解码才会暂停
    static constexpr size_t MAX_VIDEO_PACKET_QUEUE_SIZE = 200; // 最大视频帧队列数量
    //static constexpr size_t MAX_AUDIO_FRAME_QUEUE_SIZE = 200; // 最大音频帧队列数量
    // 低于下列值开始继续读取新的帧，取出新的值后<下列值开始通知读取线程
    static constexpr size_t MIN_VIDEO_PACKET_QUEUE_SIZE = 100; // 最小视频帧队列数量

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



    struct VideoPlayOptions {
        StreamIndexSelector streamIndexSelector{ nullptr };
        VideoClockSyncFunction clockSyncFunction{ nullptr };
        VideoRenderFunction renderer{ nullptr };
        UserDataType rendererUserData{ UserDataType{} };
        VideoFrameSwitchOptionsCallback frameSwitchCallback{ nullptr };
        UserDataType frameSwitchOptionsCallbackUserData{ UserDataType{} };
        bool enableHardwareDecoding{ false };
    };



private:
    struct VideoPlaybackStateVariables { // 用于存储播放状态相关的数据
        ConcurrentQueue<AVPacket*> packetQueue;
        ConcurrentQueue<AVFrame*> frameQueue;
        // 
        StreamIndexType streamIndex{ -1 };
        UniquePtr<AVFormatContext> formatCtx{ nullptr, constDeleterAVFormatContext };
        UniquePtr<AVCodecContext> codecCtx{ nullptr, constDeleterAVCodecContext };
        AVHWDeviceType hwDeviceType{ AV_HWDEVICE_TYPE_NONE };
        AVPixelFormat hwPixelFormat{ AV_PIX_FMT_NONE };
        // 时钟
        AtomicDouble videoClock{ 0.0 }; // 单位s

        // 系统时钟
        double realtimeClock = 0;

        // 清空队列
        void clearPktAndFrameQueues() {
            AVPacket* pkt = nullptr;
            while (tryDequeue(packetQueue, pkt))
                av_packet_free(&pkt);
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
            formatCtx.reset();
            codecCtx.reset();
            hwDeviceType = AV_HWDEVICE_TYPE_NONE;
            hwPixelFormat = AV_PIX_FMT_NONE;
            videoClock.store(0.0);
            realtimeClock = 0.0;

            // 清空请求任务队列
            {
                std::unique_lock lockMtxQueueRequestTasks(mtxQueueRequestTasks);
                Queue<RequestTaskItem> emptyQueue;
                std::swap(queueRequestTasks, emptyQueue);
            }
            threadStateManager.reset();
        }

        // 文件
        std::string filePath;
        VideoPlayOptions playOptions;
        // 请求任务队列
        Queue<RequestTaskItem> queueRequestTasks;
        Mutex mtxQueueRequestTasks; // 必须使用独占锁
        ConditionVariable cvQueueRequestTasks;
        //template <typename ...Args>
        //void pushRequestTaskQueue(Args... args) {
        //    std::unique_lock lockMtxQueueRequestTasks(playbackStateVariables.mtxQueueRequestTasks); // 独占锁
        //    playbackStateVariables.queueRequestTasks.emplace(args); // 构造入队
        //}
        void pushRequestTaskQueue(RequestTaskType type, std::function<void(std::any userData)> handler, std::any userData, std::vector<ThreadIdentifier> blockTargetThreadIds, RequestTaskProcessCallbacks callbacks) {
            std::unique_lock lockMtxQueueRequestTasks(mtxQueueRequestTasks); // 独占锁
            queueRequestTasks.emplace(type, handler, userData, blockTargetThreadIds, callbacks); // 构造入队
            // 记得通知处于等待的请求任务处理线程
            cvQueueRequestTasks.notify_all();
        }
        // 线程等待对象管理器
        ThreadStateManager threadStateManager;
    };
    struct SeekData {
        uint64_t pts{ 0 };
        StreamIndexType streamIndex{ -1 };
    };
    DefinePlayerLoggerSinks(loggerSinks, "VideoPlayer.class.log");
    Logger logger{ "VideoPlayer", loggerSinks };

    // 播放器状态
    Atomic<PlayerState> playerState{ PlayerState::Stopped };
    AtomicWaitObject<bool> waitStopped{ false }; // true表示已停止，false表示未停止
    VideoPlaybackStateVariables playbackStateVariables;

public:
    ~VideoPlayer() {
        stop();
    }
    bool play(const std::string& filePath, VideoPlayOptions options) {
        if (!isStopped())
            return false;
        if (!playerState.trySet(PlayerState::Preparing))
            return false;
        setFilePath(filePath);
        playbackStateVariables.playOptions = options;
        return playVideoFile();
    }
    bool play() override {
        if (!isStopped())
            return false;
        if (!playerState.trySet(PlayerState::Preparing))
            return false;
        if (playbackStateVariables.filePath.empty()) // 打开了文件才能播放
            return false;
        return playVideoFile(); // 重新播放
    }

    void resume() override { // 用于从暂停/停止状态恢复播放
        if (isPaused())
        {
            // 恢复播放
            playerState.set(PlayerState::Playing);
            // 唤醒所有线程
            playbackStateVariables.threadStateManager.wakeUpAll();
        }
    }
    void pause() override {
        if (playerState == PlayerState::Playing)
        {
            playerState.set(PlayerState::Pausing);
            // 唤醒所有线程
            playbackStateVariables.threadStateManager.wakeUpAll();
        }
    }
    void notifyStop() override {
        if (playerState != PlayerState::Stopping && !isStopped())
        {
            playerState.set(PlayerState::Stopping);
            // 唤醒所有线程
            playbackStateVariables.threadStateManager.wakeUpAll();
            // 唤醒请求任务等待，以便处理停止请求，退出处理线程
            playbackStateVariables.cvQueueRequestTasks.notify_all();
        }
    }
    void stop() override {
        if (playerState != PlayerState::Stopping && !isStopped())
        {
            notifyStop();
            waitStopped.wait(true); // 等待停止完成
            //resetPlayer(); // 播放结束会自动重置播放器
        }
    }

    // \param streamIndex -1表示使用AV_TIME_BASE计算，否则使用streamIndex指定的流的time_base
    void notifySeek(uint64_t pts, StreamIndexType streamIndex = -1, RequestTaskProcessCallbacks callbacks = RequestTaskProcessCallbacks{}) override {
        if (!isStopped() && playerState != PlayerState::Stopping)
        {
            // 提交seek任务
            auto seekHandler = std::bind(&VideoPlayer::requestTaskHandlerSeek, this, std::placeholders::_1);
            // 阻塞三个线程（即所有与读包解包相关的线程）
            auto&& blockThreadIds = { ThreadIdentifier::VideoReadingThread, ThreadIdentifier::VideoDecodingThread, ThreadIdentifier::VideoRenderingThread };
            playbackStateVariables.pushRequestTaskQueue(RequestTaskType::Seek, seekHandler, SeekData{ pts, streamIndex }, blockThreadIds, callbacks);
        }
    }
    void seek(uint64_t pts, StreamIndexType streamIndex = -1, RequestTaskProcessCallbacks callbacks = RequestTaskProcessCallbacks{}) override {
        notifySeek(pts, streamIndex, callbacks);
    }

    bool isPlaying() const override {
        return playerState == PlayerState::Playing;
    }
    bool isPaused() const override {
        return playerState == PlayerState::Paused;
    }
    bool isStopped() const override {
        return playerState == PlayerState::Stopped;
    }

    void setFilePath(const std::string& filePath) override {
        this->playbackStateVariables.filePath = filePath;
    }

    std::string getFilePath() const override {
        return playbackStateVariables.filePath;
    }

    void setStreamIndexSelector(const StreamIndexSelector& selector) override {
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
        this->playbackStateVariables.playOptions.enableHardwareDecoding = b;
    }

    void setFrameSwitchOptionsCallback(VideoFrameSwitchOptionsCallback callback, UserDataType userData) {
        this->playbackStateVariables.playOptions.frameSwitchCallback = callback;
        this->playbackStateVariables.playOptions.frameSwitchOptionsCallbackUserData = userData;
    }

private:
    void resetPlayer() {
        playbackStateVariables.reset();
        playerState.set(PlayerState::Stopped);
    }

    bool playVideoFile();

    // 打开文件
    bool openInput() {
        return MediaDecodeUtils::openFile(&logger, playbackStateVariables.formatCtx, playbackStateVariables.filePath);
    }
    
    // 查找流信息
    bool findStreamInfo() {
        return MediaDecodeUtils::findStreamInfo(&logger, playbackStateVariables.formatCtx.get());
    }

    // 查找并选择视频流
    bool findAndSelectVideoStream();

    // 查找并打开解码器
    bool findAndOpenVideoDecoder();

    // 从文件中读包
    void readPackets();

    void packet2VideoFrames();

    void renderVideo();

    void requestTaskProcessor();

    void requestTaskHandlerSeek(std::any userData);

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
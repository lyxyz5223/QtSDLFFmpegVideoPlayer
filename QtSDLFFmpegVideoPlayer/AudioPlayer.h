#pragma once
#include "PlayerPredefine.h"

// 音频库
#include "AudioAdapter.h"

class AudioPlayer : public PlayerInterface, private ConcurrentQueueOps
{
public:
    // 用于AudioAdapter音频缓冲区大小
    static constexpr unsigned int DEFAULT_AUDIO_OUTPUT_STREAM_BUFFER_SIZE = 1024;
    // 用于音频队列
    static constexpr int MAX_AUDIO_OUTPUT_STREAM_QUEUE_SIZE = 100;
    static constexpr int MIN_AUDIO_OUTPUT_STREAM_QUEUE_SIZE = MAX_AUDIO_OUTPUT_STREAM_QUEUE_SIZE / 2; // 1 / 2
    // 默认音频输出通道数
    static constexpr int DEFAULT_NUMBER_CHANNELS_AUDIO_OUTPUT = 1;
    // 下面两个常量需同时满足，解码才会暂停
    static constexpr size_t MAX_AUDIO_PACKET_QUEUE_SIZE = 200; // 最大音频帧队列数量
    //static constexpr size_t MAX_AUDIO_FRAME_QUEUE_SIZE = 200; // 最大音频帧队列数量
    // 低于下列值开始继续读取新的帧，取出新的值后<下列值开始通知读取线程
    static constexpr size_t MIN_AUDIO_PACKET_QUEUE_SIZE = 100; // 最小音频帧队列数量
    // 统一音频转换后用于播放的格式
    static constexpr AVSampleFormat AUDIO_OUTPUT_FORMAT = AVSampleFormat::AV_SAMPLE_FMT_S16;

    using AudioClockSyncFunction = std::function<bool(const AtomicDouble& audioClock, const AtomicBool& isClockStable, const double& audioRealtimeClock, int64_t& sleepTime)>;

public:
    struct AudioPlayOptions {
        StreamIndexSelector streamIndexSelector{ nullptr };
        AudioClockSyncFunction clockSyncFunction{ nullptr }; // Reserved for future use
    };
private:
    struct AudioStreamInfo {
        std::vector<uint8_t> dataBytes{}; // 如果是uint16_t格式的数据，nFrames表示播放缓冲区大小（在音频播放回调中使用），则dataBytes.size()应该是numberOfAudioOutputChannels * sizeof(uint16_t) * nFrames
        //std::span<uint8_t> dataBytes{}; // 如果是uint16_t格式的数据，nFrames表示播放缓冲区大小（在音频播放回调中使用），则dataBytes.size()应该是numberOfAudioOutputChannels * sizeof(uint16_t) * nFrames
        //std::vector<uint8_t> dataBytes{}; // 如果是uint16_t格式的数据，则dataBytes.size()应该是numberOfAudioOutputChannels * sizeof(uint16_t)
        double pts = 0.0; // 单位s
    };

    struct AudioPlaybackStateVariables { // 用于存储播放状态相关的数据
        // 音频输出与设备
        UniquePtrD<AudioAdapter> audioDevice;
        AtomicInt numberOfAudioOutputChannels = DEFAULT_NUMBER_CHANNELS_AUDIO_OUTPUT;
        Mutex mtxStreamQueue; // 用于保证在写入一段的时候不被读取
        Queue<AudioStreamInfo> streamQueue;
        ConcurrentQueue<AVPacket*> packetQueue;
        AtomicBool streamUploadFinished{ false };

        // 
        StreamIndexType streamIndex{ -1 };
        UniquePtr<AVFormatContext> formatCtx{ nullptr, constDeleterAVFormatContext };
        UniquePtr<AVCodecContext> codecCtx{ nullptr, constDeleterAVCodecContext };

        // 时钟
        AtomicDouble audioClock{ 0.0 }; // 单位s

        double realtimeClock{ 0.0 };

        // 文件
        std::string filePath;
        AudioPlayOptions playOptions;
        // 请求任务队列
        Queue<RequestTaskItem> queueRequestTasks;
        Mutex mtxQueueRequestTasks;
        ConditionVariable cvQueueRequestTasks;

        // 线程等待对象管理器
        ThreadStateManager threadStateManager;

        void clearPktAndStreamQueues() {
            AVPacket* pkt = nullptr;
            while (tryDequeue(packetQueue, pkt))
                av_packet_free(&pkt);
            Queue<AudioStreamInfo> streamQueueNew;
            streamQueue.swap(streamQueueNew);
        }
        // 重置所有变量，除了playOptions和filePath
        void reset() {
            if (audioDevice)
            {
                if (audioDevice->isStreamRunning())
                    audioDevice->stopStream();
                if (audioDevice->isStreamOpen())
                    audioDevice->closeStream();
            }
            // 清空队列
            clearPktAndStreamQueues();
            // 重置其他变量
            streamIndex = -1;
            formatCtx.reset();
            codecCtx.reset();
            audioClock.store(0.0);
            realtimeClock = 0.0;
            // 清空请求任务队列
            {
                std::unique_lock lockMtxQueueRequestTasks(mtxQueueRequestTasks);
                Queue<RequestTaskItem> emptyQueue;
                std::swap(queueRequestTasks, emptyQueue);
            }
            // 清空线程等待对象
            threadStateManager.reset();
        }

        void pushRequestTaskQueue(RequestTaskType type, std::function<void(MediaRequestHandleEvent* e, std::any userData)> handler, MediaRequestHandleEvent* event, std::any userData, std::vector<ThreadIdentifier> blockTargetThreadIds, RequestTaskProcessCallbacks callbacks) {
            std::unique_lock lockMtxQueueRequestTasks(mtxQueueRequestTasks); // 独占锁
            queueRequestTasks.emplace(type, handler, event, userData, blockTargetThreadIds, callbacks); // 构造入队
            // 记得通知处于等待的请求任务处理线程
            cvQueueRequestTasks.notify_all();
        }
    };
    struct SeekData {
        uint64_t pts{ 0 };
        StreamIndexType streamIndex{ -1 };
    };

    // 日志记录器
    DefinePlayerLoggerSinks(loggerSinks, "AudioPlayer.class.log");
    Logger logger{ "AudioPlayer", loggerSinks };

    // 播放器状态
    Atomic<PlayerState> playerState{ PlayerState::Stopped };
    AtomicWaitObject<bool> waitStopped{ false }; // true表示已停止，false表示未停止
    AudioPlaybackStateVariables playbackStateVariables;

public:
    AudioPlayer() : PlayerInterface(logger) {}
    ~AudioPlayer() {
        stop();
    }

    bool play(const std::string& filePath, const AudioPlayOptions& options) {
        if (!isStopped())
            return false;
        if (!trySetPlayerState(PlayerState::Preparing))
            return false;
        setFilePath(filePath);
        playbackStateVariables.playOptions = options;
        return playAudioFile();
    }
    bool play() override {
        if (!isStopped())
            return false;
        if (!trySetPlayerState(PlayerState::Preparing))
            return false;
        if (playbackStateVariables.filePath.empty()) // 打开了文件才能播放
            return false;
        return playAudioFile(); // 重新播放
    }

    void resume() override { // 用于从暂停/停止状态恢复播放
        if (isPaused())
        {
            // 恢复播放
            setPlayerState(PlayerState::Playing);
            // 唤醒所有线程
            playbackStateVariables.threadStateManager.wakeUpAll();
        }
    }
    void pause() override {
        if (playerState == PlayerState::Playing)
        {
            setPlayerState(PlayerState::Paused);
            // 唤醒所有线程
            playbackStateVariables.threadStateManager.wakeUpAll();
        }
    }
    void notifyStop() override {
        if (playerState != PlayerState::Stopping && !isStopped())
        {
            setPlayerState(PlayerState::Stopping);
            // 唤醒所有线程
            playbackStateVariables.threadStateManager.wakeUpAll();
            // 唤醒请求任务处理线程
            playbackStateVariables.cvQueueRequestTasks.notify_all();
        }
    }
    void stop() override {
        if (playerState != PlayerState::Stopping && !isStopped())
        {
            notifyStop();
            waitStopped.wait(true); // 等待停止完成
        }
    }

    // \param streamIndex -1表示使用AV_TIME_BASE计算，否则使用streamIndex指定的流的time_base
    void notifySeek(uint64_t pts, StreamIndexType streamIndex = -1, RequestTaskProcessCallbacks callbacks = RequestTaskProcessCallbacks{}) override {
        if (!isStopped() && playerState != PlayerState::Stopping)
        {
            // 提交seek任务
            auto seekHandler = std::bind(&AudioPlayer::requestTaskHandlerSeek, this, std::placeholders::_1, std::placeholders::_2);
            // 阻塞三个线程（即所有与读包解包相关的线程）
            auto&& blockThreadIds = { ThreadIdentifier::AudioReadingThread, ThreadIdentifier::AudioDecodingThread, ThreadIdentifier::AudioPlayingThread };
            playbackStateVariables.pushRequestTaskQueue(RequestTaskType::Seek, seekHandler, new MediaSeekEvent{ pts, streamIndex }, SeekData{ pts, streamIndex }, blockThreadIds, callbacks);
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

    void setClockSyncFunction(const AudioClockSyncFunction& func) {
        this->playbackStateVariables.playOptions.clockSyncFunction = func;
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
        auto e = MediaPlaybackStateChangeEvent{ newState, oldState };
        event(&e);
    }

    void resetPlayer() {
        playbackStateVariables.reset();
        setPlayerState(PlayerState::Stopped);
    }

    bool playAudioFile();

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


    // 打开文件
    bool openInput() {
        return MediaDecodeUtils::openFile(&logger, playbackStateVariables.formatCtx, playbackStateVariables.filePath);
    }

    // 查找流信息
    bool findStreamInfo() {
        return MediaDecodeUtils::findStreamInfo(&logger, playbackStateVariables.formatCtx.get());
    }

    // 查找并选择音频流
    bool findAndSelectAudioStream();

    // 查找并打开解码器
    bool findAndOpenAudioDecoder();

    // 从文件中读包
    void readPackets();

    // 将包转化为音频输出流
    void packet2AudioStreams();

    // 播放音频，预留用于同步输出，异步输出将使用renderAudioAsyncCallback
    void renderAudio();

    // 音频输出回调，用于异步输出
    // 内部处理音频的回调函数
    // \param userData 保留参数，暂时为指向当前VideoPlayer实例的指针
    AudioAdapter::AudioCallbackResult renderAudioAsyncCallback(void*& outputBuffer, void*& inputBuffer, unsigned int& nFrames, double& streamTime, AudioAdapter::AudioStreamStatuses& status, AudioAdapter::RawArgsType& rawArgs, AudioAdapter::UserDataType& userData);

    void requestTaskProcessor();

    void requestTaskHandlerSeek(MediaRequestHandleEvent* e, std::any userData);
};


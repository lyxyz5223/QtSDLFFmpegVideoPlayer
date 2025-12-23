#pragma once

// Audio Interface
#define USE_PORTAUDIO
//#define USE_RTAUDIO

// Video Interface

#include <mutex>
#include <shared_mutex>
#include <condition_variable>
#include <queue>
#include <any>
#include <functional>
#include <atomic>
#include <thread>
#include <variant>
#include <span>
//#include <Windows.h>

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libavutil/time.h>
#include <libavutil/hwcontext.h>
}
#include <Logger.h>

#include "MultiEnumTypeDefine.h"
#include "AtomicWaitObject.h"

#include <concurrentqueue.h>

#include "ThreadUtils.h"

class PlayerTypes
{
public:
    // 删除器类型
    template <typename T>
    using ConstDeleter = const std::function<void(T*)>;
    // AVCodecContext智能指针删除器类型
    using AVCodecContextConstDeleter = ConstDeleter<AVCodecContext>;
    // AVCodecContext智能指针删除器
    static AVCodecContextConstDeleter constDeleterAVCodecContext;
    // AVFrame智能指针删除器类型
    using AVPacketConstDeleter = ConstDeleter<AVPacket>;
    // AVFrame智能指针删除器
    static AVPacketConstDeleter constDeleterAVPacket;
    // AVFrame智能指针删除器类型
    using AVFrameConstDeleter = ConstDeleter<AVFrame>;
    // AVFrame智能指针删除器
    static AVFrameConstDeleter constDeleterAVFrame;
    // AVFormatContext删除器类型
    using AVFormatContextConstDeleter = ConstDeleter<AVFormatContext>;
    // AVFormatContext智能指针删除器
    static AVFormatContextConstDeleter constDeleterAVFormatContext;

    inline constexpr static auto ThreadSleepMs = SleepForMs;
    inline constexpr static auto ThreadYield = std::this_thread::yield;


    typedef long long StreamIndexType;
    // 查找到的流类型，支持按位与或运算符组合使用
    enum StreamType : char { // ST是StreamType的缩写，占用一个字节
        STNone = 0x0,
        STVideo = 0x1, // 视频
        STAudio = 0x2, // 音频
        STData = 0x4, // 数据
        STSubtitle = 0x8, // 字幕
        STAttachment = 0x10 // 附件
    };
    // 定义：支持按位与或运算符的StreamType组合类型
    using StreamTypes = MultiEnumTypeDefine<StreamType>;
    // 队列
    template <class T, class Container = std::deque<T>>
    using Queue = std::queue<T, Container>;
    template <typename T, typename Traits = moodycamel::ConcurrentQueueDefaultTraits>
    using ConcurrentQueue = moodycamel::ConcurrentQueue<T, Traits>;
    // 带自定义deleter的智能指针
    template<typename T>
    using UniquePtr = std::unique_ptr<T, ConstDeleter<T>>;
    template<typename T, typename Deleter = std::default_delete<T>>
    using UniquePtrD = std::unique_ptr<T, Deleter>;
    // 流索引映射类型
    template <typename T>
    using FromStreamIndexMap = std::unordered_map<StreamIndexType, T>;

    using Mutex = std::mutex;
    using SharedMutex = std::shared_mutex;
    using ConditionVariable = std::condition_variable;
    template <typename T>
    class Atomic : public std::atomic<T> {
    public:
        using std::atomic<T>::atomic;
        virtual void set(T s) {
            auto tmp = this->load();
            while (!this->compare_exchange_strong(tmp, s));
        }
        virtual void set(T s, T& old) {
            old = this->load();
            while (!this->compare_exchange_strong(old, s));
        }
        virtual bool trySet(T s) {
            auto tmp = this->load();
            return this->compare_exchange_strong(tmp, s);
        }
        virtual bool trySet(T s, T& old) {
            old = this->load();
            return this->compare_exchange_strong(old, s);
        }
    };
    using AtomicInt = std::atomic<int>;
    using AtomicDouble = std::atomic<double>;
    using AtomicBool = std::atomic<bool>;

    template <typename T>
    class AtomicStateMachine : public Atomic<T> {
        Atomic<T> prevValue;
    public:
        using Atomic<T>::Atomic;
        virtual void set(T s) override {
            auto tmp = this->load();
            prevValue.set(tmp);
            while (!this->compare_exchange_strong(tmp, s))
                prevValue.set(tmp);
        }
        virtual void set(T s, T& old) override {
            old = this->load();
            while (!this->compare_exchange_strong(old, s))
                prevValue.set(old);
        }
        virtual bool trySet(T s) override {
            auto tmp = this->load();
            bool r = this->compare_exchange_strong(tmp, s);
            prevValue.set(tmp);
            return r;
        }
        virtual bool trySet(T s, T& old) override {
            old = this->load();
            bool r = this->compare_exchange_strong(old, s);
            prevValue.set(old);
            return r;
        }
        virtual T prev() const {
            return prevValue.load();
        }
    };

    template <typename T>
    struct Size
    {
        T w = -1; // width
        T h = -1; // height
    public:
        Size() noexcept {}
        Size(const Size& other) noexcept : w(other.w), h(other.h) {}
        Size(const Size&& other) noexcept : w(other.w), h(other.h) {}
        Size(T width, T height) : w(width), h(height) {}
        ~Size() noexcept = default;
        T width() const noexcept { return w; }
        T height() const noexcept { return h; }
        void setWidth(T width) noexcept { w = width; }
        void setHeight(T height) noexcept { h = height; }
        void setSize(T width, T height) noexcept { w = width; h = height; }
        bool isEmpty() const noexcept { return w == -1 && h == -1; }
        bool isNull() const noexcept { return w == 0 && h == 0; }
        bool isValid() const noexcept { return w >= 0 && h >= 0; }
        Size& operator=(const Size& other) noexcept {
            if (this != &other) {
                w = other.w;
                h = other.h;
            }
            return *this;
        }
        bool isEqual(const Size& o) const {
            return w == o.w && h == o.h;
        }
        bool operator==(const Size& o) const {
            return isEqual(o);
        }
        bool operator!=(const Size& o) const {
            return !isEqual(o);
        }
    };
    using SizeI = Size<int>;
    using SizeL = Size<long>;
    using SizeLL = Size<long long>;
    using SizeF = Size<float>;
    using SizeD = Size<double>;

    enum class PlayerState {
        Stopped = 0,
        Paused = 1,
        Playing = 2,
        //Stopping = Stopped,
        Stopping = 3,
        Pausing = Paused,
        //Pausing = 4,
        Preparing = 5,
        Seeking = 6,
        //Finished = 7,
    };

    enum class MediaType : char {
        Unknown = -1,
        Video = 0,
        Audio = 1,
        Data = 2,
        Subtitle = 3,
        Attachment = 4,
    };

    enum class RequestTaskType : char {
        None = 0,
        Seek = 1,
    };
    enum class RequestHandleState {
        BeforeHandle,
        AfterHandle
    };
    enum class ThreadIdentifier {
        None = 0,
        VideoReadingThread = 1, // av_read_frame线程
        VideoDecodingThread, // av_send_packet + av_receive_frame线程
        VideoRenderingThread, // renderer渲染线程
        AudioReadingThread, // av_read_frame线程
        AudioDecodingThread, // av_send_packet + av_receive_frame线程
        AudioPlayingThread, // audio播放线程
    };

    class MediaEventType {
    public:
        enum MediaEvent : int {
            None = 0,
            PlaybackStateChange,
            RequestHandle,
        };
    private:
        MediaEvent type{ None };
    public:
        MediaEventType(MediaEvent t) : type(t) {}
        MediaEventType(const MediaEventType& t) : type(t.type) {}
        MediaEventType(const MediaEventType&& t) noexcept : type(t.type) {}
        ~MediaEventType() = default;
        MediaEvent value() const { return type; }
        std::string name() const;
        bool isEqual(const MediaEventType& other) const {
            return type == other.type;
        }
        operator MediaEvent() {
            return type;
        }
        bool operator==(const MediaEventType& other) const {
            return isEqual(other);
        }
        bool operator!=(const MediaEventType& other) const {
            return !isEqual(other);
        }
        MediaEventType& operator=(const MediaEventType& other) {
            if (this != &other)
                type = other.type;
            return *this;
        }
    };
    using MediaUserDataType = std::any;
    class MediaEventDispatcher; // 前置声明
    class MediaEvent {
        friend class MediaEventDispatcher;
        virtual void setType(MediaEventType t) final { this->eventType = t; }
    protected:
        MediaEventType eventType{ MediaEventType::None };
    public:
        MediaEvent(MediaEventType t) : eventType(t) {}
        virtual MediaEventType type() const { return this->eventType; }
    };
    //class MediaEventDispatcher {
    //    PlayerInterface* player{ nullptr };
    //public:
    //    MediaEventDispatcher(PlayerInterface* player) : player(player) {}
    //    //using MediaEventCallback = std::function<void(const MediaEventInterface& event, const MediaUserDataType& userData)>;
    //    template <typename... Args>
    //    bool invoke(MediaEventType eventType, Args... args) {
    //        return true;
    //    }
    //    template <typename Arg>
    //    bool invoke(const MediaEvent& event, Arg arg) {
    //        return true;
    //    }
    //};
    class MediaPlaybackStateChangeEvent : public MediaEvent {
    private:
        PlayerState os{ PlayerState::Stopped };
        PlayerState s{ PlayerState::Stopped };
    public:
        MediaPlaybackStateChangeEvent(PlayerState state, PlayerState oldState)
            : MediaEvent(MediaEventType::PlaybackStateChange), os(oldState), s(state) {
        }
        PlayerState state() const { return s; }
        PlayerState oldState() const { return os; }
    };
    class MediaRequestHandleEvent : public MediaEvent {
    protected:
        RequestHandleState state{ RequestHandleState::BeforeHandle };
        RequestTaskType reqType{ RequestTaskType::Seek };
        bool accepted{ true }; // 默认接受该请求
    public:
        MediaRequestHandleEvent(RequestTaskType t)
            : MediaEvent(MediaEventType::RequestHandle), reqType(t) {
        }
        MediaRequestHandleEvent(RequestHandleState s, RequestTaskType t)
            : MediaEvent(MediaEventType::RequestHandle), state(s), reqType(t) {
        }
        MediaRequestHandleEvent(RequestHandleState newState, const MediaRequestHandleEvent& toClone)
            : MediaEvent(MediaEventType::RequestHandle), state(newState), reqType(toClone.reqType), accepted(toClone.accepted) {
        }
        virtual RequestHandleState handleState() const { return state; }
        virtual RequestTaskType requestType() const { return reqType; }
        // 接受该请求
        virtual void accept() { accepted = true; }
        // 忽略该请求
        virtual void ignore() { accepted = false; }
        virtual void setAccepted(bool a) { accepted = a; }
        virtual bool isAccepted() const { return accepted; }
        virtual UniquePtrD<MediaRequestHandleEvent> clone() const {
            return std::make_unique<MediaRequestHandleEvent>(*this);
        }
        virtual UniquePtrD<MediaRequestHandleEvent> clone(RequestHandleState newState) const {
            return std::make_unique<MediaRequestHandleEvent>(newState, *this);
        }
    };
    // MediaSeekEvent 也是 MediaRequestHandleEvent 的特殊情况
    class MediaSeekEvent : public MediaRequestHandleEvent {
    protected:
        uint64_t pts{ 0 };
        StreamIndexType idx{ -1 };
    public:
        MediaSeekEvent(uint64_t pts, StreamIndexType streamIndex = -1)
            : MediaRequestHandleEvent(RequestTaskType::Seek), pts(pts), idx(streamIndex) {}
        MediaSeekEvent(RequestHandleState handleState, uint64_t pts, StreamIndexType streamIndex = -1)
            : MediaRequestHandleEvent(handleState, RequestTaskType::Seek), pts(pts), idx(streamIndex) {}
        MediaSeekEvent(RequestHandleState newState, const MediaSeekEvent& toClone)
            : MediaRequestHandleEvent(newState, toClone), pts(toClone.pts), idx(toClone.idx) {}
        virtual uint64_t timestamp() const { return pts; }
        virtual StreamIndexType streamIndex() const { return idx; }
        virtual UniquePtrD<MediaRequestHandleEvent> clone() const override {
            return std::make_unique<MediaSeekEvent>(*this);
        }
        virtual UniquePtrD<MediaRequestHandleEvent> clone(RequestHandleState newState) const override {
            return std::make_unique<MediaSeekEvent>(newState, *this);
        }

    };

    // 用于处理一些请求任务
    struct RequestTaskItem;
    using RequestTaskProcessCallback = std::function<void(const RequestTaskItem& taskItem)>;
    struct RequestTaskProcessCallbacks {
        RequestTaskProcessCallback beforeProcess;
        RequestTaskProcessCallback afterProcess;
    };
    struct RequestTaskItem {
        RequestTaskType type{ RequestTaskType::None }; // 任务类型
        std::function<void(MediaRequestHandleEvent* e, std::any userData)> handler; // 处理函数
        MediaRequestHandleEvent* event; // 关联的事件对象指针
        std::any userData; // 用户数据
        std::vector<ThreadIdentifier> blockTargetThreadIds; // 需要暂停等待当前任务处理完成的线程ID列表
        RequestTaskProcessCallbacks callbacks;
    };

    // 打开文件的时候会查找流，并调用流选择器，用于选择需要解码的流
    // 返回true表示选择成功，outStreamIndex为选中的流索引，streamIndicesList为所有可选流索引列表
    // 如果返回false，则表示未选择任何流
    using StreamIndexSelector = std::function<bool(StreamIndexType& outStreamIndex, MediaType type, const std::vector<StreamIndexType>& streamIndicesList, const AVFormatContext* fmtCtx, const AVCodecContext* codecCtx)>;



    class ThreadStateManager {
    public:
        enum ThreadState {
            Blocking = 0,
            Blocked,
            Playing,
            Paused,
            Pausing,
        };
    private:
        //typedef AtomicWaitObject<ThreadState> WaitObject;
        struct Obj
        {
            ThreadIdentifier tid{ ThreadIdentifier::None };
            Atomic<ThreadState> state = Playing;
            ConditionVariable cv;
            Mutex mtx;
            AtomicBool noWakeUp = false;
            Obj() {}
            Obj(ThreadIdentifier tid) : tid(tid) {}
        };
        using ObjsHashMap = std::unordered_map<ThreadIdentifier, Obj>;
        ObjsHashMap mapObjs;
        SharedMutex mtxMapObjs;
    public:
        class ThreadStateController {
        private:
            Obj& obj;
            SharedMutex* mtx = nullptr;
        public:
            // 无锁构造
            ThreadStateController(Obj& s) : obj(s) {}
            // 带共享锁构造，可以延长shared_lock的生命周期
            ThreadStateController(Obj& s, SharedMutex& mtx) : obj(s), mtx(&mtx) {}
            bool testFlag(ThreadState s) const {
                return obj.state.load() == s;
            }
            // 状态
            bool isBlocking() const {
                return testFlag(Blocking);
            }
            bool isBlocked() const {
                return testFlag(Blocked);
            }
            bool isPlaying() const {
                return testFlag(Playing);
            }
            bool isPausing() const {
                return testFlag(Pausing);
            }
            bool isPaused() const {
                return testFlag(Paused);
            }
            // 暂停线程
            void pause() {
                std::unique_lock lock(obj.mtx);
                if (obj.state != Playing && obj.state != Pausing)
                    return;
                obj.state.set(Paused);
                obj.cv.notify_all();
                obj.cv.wait(lock, [&] { return obj.state != Pausing && obj.state != Paused; });
            }
            // 阻塞线程
            void block() {
                std::unique_lock lock(obj.mtx);
                if (obj.state != Playing && obj.state != Blocking)
                    return;
                obj.state.set(Blocked);
                obj.cv.notify_all();
                obj.cv.wait(lock, [&] { return obj.state != Blocking && obj.state != Blocked; });
            }
            // 设置为阻塞并等待状态变更
            void setBlockedAndWaitChanged(bool wakeUpBeforeWait = true) {
                std::unique_lock lock(obj.mtx);
                obj.noWakeUp = true;
                obj.state.set(Blocking);
                if (wakeUpBeforeWait)
                    obj.cv.notify_all();
                obj.cv.wait(lock, [&] { return obj.state == Blocked; });
                obj.noWakeUp = false;
            }
            // 唤醒线程
            void wakeUp() {
                std::unique_lock lock(obj.mtx);
                if (obj.noWakeUp) return;
                obj.state.set(Playing);
                obj.cv.notify_all();
            }
            ThreadIdentifier getThreadId() const {
                return obj.tid;
            }
        };
    public:
        class AutoRemovedThreadObj {
            ThreadStateManager& tsm;
            ThreadStateController& obj;
        public:
            AutoRemovedThreadObj(ThreadStateManager& tsm, ThreadStateController& obj) : tsm(tsm), obj(obj) {}
            ~AutoRemovedThreadObj() {
                tsm.removeThread(obj.getThreadId());
            }
        };

        ThreadStateController addThread(ThreadIdentifier tid) {
            std::unique_lock lockMtxMapObjs(mtxMapObjs);
            // < C++17 写法
            //auto [it, inserted] = mapObjs.emplace(std::piecewise_construct, std::forward_as_tuple(tid), std::forward_as_tuple());
            auto& waitObj = mapObjs.try_emplace(tid, tid).first->second;
            return ThreadStateController(waitObj);
        }
        void removeThread(ThreadIdentifier tid) {
            std::unique_lock lockMtxMapObjs(mtxMapObjs);
            auto it = mapObjs.find(tid);
            if (it != mapObjs.end())
                mapObjs.erase(it);
        }
        ThreadStateController get(ThreadIdentifier tid) {
            std::shared_lock readLockMtxMapObjs(mtxMapObjs);
            auto it = mapObjs.find(tid);
            if (it != mapObjs.end())
            {
                // 通知解码线程继续工作
                return ThreadStateController(it->second); // 返回带锁的控制器
            }
            throw std::runtime_error("ThreadIdentifier not found in ThreadStateManager.");
        }
        bool wakeUpById(ThreadIdentifier tid) {
            try {
                auto&& t = get(tid);
                t.wakeUp();
            }
            catch (...)
            {
                return false;
            }
            return true;
        }
        void wakeUpAll() {
            std::shared_lock readLockMtxMapObjs(mtxMapObjs);
            for (auto it = mapObjs.begin(); it != mapObjs.end(); ++it)
                ThreadStateController(it->second).wakeUp();
        }
        void reset() {
            std::unique_lock lockMtxMapObjs(mtxMapObjs);
            mapObjs.clear();
        }
        // 参数1决定是否继续遍历，如果返回false，则停止遍历过程
        void visit(std::function<bool(ObjsHashMap::iterator& iter, std::any& userData)> func, std::any userData) {
            std::shared_lock readLockMtxMapObjs(mtxMapObjs);
            for (auto it = mapObjs.begin(); it != mapObjs.end(); ++it)
                if (!func(it, userData))
                    break;
        }
    };

};


class MediaDecodeUtils : public PlayerTypes
{
private:
public:
    static bool openFile(Logger* logger, AVFormatContext*& fmtCtx, const std::string& filePath)
    {
        if (filePath.empty())
        {
            logger->error("File path is empty.");
            return false;
        }
        // 打开输入文件
        fmtCtx = nullptr;
        if (avformat_open_input(&fmtCtx, filePath.c_str(), nullptr, nullptr) < 0)
        {
            logger->error("Cannot open file: {}", filePath.c_str());
            return false;
        }
        return true;
    }
    static bool openFile(Logger* logger, UniquePtr<AVFormatContext>& fmtCtx, const std::string& filePath)
    {
        AVFormatContext* p = nullptr;
        bool rst = openFile(logger, p, filePath);
        fmtCtx.reset(p);
        return rst;
    }

    static bool findStreamInfo(Logger* logger, AVFormatContext* formatCtx)
    {
        if (avformat_find_stream_info(formatCtx, nullptr) < 0)
        {
            logger->error("Cannot find stream info.");
            return false;
        }
        return true;
    }

    static bool readFrame(Logger* logger, AVFormatContext* fmtCtx, AVPacket*& packet, bool allocPacket = true, bool* isEof = nullptr)
    {
        if (allocPacket)
            packet = av_packet_alloc();
        if (!packet)
        {
            logger->error("AVPacket is null.");
            return false;
        }
        int ret = av_read_frame(fmtCtx, packet);
        if (ret < 0)
        {
            if (allocPacket)
            { // 只有在函数内部分配的packet才需要释放
                av_packet_free(&packet);
                packet = nullptr;
            }
            if (ret == AVERROR_EOF)
            {
                // 读取到文件末尾
                if (isEof)
                    *isEof = true;
                logger->trace("Reached end of file.");
                return false;
            }
            else
            {
                if (isEof)
                    *isEof = false;
                logger->error("Error reading frame: {}", ret);
                return false;
            }
        }
        else
        {
            if (isEof)
                *isEof = true;
        }
        return true;
    }

    static bool findAndOpenAudioDecoder(Logger* logger, AVFormatContext* formatCtx, StreamIndexType streamIndex, UniquePtr<AVCodecContext>& codecContext)
    {
        auto* codecPar = formatCtx->streams[streamIndex]->codecpar;
        // 对每个流都尝试初始化解码器
        const AVCodec* codec = avcodec_find_decoder(codecPar->codec_id); // 查找解码器，不需要手动释放
        if (!codec)
        {
            logger->error("Cannot find audio decoder.");
            return false;
        }
        auto* codecCtx = avcodec_alloc_context3(codec);
        UniquePtr<AVCodecContext> uniquePtr(codecCtx, constDeleterAVCodecContext);
        if (avcodec_parameters_to_context(codecCtx, codecPar) < 0)
        {
            logger->error("Cannot copy audio decoder parameters to context.");
            return false;
        }
        if (avcodec_open2(codecCtx, codec, nullptr) < 0)
        {
            logger->error("Cannot open audio decoder.");
            return false;
        }
        uniquePtr.release();
        codecContext.reset(codecCtx);
        return true;
    }

    static bool findAndOpenVideoDecoder(Logger* logger, AVFormatContext* formatCtx, StreamIndexType streamIndex, UniquePtr<AVCodecContext>& codecContext, bool useHardwareDecoder = false, AVHWDeviceType* hwDeviceType = nullptr, AVPixelFormat* hwPixelFormat = nullptr) {
        auto* videoCodecPar = formatCtx->streams[streamIndex]->codecpar;
        // 对每个视频流都尝试初始化解码器
        const AVCodec* videoCodec = avcodec_find_decoder(videoCodecPar->codec_id); // 查找解码器，不需要手动释放
        if (!videoCodec)
        {
            logger->error("Cannot find video decoder.");
            return false;
        }
        auto* videoCodecCtx = avcodec_alloc_context3(videoCodec);
        UniquePtr<AVCodecContext> uniquePtr(videoCodecCtx, constDeleterAVCodecContext);
        if (avcodec_parameters_to_context(videoCodecCtx, videoCodecPar) < 0)
        {
            logger->error("Cannot copy video decoder parameters to context.");
            return false;
        }
        if (useHardwareDecoder)
        {
            // 初始化硬件解码器与媒体相关的只需要用到videoCodecCtx变量，但是会使用hwDeviceType变量作为目标硬件类型
            auto type = findHardwareDecoder(logger, videoCodec, AV_HWDEVICE_TYPE_NONE, *hwDeviceType, *hwPixelFormat);
            while (!initHardwareDecoder(logger, videoCodecCtx, *hwDeviceType) && type != AV_HWDEVICE_TYPE_NONE)
            {
                if (videoCodecCtx->hw_device_ctx)
                {
                    av_buffer_unref(&videoCodecCtx->hw_device_ctx);
                    videoCodecCtx->hw_device_ctx = nullptr;
                }
                type = findHardwareDecoder(logger, videoCodec, type, *hwDeviceType, *hwPixelFormat);
            }
            if (type == AV_HWDEVICE_TYPE_NONE)
                logger->warning("Hardware decoder not supported, using software instead.");
        }
        if (avcodec_open2(videoCodecCtx, videoCodec, nullptr) < 0)
        {
            logger->error("Cannot open decoder");
            return false;
        }
        codecContext.reset(uniquePtr.release()); // 先构造智能指针放入，失败时再弹出，届时自动释放上下文内存
        return true;
    }
    static void listAllHardwareDecoders(Logger* logger)
    {
        // 列举所有支持的硬件解码器类型
        AVHWDeviceType type = AV_HWDEVICE_TYPE_NONE;
        logger->info("Find hardware decoders:");
        for (size_t i = 1; (type/*current type*/ = av_hwdevice_iterate_types(type/*previous type*/)) != AV_HWDEVICE_TYPE_NONE; ++i)
        {
            logger->info("\t decoder {}: {}", i, av_hwdevice_get_type_name(type));
        }
    }

    // fromType 表示从AVHWDeviceType的哪一个的下一个开始遍历查找
    // hwDeviceType与hwPixelFormat为输出
    static AVHWDeviceType findHardwareDecoder(Logger* logger, const AVCodec* codec, AVHWDeviceType fromType, AVHWDeviceType& hwDeviceType, AVPixelFormat& hwPixelFormat)
    {
        AVHWDeviceType selectedHWDeviceType = AV_HWDEVICE_TYPE_NONE;
        AVPixelFormat selectedHWPixelFormat = AV_PIX_FMT_NONE;
        for (AVHWDeviceType type = fromType; (type/*current type*/ = av_hwdevice_iterate_types(type/*previous type*/)) != AV_HWDEVICE_TYPE_NONE; )
        {
            bool found = false;
            for (size_t i = 0; ; ++i)
            {
                const AVCodecHWConfig* config = avcodec_get_hw_config(codec, i);
                if (!config)
                {
                    logger->info("Decoder {} does not support device type {}.", codec->name, av_hwdevice_get_type_name(type));
                    break;
                }
                if (config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX && config->device_type == type)
                {
                    selectedHWPixelFormat = config->pix_fmt;
                    selectedHWDeviceType = type; // 选择第一个可用的硬件解码器类型
                    logger->info("Decoder {} supports device type {} with pixel format {}.", codec->name, av_hwdevice_get_type_name(type), static_cast<std::underlying_type_t<AVPixelFormat>>(selectedHWPixelFormat));
                    found = true;
                    break;
                }
            }
            if (found)
                break;
        }
        hwDeviceType = selectedHWDeviceType;
        hwPixelFormat = selectedHWPixelFormat;
        return selectedHWDeviceType;
    }

    // 会初始化并设置codecCtx的hw_device_ctx成员
    static bool initHardwareDecoder(Logger* logger, AVCodecContext* codecCtx, AVHWDeviceType hwDeviceType)
    {
        AVBufferRef* hwDeviceCtx = nullptr;
        if (av_hwdevice_ctx_create(&hwDeviceCtx, hwDeviceType, nullptr, nullptr, 0) < 0)
        {
            logger->error("Cannot create hardware context.");
            return false;
        }
        if (av_hwdevice_ctx_init(hwDeviceCtx) < 0)
        {
            logger->error("Cannot initialize hardware context.");
            if (hwDeviceCtx)
                av_buffer_unref(&hwDeviceCtx);
            return false;
        }
        codecCtx->hw_device_ctx = hwDeviceCtx; // 创建的时候自动添加了一次引用，这里只需要指针赋值，因为局部变量hwDeviceCtx不再使用
        return true;
    }

};

// 定义播放器日志槽列表
#define DefinePlayerLoggerSinks(variableName, fileLoggerNameIfNeeded) \
const std::vector<LoggerSinkPtr> variableName{ \
    std::make_shared<ConsoleLoggerSink>(), \
    std::make_shared<FileLoggerSink>(fileLoggerNameIfNeeded), \
    /*End*/}
/////

class ConcurrentQueueOps
{
public:
    // 并发队列操作封装
    template <typename T, typename Item>
    static void enqueue(T&& queue, Item&& item) {
        queue.enqueue(std::forward<Item>(item));
    }
    template <typename T, typename Item>
    static bool tryDequeue(T&& queue, Item&& item) {
        return queue.try_dequeue(std::forward<Item>(item));
    }
    template <typename T>
    static auto getQueueSize(T&& queue) {
        return queue.size_approx();
    }
    template <typename T>
    static bool isQueueEmpty(T&& queue) {
        return getQueueSize(queue) == 0;
    }

};

class PlayerInterface : public PlayerTypes
{
private:
    Logger& logger;
private:
    void playbackStateChangeEventHandler(MediaPlaybackStateChangeEvent* e);
    void requestHandleEventHandler(MediaRequestHandleEvent* e);
public:
    PlayerInterface(Logger& logger) : logger(logger) {}
    virtual ~PlayerInterface() = default;
    virtual bool play() = 0;
    virtual void resume() = 0;
    virtual void pause() = 0;
    virtual void notifyStop() = 0;
    virtual void stop() = 0;
    virtual void notifySeek(uint64_t pts, StreamIndexType streamIndex = -1, RequestTaskProcessCallbacks callbacks = RequestTaskProcessCallbacks{}) = 0;
    virtual void seek(uint64_t pts, StreamIndexType streamIndex = -1, RequestTaskProcessCallbacks callbacks = RequestTaskProcessCallbacks{}) = 0;
    virtual bool isPlaying() const = 0;
    virtual bool isPaused() const = 0;
    virtual bool isStopped() const = 0;
    virtual void setFilePath(const std::string& filePath) = 0;
    virtual std::string getFilePath() const = 0;
    virtual void setStreamIndexSelector(const StreamIndexSelector& selector) = 0;
    //virtual void setVolume(double volume) = 0;
    
    // 方法一
    //virtual void addEventHandler(size_t handlerId, std::function<void(const MediaEventType& eventType, const std::any& eventData)> handler) = 0;
    //virtual void removeEventHandler(size_t handlerId) = 0;

    // 方法二
protected:
    virtual bool event(MediaEvent* e);
    virtual void playbackStateChangeEvent(MediaPlaybackStateChangeEvent* e) {}
    // 已经开始播放事件
    virtual void startEvent(MediaPlaybackStateChangeEvent* e) {}
    // 暂停播放事件
    virtual void pauseEvent(MediaPlaybackStateChangeEvent* e) {}
    // 恢复播放事件
    virtual void resumeEvent(MediaPlaybackStateChangeEvent* e) {}
    // 播放已经停止事件
    virtual void stopEvent(MediaPlaybackStateChangeEvent* e) {}
    virtual void requestHandleEvent(MediaRequestHandleEvent* e) {}
    virtual void seekEvent(MediaSeekEvent* e) {}

};
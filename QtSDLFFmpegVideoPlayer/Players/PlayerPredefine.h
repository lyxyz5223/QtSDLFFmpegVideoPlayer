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
#include <libavfilter/avfilter.h> // 音量调节，倍速等
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libavutil/time.h>
#include <libavutil/hwcontext.h>
}
#include <Logger.h>

#include "EnumDefine.h"
#include "MultiEnumTypeDefine.h"
#include "AtomicWaitObject.h"

#include <concurrentqueue.h>

#include "ThreadUtils.h"

// 定义播放器日志槽列表
#define DefinePlayerLoggerSinks(variableName, fileLoggerNameWithoutSuffix) \
const std::vector<LoggerSinkPtr> variableName{ \
    std::make_shared<ConsoleLoggerSink>(), \
    std::make_shared<FileLoggerSink>(std::string(".\\logs\\") + fileLoggerNameWithoutSuffix + std::string(".class.log")), \
    /*End*/}
/////


class PlayerInterface; // 前置声明
class MediaDecodeUtils;
class ConcurrentQueueOps;
class PlayerTypes;

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
    // AVFilterContext删除器类型
    using AVFilterContextConstDeleter = ConstDeleter<AVFilterContext>;
    // AVFilterContext智能指针删除器
    static AVFilterContextConstDeleter constDeleterAVFilterContext;
    // AVFilterGraph删除器类型
    using AVFilterGraphConstDeleter = ConstDeleter<AVFilterGraph>;
    // AVFilterGraph智能指针删除器
    static AVFilterGraphConstDeleter constDeleterAVFilterGraph;

    inline constexpr static auto ThreadSleepMs = SleepForMs;
    inline constexpr static auto ThreadYield = std::this_thread::yield;


    typedef long long StreamIndexType;
    // 查找到的流类型，支持按位与或运算符组合使用
    // 基础类型必须为unsigned类型，否则需要修改streamTypesVisit函数中的max计算方式
    enum StreamType : unsigned char { // ST是StreamType的缩写，占用一个字节
        STNone = 0x0,
        STVideo = 0x1, // 视频
        STAudio = 0x2, // 音频
        STData = 0x4, // 数据
        STSubtitle = 0x8, // 字幕
        STAttachment = 0x10, // 附件
        STAll = 0xFF
    };
    // 定义：支持按位与或运算符的StreamType组合类型
    using StreamTypes = MultiEnumTypeDefine<StreamType>;
    constexpr static size_t StreamTypeSize = sizeof(std::underlying_type_t<StreamType>); // StreamType枚举值字节数
    constexpr static size_t StreamTypeCount = 5; // StreamType枚举值字节数
    // 返回false结束遍历，true继续遍历
    static void streamTypesVisit(StreamTypes streamTypes, std::function<bool(StreamType streamType, std::any userData)> visit, std::any userData = std::any{}) { // 遍历
        size_t count = 0;
        std::underlying_type_t<StreamType> max = (1 << (StreamTypeSize * 8 - 1));
        //memset(&max, 0xFF, StreamTypeSize); // 全1
        for (std::underlying_type_t<StreamType> i = STNone + 1; i <= max; i <<= 1/*左移1位*/)
        {
            if (streamTypes.contains(static_cast<StreamType>(i)))
            {
                ++count;
                if (!visit(static_cast<StreamType>(i), userData))
                    break;
            }
            if (count == StreamTypeCount || i == max)
                break; // 已遍历的数量达到定义的枚举值数量，结束遍历
        }
    }
    static constexpr AVMediaType streamTypeToAVMediaType(StreamType type) {
        switch (type)
        {
        case StreamType::STVideo:
            return AVMEDIA_TYPE_VIDEO;
        case StreamType::STAudio:
            return AVMEDIA_TYPE_AUDIO;
        case StreamType::STData:
            return AVMEDIA_TYPE_DATA;
        case StreamType::STSubtitle:
            return AVMEDIA_TYPE_SUBTITLE;
        case StreamType::STAttachment:
            return AVMEDIA_TYPE_ATTACHMENT;
        default:
            return AVMEDIA_TYPE_NB;
        }
    }

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
    template<typename T>
    using SharedPtr = std::shared_ptr<T>;

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
        using std::atomic<T>::operator=;
        virtual void set(T s) {
            auto tmp = this->load();
            while (!this->compare_exchange_strong(tmp, s));
        }
        virtual void set(T s, T& old) {
            auto tmp = this->load();
            while (!this->compare_exchange_strong(tmp, s));
            old = tmp;
        }
        virtual bool trySet(T s) {
            auto tmp = this->load();
            return this->compare_exchange_strong(tmp, s);
        }
        virtual bool trySet(T s, T& old) {
            auto tmp = this->load();
            bool r =this->compare_exchange_strong(tmp, s);
            old = tmp;
            return r;
        }
    };
    using AtomicInt = Atomic<int>;
    using AtomicDouble = Atomic<double>;
    using AtomicBool = Atomic<bool>;

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
            auto tmp = this->load();
            prevValue.set(tmp);
            while (!this->compare_exchange_strong(tmp, s))
                prevValue.set(tmp);
            old = tmp;
        }
        virtual bool trySet(T s) override {
            auto tmp = this->load();
            bool r = this->compare_exchange_strong(tmp, s);
            prevValue.set(tmp);
            return r;
        }
        virtual bool trySet(T s, T& old) override {
            auto tmp = this->load();
            bool r = this->compare_exchange_strong(tmp, s);
            prevValue.set(tmp);
            old = tmp;
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
    using PlayerStateEnum = EnumType<PlayerState>;


    enum class RequestTaskType : char {
        None = 0,
        Seek = 1,
    };
    enum class RequestHandleState {
        BeforeEnqueue,
        AfterEnqueue,
        BeforeHandle,
        AfterHandle,
    };
    enum class ThreadIdentifier {
        None = 0,
        Demuxer = 1,
        Decoder,
        Renderer,
        //VideoReadingThread = 1, // av_read_frame线程
        //VideoDecodingThread, // av_send_packet + av_receive_frame线程
        //VideoRenderingThread, // renderer渲染线程
        //AudioReadingThread, // av_read_frame线程
        //AudioDecodingThread, // av_send_packet + av_receive_frame线程
        //AudioPlayingThread, // audio播放线程
    };

    class MediaEventType {
    public:
        enum MediaEvent : int {
            None = 0,
            PlaybackStateChange,
            RequestHandle,
            Render,
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
        bool isEqual(const MediaEventType::MediaEvent& e) const {
            return type == e;
        }
        operator MediaEvent() {
            return type;
        }
        bool operator==(const MediaEventType::MediaEvent& e) const {
            return isEqual(e);
        }
        bool operator!=(const MediaEventType::MediaEvent& e) const {
            return !isEqual(e);
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
        virtual void setStreamTypes(StreamTypes type) final { this->stream = type; }
        virtual void setStreamType(StreamType type, bool enabled = true) final { if (enabled) this->stream |= type; else this->stream &= static_cast<decltype(type)>(~type); }
    protected:
        MediaEventType eventType{ MediaEventType::None };
        StreamTypes stream{ StreamType::STNone };
    public:
        MediaEvent(MediaEventType t, StreamType st) : eventType(t), stream(st) {}
        virtual ~MediaEvent() = default;
        virtual MediaEventType type() const { return this->eventType; }
        // 可能包含多种流，这取决于对象（如VideoPlayer只包含STVideo，而MediaPlayer可能包含STVideo|STAudio）
        virtual StreamTypes streamType() const { return this->stream; }
        virtual UniquePtrD<MediaEvent> clone() const {
            return std::make_unique<MediaEvent>(*this);
        }
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
        MediaPlaybackStateChangeEvent(StreamTypes st, PlayerState state, PlayerState oldState)
            : MediaEvent(MediaEventType::PlaybackStateChange, st), os(oldState), s(state) {
        }
        virtual UniquePtrD<MediaEvent> clone() const {
            return std::make_unique<MediaPlaybackStateChangeEvent>(*this);
        }
        PlayerState state() const { return s; }
        PlayerState oldState() const { return os; }
    };
    //class MediaRenderEvent : public MediaEvent {
    //    
    //public:
    //    MediaRenderEvent(StreamTypes st) : MediaEvent(MediaEventType::Render, st) {}
    //    virtual UniquePtrD<MediaEvent> clone() const {
    //        return std::make_unique<MediaRenderEvent>(*this);
    //    }
    //};

    // 请求处理事件，用于处理一些需要播放器响应的请求，如Seek等
    // 可以在请求处理前后进行拦截处理
    // 亦可以阻止请求入队
    class MediaRequestHandleEvent : public MediaEvent {
    protected:
        RequestHandleState state{ RequestHandleState::BeforeHandle };
        RequestTaskType reqType{ RequestTaskType::Seek };
        bool accepted{ true }; // 默认接受该请求
    public:
        // 默认BeforeHandle构造
        MediaRequestHandleEvent(StreamTypes st, RequestTaskType t)
            : MediaEvent(MediaEventType::RequestHandle, st), reqType(t) {
        }
        MediaRequestHandleEvent(StreamTypes st, RequestHandleState s, RequestTaskType t)
            : MediaEvent(MediaEventType::RequestHandle, st), state(s), reqType(t) {
        }
        MediaRequestHandleEvent(RequestHandleState newState, const MediaRequestHandleEvent& toClone)
            : MediaEvent(MediaEventType::RequestHandle, toClone.stream), state(newState), reqType(toClone.reqType), accepted(toClone.accepted) {
        }
        virtual RequestHandleState handleState() const { return state; }
        virtual RequestTaskType requestType() const { return reqType; }
        // 接受该请求
        virtual void accept() { accepted = true; }
        // 忽略该请求
        virtual void ignore() { accepted = false; }
        virtual void setAccepted(bool a) { accepted = a; }
        virtual bool isAccepted() const { return accepted; }
        virtual UniquePtrD<MediaEvent> clone() const {
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
        // 默认BeforeHandle构造
        MediaSeekEvent(StreamTypes st, uint64_t pts, StreamIndexType streamIndex = -1)
            : MediaRequestHandleEvent(st, RequestTaskType::Seek), pts(pts), idx(streamIndex) {}
        MediaSeekEvent(StreamTypes st, RequestHandleState handleState, uint64_t pts, StreamIndexType streamIndex = -1)
            : MediaRequestHandleEvent(st, handleState, RequestTaskType::Seek), pts(pts), idx(streamIndex) {}
        MediaSeekEvent(RequestHandleState newState, const MediaSeekEvent& toClone)
            : MediaRequestHandleEvent(newState, toClone), pts(toClone.pts), idx(toClone.idx) {}
        virtual uint64_t timestamp() const { return pts; }
        virtual StreamIndexType streamIndex() const { return idx; }
        virtual UniquePtrD<MediaEvent> clone() const override {
            return std::make_unique<MediaSeekEvent>(*this);
        }
        virtual UniquePtrD<MediaRequestHandleEvent> clone(RequestHandleState newState) const override {
            return std::make_unique<MediaSeekEvent>(newState, *this);
        }

    };

    // 用于处理一些请求任务
    struct RequestTaskItem;
    using RequestTaskProcessCallback = std::function<void(const RequestTaskItem& taskItem)>;
    //struct RequestTaskProcessCallbacks {
    //    RequestTaskProcessCallback beforeProcess;
    //    RequestTaskProcessCallback afterProcess;
    //};
    struct RequestTaskItem {
        RequestTaskType type{ RequestTaskType::None }; // 任务类型
        std::function<void(MediaRequestHandleEvent* e, std::any userData)> handler; // 处理函数
        MediaRequestHandleEvent* event; // 关联的事件对象指针
        std::any userData; // 用户数据
        std::vector<ThreadIdentifier> blockTargetThreadIds; // 需要暂停等待当前任务处理完成的线程ID列表
    };

    class ThreadStateManager;
    class RequestTaskQueueHandler {
        // 请求任务队列
        Queue<RequestTaskItem> queueRequestTasks;
        Mutex mtxQueueRequestTasks;
        ConditionVariable cvQueueRequestTasks;
        //std::function<bool(MediaEvent* e)> eventDispatcher{nullptr};
        PlayerInterface* player{ nullptr };
        AtomicBool stopped{ false };
        AtomicWaitObject<bool> waitStopped{ true };
        std::thread requestTaskHandlerThread;
        using ThreadBlocker = std::function<void(const RequestTaskItem& taskItem, std::any& userData)>;
        using ThreadAwakener = ThreadBlocker;
        struct ThreadStateHandlersContext {
            ThreadBlocker threadBlocker{ nullptr };
            ThreadAwakener threadAwakener{ nullptr };
            std::any blockerAwakenerUserData;
        };
        std::unordered_map<StreamType, ThreadStateHandlersContext> threadStateHandlersMap;
    public:
        RequestTaskQueueHandler(PlayerInterface* player = nullptr) : player(player) {}
        void addThreadStateHandlersContext(StreamType streamType, const ThreadBlocker& threadBlocker, const ThreadAwakener& threadAwakener, std::any userData = std::any{}) {
            threadStateHandlersMap.try_emplace(streamType,
                threadBlocker,
                threadAwakener,
                userData);
        }
        void removeThreadStateHandlersContext(StreamType streamType) {
            auto it = threadStateHandlersMap.find(streamType);
            if (it != threadStateHandlersMap.end())
                threadStateHandlersMap.erase(it);
        }

        void push(RequestTaskType requestType, std::vector<ThreadIdentifier> blockTargetThreadIds, MediaRequestHandleEvent* event, std::function<void(MediaRequestHandleEvent* e, std::any userData)> handler, std::any userData = std::any{});

        void reset();

        void stop() {
            notifyStop();
            waitStopped.wait(true);
        }

        void start() {
            stopped.set(false);
            requestTaskHandlerThread = std::thread(&RequestTaskQueueHandler::requestTaskHandler, this);
        }

        void waitStop() {
            if (requestTaskHandlerThread.joinable())
                requestTaskHandlerThread.join();
        }

    private:
        void notifyAll() {
            cvQueueRequestTasks.notify_all();
        }
        void notifyStop() {
            stopped.set(true);
            notifyAll();
        }
        bool shouldStop() const {
            return stopped.load();
        }
        bool eventDispatcher(MediaEvent* e);
        void requestTaskHandler();
    };

    // 打开文件的时候会查找流，并调用流选择器，用于选择需要解码的流
    // 返回true表示选择成功，outStreamIndex为选中的流索引，streamIndicesList为所有可选流索引列表
    // 如果返回false，则表示未选择任何流
    using StreamIndexSelector = std::function<bool(StreamIndexType& outStreamIndex, StreamType type, const std::span<AVStream*> streamIndicesList, const AVFormatContext* fmtCtx)>;

    enum class ComponentWorkMode {
        Internal,
        External
    };

    class ThreadStateManager {
    public:
        enum ThreadState {
            Blocking = 0,
            Blocked,
            Playing,
            Paused,
            Pausing,
        };
        class ThreadStateObj
        {
            ThreadIdentifier tid{ ThreadIdentifier::None };
            Atomic<ThreadState> state = Playing;
            ConditionVariable cv;
            Mutex mtx;
            AtomicBool noWakeUp = false;
            friend class ThreadStateController;
            friend class ThreadStateManager;
            //ThreadStateObj() {}
        public:
            ThreadStateObj(ThreadIdentifier tid) : tid(tid) {}
        };
        class ThreadStateController {
        private:
            ThreadStateObj& obj;
            SharedMutex* mtx = nullptr;
        public:
            // 无锁构造
            ThreadStateController(ThreadStateObj& s) : obj(s) {}
            // 带共享锁构造
            //ThreadStateController(ThreadStateObj& s, SharedMutex& mtx) : obj(s), mtx(&mtx) {}
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
                obj.cv.wait(lock, [&] { return obj.state != Paused; });
            }
            // 阻塞线程
            void block() {
                std::unique_lock lock(obj.mtx);
                if (obj.state != Playing && obj.state != Blocking)
                    return;
                obj.state.set(Blocked);
                obj.cv.notify_all();
                obj.cv.wait(lock, [&] { return obj.state != Blocked; });
            }
            // 设置为阻塞并等待状态变更
            void setBlockedAndWaitChanged(bool wakeUpBeforeWait = true) {
                std::unique_lock lock(obj.mtx);
                obj.state.set(Blocking);
                if (wakeUpBeforeWait)
                    obj.cv.notify_all();
                obj.cv.wait(lock, [&] { return obj.state != Blocking; });
            }
            void disableWakeUp() {
                obj.noWakeUp.store(true);
            }
            void enableWakeUp() {
                obj.noWakeUp.store(false);
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
        class AutoRemovedThreadObj {
            ThreadStateManager& tsm;
            ThreadStateController& obj;
        public:
            AutoRemovedThreadObj(ThreadStateManager& tsm, ThreadStateController& obj) : tsm(tsm), obj(obj) {}
            ~AutoRemovedThreadObj() {
                tsm.removeThread(obj.getThreadId());
            }
            ThreadStateManager& getManager() {
                return tsm;
            }
            ThreadStateController& getController() {
                return obj;
            }
        };
    private:
        //typedef AtomicWaitObject<ThreadState> WaitObject;
        using ObjsHashMap = std::unordered_map<ThreadIdentifier, ThreadStateObj>;
        ObjsHashMap mapObjs;
        SharedMutex mtxMapObjs;
    public:
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
        // 参数func决定是否继续遍历，如果返回false，则停止遍历过程
        void visit(std::function<bool(ObjsHashMap::iterator& iter, std::any& userData)> func, std::any userData) {
            std::shared_lock readLockMtxMapObjs(mtxMapObjs);
            for (auto it = mapObjs.begin(); it != mapObjs.end(); ++it)
                if (!func(it, userData))
                    break;
        }
    };
    struct DemuxerInterface;
    static void threadBlocker(Logger& logger, const std::vector<ThreadIdentifier>& blockTargetThreadIds, ThreadStateManager& threadStateManager, DemuxerInterface* demuxer, std::vector<ThreadStateManager::ThreadStateController>& outWaitObjs, bool& outDemuxerPaused);
    static void threadAwakener(std::vector<ThreadStateManager::ThreadStateController>& waitObjs, DemuxerInterface* demuxer, bool demuxerPaused);


    // 解复用器接口
    struct DemuxerInterface {
        static constexpr size_t defaultMaxPacketQueueSize = 200;
        static constexpr size_t defaultMinPacketQueueSize = 100;
        struct StreamContext {
            StreamType type{ StreamType::STNone };
            StreamIndexType index{ -1 };
            ConcurrentQueue<AVPacket*> packetQueue;
            // 包队列最大最小值
            size_t maxPacketQueueSize = defaultMaxPacketQueueSize;
            size_t minPacketQueueSize = defaultMinPacketQueueSize;
            std::function<void()> packetEnqueueCallback{ nullptr }; // 每次成功入队一个AVPacket后调用的回调函数，回调调用时将暂停解码
            StreamContext(StreamType type) : type(type) {}
        };
        virtual ~DemuxerInterface() {
            close(); // 确保关闭
        }
        virtual bool open(const std::string& url);
        virtual void close();
        virtual bool isOpen() const { return opened; }
        virtual bool findStreamInfo();
        virtual bool selectStreamsIndices(StreamTypes streamTypes, StreamIndexSelector selector) = 0;
        // 读取一个包，不论这个包是什么流类型
        virtual AVPacket* getOnePacket() = 0;
        // 让解复用器读取一个合适的包，并存入队列，可以通过参数获取到读取到的包
        virtual bool readOnePacket(AVPacket** pkt = nullptr) = 0;
        virtual void setMaxPacketQueueSize(StreamType type, size_t size) = 0;
        virtual void setMinPacketQueueSize(StreamType type, size_t size) = 0;
        virtual void flushPacketQueue(StreamType type) = 0;
        virtual void reset() = 0;
        virtual void setPacketEnqueueCallback(StreamType type, const std::function<void()>& callback) = 0;
        virtual void addStreamType(StreamType type) = 0;
        virtual bool isStreamTypeAdded(StreamType type) const = 0; // 用于判断demuxer是否已经添加该流
        virtual void removeStreamType(StreamType type) = 0;
        // 获取信息
        virtual std::string getCurrentUrl() const = 0;
        virtual AVFormatContext* getFormatContext() const = 0;
        virtual StreamTypes getStreamTypes() const = 0;
        virtual StreamTypes getOrFindStreamTypes() = 0;
        virtual StreamTypes findStreamTypes() const = 0;
        // StreamContext
        virtual StreamIndexType getStreamIndex(StreamType type) const = 0;
        virtual size_t getMaxPacketQueueSize(StreamType type) const = 0;
        virtual size_t getMinPacketQueueSize(StreamType type) const = 0;
        virtual ConcurrentQueue<AVPacket*>* getPacketQueue(StreamType type) = 0;

        // 高级api
        virtual void openAndSelectStreams(const std::string& url, StreamTypes streams, StreamIndexSelector selector);
        // 启动解复用器
        // \param stopCondition 停止条件回调函数，返回true表示需要停止解复用器
        // \param packetEnqueueCallback 每次成功入队一个AVPacket后调用的回调函数，回调调用时将暂停解码
        virtual void start() = 0; // 创建读取线程，启动解复用器
        virtual void stop() = 0; // 停止解复用器
        virtual void waitStop() = 0; // 等待解复用器停止
        // 暂停解复用器，专用于暂停解码时调用
        virtual void pause() {
            threadStateController.disableWakeUp();
            threadStateController.setBlockedAndWaitChanged(true);
        }
        // 恢复解复用器，专用于从暂停中恢复解码时调用，如果只是需要唤醒解码器请勿调用
        virtual void resume() {
            threadStateController.enableWakeUp();
            threadStateController.wakeUp();
        }
        // 唤醒解复用器，专用于唤醒解码器使用（解码器解码过多会暂时睡眠）
        virtual void wakeUp() {
            threadStateController.wakeUp();
        }

        static StreamTypes findStreamTypes(AVFormatContext* fmtCtx);
    protected:
        Logger& logger;
        DemuxerInterface(Logger& logger) : logger(logger) {}
        std::string url; // 当前打开的URL
        bool opened{ false };
        UniquePtr<AVFormatContext> formatCtx{ nullptr, constDeleterAVFormatContext };
        // 协调线程控制
        ThreadStateManager::ThreadStateObj threadStateObj{ ThreadIdentifier::Demuxer };
        ThreadStateManager::ThreadStateController threadStateController{ threadStateObj };
    };


    // 单线程兼容单媒体流解复用器
    class SingleDemuxer : public DemuxerInterface {
        const std::string loggerNameSuffix;
        DefinePlayerLoggerSinks(singleDemuxerLoggerSinks, "SingleDemuxer_" + loggerNameSuffix);
        Logger logger{ "SingleDemuxer_" + loggerNameSuffix, singleDemuxerLoggerSinks };

        StreamIndexType streamIndex{ -1 };
        StreamType streamType{ StreamType::STNone };
        ConcurrentQueue<AVPacket*> packetQueue;
        // 解复用器线程
        std::thread demuxerThread;
        // 包队列最大最小值
        size_t maxPacketQueueSize = defaultMaxPacketQueueSize;
        size_t minPacketQueueSize = defaultMinPacketQueueSize;
        std::function<void()> packetEnqueueCallback{ nullptr }; // 每次成功入队一个AVPacket后调用的回调函数，回调调用时将暂停解码

        StreamTypes foundStreamTypes{ StreamType::STNone };

        AtomicBool stopped{ false };
        AtomicWaitObject<bool> waitStopped{ true };

    public:
        explicit SingleDemuxer(const std::string& loggerNameSuffix)
            : loggerNameSuffix(loggerNameSuffix), DemuxerInterface(logger) {}
        explicit SingleDemuxer(const std::string& loggerNameSuffix, StreamType streamType)
            : loggerNameSuffix(loggerNameSuffix), DemuxerInterface(logger), streamType(streamType) {}
        ~SingleDemuxer() {
            waitStop();
            close();
        }
        // 无论流选择器是否选择了正确的流，都会遍历一遍streams参数指定的所有流类型，尽可能选择所有流对应的索引
        virtual bool selectStreamsIndices(StreamTypes streamTypes, StreamIndexSelector selector) override;
        /**/bool selectStreamIndex(StreamIndexSelector selector) { return selectStreamIndex(this->streamType, selector); }
        virtual AVPacket* getOnePacket() override;
        virtual bool readOnePacket(AVPacket** pkt = nullptr) override;
        virtual void setMaxPacketQueueSize(StreamType type, size_t size) override { if (type == streamType) maxPacketQueueSize = size; }
        virtual void setMinPacketQueueSize(StreamType type, size_t size) override { if (type == streamType) minPacketQueueSize = size; }
        /*非虚函数*/void setMaxPacketQueueSize(size_t size) { maxPacketQueueSize = size; }
        /*非虚函数*/void setMinPacketQueueSize(size_t size) { minPacketQueueSize = size; }
        virtual void flushPacketQueue(StreamType type) override { if (type == streamType) flushPacketQueue(); }
        /*非虚函数*/void flushPacketQueue();
        // 仅仅重置状态，不关闭文件，不改变maxPacketQueueSize和minPacketQueueSize
        virtual void reset() override;
        virtual void setPacketEnqueueCallback(StreamType type, const std::function<void()>& callback) override { if (type == streamType) packetEnqueueCallback = callback; }
        /*非虚函数*/void setPacketEnqueueCallback(const std::function<void()>& callback) { packetEnqueueCallback = callback; }
        // 等效于setStreamType，会在selectStreams中被覆盖，所以不建议使用
        virtual void addStreamType(StreamType type) override { streamType = type; }
        /*非虚函数*/void setStreamType(StreamType type) { streamType = type; }
        virtual bool isStreamTypeAdded(StreamType type) const override { if (type == streamType) return true; return false; }
        /**/void unsetStreamType() { streamType = StreamType::STNone; }
        /**/void removeStreamType() { streamType = StreamType::STNone; }
        virtual void removeStreamType(StreamType type) override { if (type == streamType) streamType = StreamType::STNone; }

        virtual std::string getCurrentUrl() const override { return url; }
        /*非虚函数*/StreamIndexType getStreamIndex() const { return streamIndex; }
        virtual StreamIndexType getStreamIndex(StreamType type) const override { if (type != streamType) return -1; return streamIndex; }
        virtual AVFormatContext* getFormatContext() const override { return formatCtx.get(); }
        virtual StreamTypes getStreamTypes() const override { return foundStreamTypes; }
        virtual StreamTypes getOrFindStreamTypes() override { if (foundStreamTypes == StreamType::STNone) foundStreamTypes = findStreamTypes(); return foundStreamTypes; }
        virtual StreamTypes findStreamTypes() const override { if (!opened || !formatCtx) /*未打开文件*/ return StreamType::STNone; return DemuxerInterface::findStreamTypes(formatCtx.get()); }
        virtual size_t getMaxPacketQueueSize(StreamType type) const override { if (type != streamType) return 0; return maxPacketQueueSize; }
        virtual size_t getMinPacketQueueSize(StreamType type) const override { if (type != streamType) return 0; return minPacketQueueSize; }
        virtual ConcurrentQueue<AVPacket*>* getPacketQueue(StreamType type) override { if (type != streamType) return nullptr; return &packetQueue; }
        /*非虚函数*/ConcurrentQueue<AVPacket*>& getPacketQueue() { return packetQueue; }
        // 高级api
        virtual void start() override {
            stopped.set(false);
            demuxerThread = std::thread(&SingleDemuxer::readPackets, this, packetEnqueueCallback);
        }
        virtual void stop() override {
            stopped.set(true);
            threadStateController.enableWakeUp();
            threadStateController.wakeUp();
            waitStopped.wait(true);
        }
        virtual void waitStop() override { if (demuxerThread.joinable()) demuxerThread.join(); /*等待解复用线程退出*/ }
    private:
        bool selectStreamIndex(StreamType streamType, StreamIndexSelector selector);

        bool shouldStop() const {
            return stopped.load();
        }
        void readPackets(std::function<void()> packetEnqueueCallback); // 读取包线程函数
    };

    // 一体解复用器
    class UnifiedDemuxer : public DemuxerInterface {
        const std::string loggerNameSuffix;
        DefinePlayerLoggerSinks(singleDemuxerLoggerSinks, "UnifiedDemuxer_" + loggerNameSuffix);
        Logger logger{ "UnifiedDemuxer_" + loggerNameSuffix, singleDemuxerLoggerSinks };

        std::unordered_map<StreamType, StreamContext> streamContexts;
        // 解复用器线程
        //std::unordered_map<StreamType, std::thread> demuxerThreads;
        std::thread demuxerThread;

        StreamTypes foundStreamTypes{ StreamType::STNone };

        const std::vector<StreamType> packetQueueSizeOrder{
            StreamType::STVideo,
            StreamType::STAudio,
            StreamType::STSubtitle,
            StreamType::STData,
            StreamType::STAttachment,
        };
        // 协调控制（start/waitStop)
        AtomicBool started{ false };
        AtomicBool joined{ false };
        Mutex mtxDemuxerStartStop;
        AtomicBool stopped{ false };
        AtomicWaitObject<bool> waitStopped{ true };
    public:
        explicit UnifiedDemuxer(const std::string& loggerNameSuffix)
            : loggerNameSuffix(loggerNameSuffix), DemuxerInterface(logger) {}
        explicit UnifiedDemuxer(const std::string& loggerNameSuffix, const std::vector<StreamType>& streamTypes)
            : loggerNameSuffix(loggerNameSuffix), DemuxerInterface(logger) {
            addStreamTypes(streamTypes);
        }
        ~UnifiedDemuxer() {
            waitStop();
            close();
        }
        // 无论流选择器是否选择了正确的流，都会遍历一遍streams参数指定的所有流类型，尽可能选择所有流对应的索引
        virtual bool selectStreamsIndices(StreamTypes streams, StreamIndexSelector selector) override;
        virtual bool readOnePacket(AVPacket** pkt = nullptr) override;
        virtual AVPacket* getOnePacket() override;
        virtual void setMaxPacketQueueSize(StreamType type, size_t size) override { auto it = streamContexts.find(type); if (it != streamContexts.end()) it->second.maxPacketQueueSize = size; }
        virtual void setMinPacketQueueSize(StreamType type, size_t size) override { auto it = streamContexts.find(type); if (it != streamContexts.end()) it->second.minPacketQueueSize = size; }
        virtual void flushPacketQueue(StreamType type) override;
        // 仅仅重置状态，不关闭文件，不改变maxPacketQueueSize和minPacketQueueSize
        virtual void reset() override;
        // 调用此函数需确保type流已存在，即已调用addStreamContext/调用构造函数添加该流
        virtual void setPacketEnqueueCallback(StreamType type, const std::function<void()>& callback) override {
            auto it = streamContexts.find(type);
            if (it != streamContexts.end())
                it->second.packetEnqueueCallback = callback;
        }
        virtual void addStreamType(StreamType type) override {
            //if (streamContexts.count(type)) return; // 已存在
            streamContexts.try_emplace(type, type); // key, construct arguments, ...
        }
        /*非虚函数*/void addStreamTypes(const std::vector<StreamType>& types) {
            for (auto type : types)
                addStreamType(type);
        }
        virtual bool isStreamTypeAdded(StreamType type) const override { if (streamContexts.count(type)) return true; return false; }
        virtual void removeStreamType(StreamType type) override {
            auto it = streamContexts.find(type);
            if (it != streamContexts.end())
                streamContexts.erase(it);
        }

        virtual std::string getCurrentUrl() const override { return url; }
        virtual StreamIndexType getStreamIndex(StreamType type) const override { auto it = streamContexts.find(type); if (it == streamContexts.end()) return -1; return it->second.index; }
        virtual AVFormatContext* getFormatContext() const override { return formatCtx.get(); }
        virtual StreamTypes getStreamTypes() const override { return foundStreamTypes; }
        virtual StreamTypes getOrFindStreamTypes() override { if (foundStreamTypes == StreamType::STNone) foundStreamTypes = findStreamTypes(); return foundStreamTypes; }
        virtual StreamTypes findStreamTypes() const override { if (!opened || !formatCtx) /*未打开文件*/ return StreamType::STNone; return DemuxerInterface::findStreamTypes(formatCtx.get()); }
        virtual size_t getMaxPacketQueueSize(StreamType type) const override { auto it = streamContexts.find(type); if (it != streamContexts.end()) return it->second.maxPacketQueueSize; return defaultMaxPacketQueueSize; }
        virtual size_t getMinPacketQueueSize(StreamType type) const override { auto it = streamContexts.find(type); if (it != streamContexts.end()) return it->second.minPacketQueueSize; return defaultMinPacketQueueSize; }
        virtual ConcurrentQueue<AVPacket*>* getPacketQueue(StreamType type) override { auto it = streamContexts.find(type); if (it != streamContexts.end()) return &it->second.packetQueue; return nullptr; }
        // 高级api
        // 创建读取线程，启动解复用器，仅初次调用有效，直到线程结束
        virtual void start() override {
            std::lock_guard lock(mtxDemuxerStartStop);
            if (started.load()) return;
            stopped.set(false);
            started.store(true);
            joined.store(false);
            //for (auto& [key, streamCtx] : streamContexts)
            //    demuxerThreads.try_emplace(key, &UnifiedDemuxer::readPackets, this, &streamCtx, stopCondition);
            if (demuxerThread.joinable()) // 如果之前的线程还在运行，先等待其结束
            {
                stopNoLock();
                demuxerThread.join();
            }
            demuxerThread = std::thread(
                //static_cast<void(UnifiedDemuxer::*)(std::function<bool()>)>(&UnifiedDemuxer::readPackets), this, stopCondition
                [this]() {
                    this->readPackets();
                    std::unique_lock lock(mtxDemuxerStartStop);
                    started.store(false);
                }
            );
        }
        virtual void stop() override {
            std::lock_guard lock(mtxDemuxerStartStop);
            stopNoLock();
        }
        // 仅初次调用有效，直到线程结束
        virtual void waitStop() override {
            std::unique_lock lock(mtxDemuxerStartStop);
            if (joined.load()) return;
            joined.store(true);
            lock.unlock();
            //for (auto& [key, demuxerThread] : demuxerThreads)
            if (demuxerThread.joinable())
                demuxerThread.join(); // 等待解复用线程退出
            lock.lock();
            started.store(false);
            joined.store(false);
        }
    private:
        bool shouldStop() const {
            return stopped.load();
        }
        void stopNoLock() {
            if (!started.load()) return;
            stopped.set(true);
            threadStateController.enableWakeUp();
            threadStateController.wakeUp();
            waitStopped.wait(true);
        }

        ///*非虚函数*/void readPackets(StreamContext* streamCtx, std::function<bool()> stopCondition); // 读取包线程函数
        /*非虚函数*/void readPackets(); // 读取包线程函数
        void resetStreamContexts() {
            for (auto& [key, streamCtx] : streamContexts) {
                ConcurrentQueue<AVPacket*> tmpQueue;
                streamCtx.packetQueue.swap(tmpQueue);
                streamCtx.index = -1;
            }
        }
    };

    struct FrameFilter {
        virtual void filter(AVFrame* frame) = 0;
        FrameFilter() = default;
        virtual ~FrameFilter() = default;
    private:
        // 禁止拷贝与赋值
        FrameFilter(const FrameFilter&) = delete;
        FrameFilter& operator=(const FrameFilter&) = delete;
    };

    class FrameVolumeFilter : public FrameFilter {
        AVCodecContext* codecCtx{ nullptr };
        AtomicDouble vol{ 1.0 }; // 音量: [0.0, 1.0]
        UniquePtr<AVFilterContext> srcCtx{ nullptr };
        UniquePtr<AVFilterContext> volumeCtx{ nullptr };
        UniquePtr<AVFilterContext> sinkCtx{ nullptr };
    public:
        FrameVolumeFilter(AVCodecContext* codecCtx, double volume = 1.0) : codecCtx(codecCtx), vol(volume) {}
        ~FrameVolumeFilter() = default;
        virtual void filter(AVFrame* frame) override;
        virtual void setVolume(double vol) {
            if (vol < 0.0) vol = 0.0;
            if (vol > 100.0) vol = 100.0;
            this->vol.store(vol);
        }
        virtual double volume() const {
            return vol.load();
        }
    };
};

class MediaDecodeUtils : public PlayerTypes
{
private:
public:
    static bool openFile(Logger* logger, AVFormatContext*& fmtCtx, const std::string& filePath);
    static bool openFile(Logger* logger, UniquePtr<AVFormatContext>& fmtCtx, const std::string& filePath);
    static void closeFile(Logger* logger, AVFormatContext*& fmtCtx);
    static void closeFile(Logger* logger, UniquePtr<AVFormatContext>& fmtCtx);
    static bool findStreamInfo(Logger* logger, AVFormatContext* formatCtx);
    static bool readFrame(Logger* logger, AVFormatContext* fmtCtx, AVPacket*& packet, bool allocPacket = true, bool* isEof = nullptr);
    static bool findAndOpenAudioDecoder(Logger* logger, AVFormatContext* formatCtx, StreamIndexType streamIndex, UniquePtr<AVCodecContext>& codecContext);
    static bool findAndOpenVideoDecoder(Logger* logger, AVFormatContext* formatCtx, StreamIndexType streamIndex, UniquePtr<AVCodecContext>& codecContext, bool useHardwareDecoder = false, AVHWDeviceType* hwDeviceType = nullptr, AVPixelFormat* hwPixelFormat = nullptr);
    static void listAllHardwareDecoders(Logger* logger);
    // fromType 表示从AVHWDeviceType的哪一个的下一个开始遍历查找
    // hwDeviceType与hwPixelFormat为输出
    static AVHWDeviceType findHardwareDecoder(Logger* logger, const AVCodec* codec, AVHWDeviceType fromType, AVHWDeviceType& hwDeviceType, AVPixelFormat& hwPixelFormat);
    // 会初始化并设置codecCtx的hw_device_ctx成员
    static bool initHardwareDecoder(Logger* logger, AVCodecContext* codecCtx, AVHWDeviceType hwDeviceType);
};

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
    friend class PlayerTypes;
private:
    void playbackStateChangeEventHandler(MediaPlaybackStateChangeEvent* e);
    void requestHandleEventHandler(MediaRequestHandleEvent* e);
public:
    struct PlayerContext {
        // 解复用器
        StreamType demuxerStreamType{ StreamType::STVideo };
        SharedPtr<DemuxerInterface> demuxer{ nullptr };
        AVFormatContext* formatCtx{ nullptr };
        StreamIndexType streamIndex{ -1 };
        ConcurrentQueue<AVPacket*>* packetQueue{ nullptr };

        ConcurrentQueue<AVFrame*> frameQueue;
        UniquePtr<AVCodecContext> codecCtx{ nullptr, constDeleterAVCodecContext };

        // 时钟
        AtomicDouble clock{ 0.0 }; // 单位s
        AtomicBool isClockStable{ false }; // 时钟是否稳定
        // 系统时钟
        double realtimeClock{ 0.0 };
    };
    //void requestTaskHandlerSeek(
    //    AVFormatContext* formatCtx, std::vector<AVCodecContext*> codecCtxList,
    //    std::function<void(PlayerState)> playerStateSetter, std::function<void()> clearPktAndFrameQueuesFunc,
    //    MediaRequestHandleEvent* e, std::any userData);
public:
    PlayerInterface(Logger& logger) : logger(logger) {}
    virtual ~PlayerInterface() = default;
    virtual bool play() = 0;
    virtual void resume() = 0;
    virtual void pause() = 0;
    virtual void notifyStop() = 0;
    virtual void stop() = 0;
    virtual void setMute(bool state) = 0;
    virtual bool getMute() const = 0;
    // volume range: [0.0, 1.0]
    virtual void setVolume(double volume) = 0;
    virtual double getVolume() const = 0;
    virtual void notifySeek(uint64_t pts, StreamIndexType streamIndex = -1) = 0;
    virtual void seek(uint64_t pts, StreamIndexType streamIndex = -1) = 0;
    virtual bool isPlaying() const = 0;
    virtual bool isPaused() const = 0;
    virtual bool isStopped() const = 0;
    virtual PlayerState getPlayerState() const = 0;
    virtual void setFilePath(const std::string& filePath) = 0;
    virtual std::string getFilePath() const = 0;
    //virtual void setStreamIndexSelector(const StreamIndexSelector& selector) = 0;
    //virtual void setVolume(double volume) = 0;
    virtual void setDemuxerMode(ComponentWorkMode mode) = 0;
    virtual ComponentWorkMode getDemuxerMode() const = 0;
    virtual void setExternalDemuxer(const SharedPtr<UnifiedDemuxer>& demuxer) = 0;
    virtual void setRequestTaskQueueHandlerMode(ComponentWorkMode mode) = 0;
    virtual ComponentWorkMode getRequestTaskQueueHandlerMode() const = 0;
    virtual void setExternalRequestTaskQueueHandler(const SharedPtr<RequestTaskQueueHandler>& handler) = 0;

    // 方法一
    //virtual void addEventHandler(size_t handlerId, std::function<void(const MediaEventType& eventType, const std::any& eventData)> handler) = 0;
    //virtual void removeEventHandler(size_t handlerId) = 0;

    // 方法二
protected:
    // 事件分发入口
    virtual bool event(MediaEvent* e);
    // 播放状态改变事件（重写该方法不会影响其子事件（start,pause,resume,stop）的事件分发）
    virtual void playbackStateChangeEvent(MediaPlaybackStateChangeEvent* e) {}
    // 已经开始播放事件（启动渲染线程前，不保证是否已经启动解码线程），隶属于播放状态改变事件
    virtual void startEvent(MediaPlaybackStateChangeEvent* e) {}
    // 暂停播放事件，隶属于播放状态改变事件
    virtual void pauseEvent(MediaPlaybackStateChangeEvent* e) {}
    // 恢复播放事件，隶属于播放状态改变事件
    virtual void resumeEvent(MediaPlaybackStateChangeEvent* e) {}
    // 播放已经停止事件，隶属于播放状态改变事件
    virtual void stopEvent(MediaPlaybackStateChangeEvent* e) {}
    //virtual void renderEvent(MediaRenderEvent* e) {}
    // 请求处理事件（重写该方法不会影响其子事件（seek）的事件分发）
    virtual void requestHandleEvent(MediaRequestHandleEvent* e) {}
    // seek进度调整事件，隶属于请求处理事件
    virtual void seekEvent(MediaSeekEvent* e) {}

};


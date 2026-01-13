#include "PlayerPredefine.h"

inline PlayerTypes::AVCodecContextConstDeleter PlayerTypes::constDeleterAVCodecContext = [](AVCodecContext* ctx) { if (ctx) avcodec_free_context(&ctx); };
inline PlayerTypes::AVPacketConstDeleter PlayerTypes::constDeleterAVPacket = [](AVPacket* pkt) { if (pkt) av_packet_free(&pkt); };
inline PlayerTypes::AVFrameConstDeleter PlayerTypes::constDeleterAVFrame = [](AVFrame* frame) { if (frame) av_frame_free(&frame); };
inline PlayerTypes::AVFormatContextConstDeleter PlayerTypes::constDeleterAVFormatContext = [](AVFormatContext* ctx) { if (ctx) avformat_close_input(&ctx); };

#define EnumValueToStringCase(prefix, enumValue) \
    case prefix::enumValue: return #enumValue

#define MediaEventTypeToStringCase(event) \
    EnumValueToStringCase(MediaEventType, event)

std::string PlayerTypes::MediaEventType::name() const
{
	switch (type)
	{
	MediaEventTypeToStringCase(None);
	MediaEventTypeToStringCase(PlaybackStateChange);
	MediaEventTypeToStringCase(RequestHandle);
	default:
		break;
	}
	return std::string{};
}

std::string PlayerTypes::PlayerStateEnum::getName(PlayerTypes::PlayerState value) {
    switch (value)
    {
        EnumValueToStringCase(PlayerTypes::PlayerState, Stopped);
        EnumValueToStringCase(PlayerTypes::PlayerState, Paused);
        EnumValueToStringCase(PlayerTypes::PlayerState, Playing);
        EnumValueToStringCase(PlayerTypes::PlayerState, Stopping);
        EnumValueToStringCase(PlayerTypes::PlayerState, Preparing);
        EnumValueToStringCase(PlayerTypes::PlayerState, Seeking);
    default:
        break;
    }
    return std::string{};
}
std::string PlayerTypes::PlayerStateEnum::name() const {
    return getName(value());
}

void PlayerInterface::playbackStateChangeEventHandler(MediaPlaybackStateChangeEvent* e)
{
    auto&& s = e->state();
    auto&& os = e->oldState();
    if (s == PlayerState::Playing)
    {
        if (os == PlayerState::Stopping || os == PlayerState::Stopped) // 从停止状态开始播放
            startEvent(e);
        else if (os == PlayerState::Preparing) // 从准备状态开始播放
            startEvent(e);
        else if (os == PlayerState::Pausing || os == PlayerState::Paused) // 从暂停状态恢复播放
            resumeEvent(e);
    }
    else if (s == PlayerState::Paused)
        pauseEvent(e);
    else if (s == PlayerState::Stopped)
        stopEvent(e);
}

void PlayerInterface::requestHandleEventHandler(MediaRequestHandleEvent* e)
{
    std::string strHandleState;
    switch (e->handleState())
    {
    case RequestHandleState::BeforeEnqueue:
        strHandleState = "BeforeEnqueue";
        break;
    case RequestHandleState::AfterEnqueue:
        strHandleState = "AfterEnqueue";
        break;
    case RequestHandleState::BeforeHandle:
        strHandleState = "BeforeHandle";
        break;
    case RequestHandleState::AfterHandle:
        strHandleState = "AfterHandle";
        break;
    default:
        strHandleState = "Unknown"; // should not happen
        break;
    }
    if (e->requestType() == RequestTaskType::Seek)
    {
        auto&& se = static_cast<MediaSeekEvent*>(e);
        logger.trace("Request: Seek triggered, request handle state: {}, seek to timestamp {} base on timebase of streamIndex {}.", strHandleState, se->timestamp(), se->streamIndex());
        seekEvent(se);
    }
    else
    {
        logger.trace("Request: {} triggered, request handle state: {}.", static_cast<std::underlying_type_t<decltype(e->requestType())>>(e->requestType()), strHandleState);
    }
}

bool PlayerInterface::event(MediaEvent* e)
{
    switch (e->type())
    {
    case MediaEventType::PlaybackStateChange:
        // 先处理小事件，如果不满足则分发到大事件
        playbackStateChangeEventHandler(static_cast<MediaPlaybackStateChangeEvent*>(e));
        playbackStateChangeEvent(static_cast<MediaPlaybackStateChangeEvent*>(e));
        return true;
    case MediaEventType::RequestHandle:
        // 先处理小事件，如果不满足则分发到大事件
        requestHandleEventHandler(static_cast<MediaRequestHandleEvent*>(e));
        requestHandleEvent(static_cast<MediaRequestHandleEvent*>(e));
        return true;
    case MediaEventType::None:
    default:
        break;
    }
    return true;
}


bool MediaDecodeUtils::openFile(Logger* logger, AVFormatContext*& fmtCtx, const std::string& filePath)
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
bool MediaDecodeUtils::openFile(Logger* logger, UniquePtr<AVFormatContext>& fmtCtx, const std::string& filePath)
{
    AVFormatContext* p = nullptr;
    bool rst = openFile(logger, p, filePath);
    fmtCtx.reset(p);
    return rst;
}
void MediaDecodeUtils::closeFile(Logger* logger, AVFormatContext*& fmtCtx)
{
    if (fmtCtx)
    {
        avformat_close_input(&fmtCtx);
        fmtCtx = nullptr;
    }
}
void MediaDecodeUtils::closeFile(Logger* logger, UniquePtr<AVFormatContext>& fmtCtx)
{
    if (fmtCtx)
    {
        //auto* ctx = fmtCtx.get();
        //closeFile(logger, ctx);
        fmtCtx.reset();
    }
}
bool MediaDecodeUtils::findStreamInfo(Logger* logger, AVFormatContext* formatCtx)
{
    if (avformat_find_stream_info(formatCtx, nullptr) < 0)
    {
        logger->error("Cannot find stream info.");
        return false;
    }
    return true;
}
bool MediaDecodeUtils::readFrame(Logger* logger, AVFormatContext* fmtCtx, AVPacket*& packet, bool allocPacket, bool* isEof)
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
bool MediaDecodeUtils::findAndOpenAudioDecoder(Logger* logger, AVFormatContext* formatCtx, StreamIndexType streamIndex, UniquePtr<AVCodecContext>& codecContext)
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
bool MediaDecodeUtils::findAndOpenVideoDecoder(Logger* logger, AVFormatContext* formatCtx, StreamIndexType streamIndex, UniquePtr<AVCodecContext>& codecContext, bool useHardwareDecoder, AVHWDeviceType* hwDeviceType, AVPixelFormat* hwPixelFormat)
{
    if (streamIndex < 0)
    {
        logger->error("Stream index is negative");
        return false;
    }
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
void MediaDecodeUtils::listAllHardwareDecoders(Logger* logger)
{
    // 列举所有支持的硬件解码器类型
    AVHWDeviceType type = AV_HWDEVICE_TYPE_NONE;
    logger->info("Find hardware decoders:");
    for (size_t i = 1; (type/*current type*/ = av_hwdevice_iterate_types(type/*previous type*/)) != AV_HWDEVICE_TYPE_NONE; ++i)
    {
        logger->info("\t decoder {}: {}", i, av_hwdevice_get_type_name(type));
    }
}
AVHWDeviceType MediaDecodeUtils::findHardwareDecoder(Logger* logger, const AVCodec* codec, AVHWDeviceType fromType, AVHWDeviceType& hwDeviceType, AVPixelFormat& hwPixelFormat)
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
bool MediaDecodeUtils::initHardwareDecoder(Logger* logger, AVCodecContext* codecCtx, AVHWDeviceType hwDeviceType)
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


PlayerTypes::StreamTypes PlayerTypes::DemuxerInterface::findStreamTypes(AVFormatContext* formatCtx)
{
    StreamTypes rst = StreamType::STNone;
    // 查找视频流和音频流
    for (size_t i = 0; i < formatCtx->nb_streams; ++i)
    {
        AVCodecParameters* codecPar = formatCtx->streams[i]->codecpar;
        switch (codecPar->codec_type)
        {
        case AVMEDIA_TYPE_VIDEO:
            rst |= StreamType::STVideo;
            break;
        case AVMEDIA_TYPE_AUDIO:
            rst |= StreamType::STAudio;
            break;
        case AVMEDIA_TYPE_SUBTITLE:
            rst |= StreamType::STSubtitle;
            break;
        case AVMEDIA_TYPE_ATTACHMENT:    ///< Opaque data information usually sparse
            rst |= StreamType::STAttachment;
            break;
        case AVMEDIA_TYPE_UNKNOWN:  ///< Usually treated as AVMEDIA_TYPE_DATA
        case AVMEDIA_TYPE_DATA:          ///< Opaque data information usually continuous
            rst |= StreamType::STData;
            break;
        default:
            break;
        case AVMEDIA_TYPE_NB: // Not part of ABI, 仅用于统计枚举值数量
            break;
        }
    }
    return rst;
}

bool PlayerTypes::DemuxerInterface::open(const std::string& url)
{
    if (opened)
    {
        close();
        opened = false;
    }
    this->url = url;
    return opened = MediaDecodeUtils::openFile(&logger, formatCtx, url);
}
void PlayerTypes::DemuxerInterface::close()
{
    MediaDecodeUtils::closeFile(&logger, formatCtx); opened = false;
}
bool PlayerTypes::DemuxerInterface::findStreamInfo()
{
    return MediaDecodeUtils::findStreamInfo(&logger, formatCtx.get());
}
void PlayerTypes::DemuxerInterface::openAndSelectStreams(const std::string& url, StreamTypes streams, StreamIndexSelector selector)
{
    if (!open(url))
        throw std::runtime_error("Failed to open media file: " + url);
    if (!findStreamInfo())
        throw std::runtime_error("Failed to find stream info: " + url);
    if (!selectStreamsIndices(streams, selector))
        throw std::runtime_error("Failed to find and select streams: " + url);
}


bool PlayerTypes::SingleDemuxer::selectStreamsIndices(StreamTypes streams, StreamIndexSelector selector)
{
    if (!selector)
        return false;
    this->streamIndex = -1;
    bool result = true;
    streamTypesVisit(streams, [this, &result, &selector](StreamType streamType, std::any) -> bool {
        if (this->streamType != streamType)
            return true; // 只选择指定类型的流
        result = selectStreamIndex(streamType, selector);
        return false; // 返回true继续遍历其他流类型，否则停止遍历
        });
    return result;
}
bool PlayerTypes::SingleDemuxer::selectStreamIndex(StreamType streamType, StreamIndexSelector selector)
{
    // 保存当前流类型
    std::span<AVStream*> streamsSpan(this->formatCtx->streams, this->formatCtx->nb_streams);
    StreamIndexType si = -1;
    this->streamIndex = -1; // 重置流索引
    bool rst = selector(si, streamType, streamsSpan, this->formatCtx.get());
    if (rst && si >= 0 && si < static_cast<StreamIndexType>(this->formatCtx->nb_streams)
        && streamsSpan[si]->codecpar->codec_type == streamTypeToAVMediaType(streamType)
        ) {
        this->streamIndex = si; // 检查选择的流索引是否合理，如果合理则选择该流
        return true;
    }
    else if (this->getStreamTypes().contains(streamType))
        return false; // 只有找到了该流类型但选择失败时，才返回false，如果是没找到该流类型则忽略
    return false;
}

AVPacket* PlayerTypes::SingleDemuxer::getOnePacket()
{
    AVPacket* pkt = nullptr;
    if (!MediaDecodeUtils::readFrame(&logger, formatCtx.get(), pkt, true))
    {
        if (pkt) av_packet_free(&pkt); // 释放包
        logger.trace("Read frame finished.");
        return nullptr;
    }
    return pkt;
}
bool PlayerTypes::SingleDemuxer::readOnePacket(AVPacket** outPkt)
{
    AVPacket* pkt = nullptr;
    while (true)
    {
        pkt = getOnePacket();
        if (!pkt) return false;
        if (pkt->stream_index == streamIndex) break;
        av_packet_free(&pkt); // 释放不需要的包
    }
    if (outPkt) *outPkt = pkt;
    auto oldPktQueueSize = ConcurrentQueueOps::getQueueSize(packetQueue);
    ConcurrentQueueOps::enqueue(packetQueue, pkt);
    //auto pktQueueSize = getQueueSize(playbackStateVariables.packetQueue);
    logger.trace("Pushed audio packet, queue size: {}", oldPktQueueSize + 1);
    packetEnqueueCallback();
    return true;
}

void PlayerTypes::SingleDemuxer::flushPacketQueue()
{
    AVPacket* pkt = nullptr;
    while (ConcurrentQueueOps::tryDequeue(packetQueue, pkt))
        av_packet_free(&pkt);
}
void PlayerTypes::SingleDemuxer::reset()
{
    flushPacketQueue();
    streamIndex = -1;
    streamType = StreamType::STNone;
    foundStreamTypes = StreamType::STNone;
}
void PlayerTypes::SingleDemuxer::readPackets(std::function<void()> packetEnqueueCallback)
{
    waitStopped.set(false);
    // 视频解码和显示循环
    while (true)
    {
        if (threadStateController.isBlocking())
            threadStateController.block();

        if (shouldStop())
            break;

        auto oldPktQueueSize = ConcurrentQueueOps::getQueueSize(packetQueue);
        if (oldPktQueueSize >= maxPacketQueueSize)
        {
            threadStateController.pause();
            continue;
        }
        // Read frame from the format context 读取帧
        AVPacket* pkt = nullptr;
        if (!MediaDecodeUtils::readFrame(&logger, formatCtx.get(), pkt, true))
        {
            if (pkt) av_packet_free(&pkt); // 释放包
            logger.trace("Read frame finished.");
            //break; // 读取结束，退出循环
            // 读取结束，暂停线程，等待通知
            threadStateController.pause();
            continue;
        }
        if (pkt->stream_index != streamIndex)
        {
            av_packet_free(&pkt); // 释放不需要的包
            continue;
        }
        ConcurrentQueueOps::enqueue(packetQueue, pkt);
        //auto pktQueueSize = getQueueSize(playbackStateVariables.packetQueue);
        logger.trace("Pushed audio packet, queue size: {}", oldPktQueueSize + 1);
        packetEnqueueCallback();
    }
    waitStopped.setAndNotifyAll(true);
}


bool PlayerTypes::UnifiedDemuxer::selectStreamsIndices(StreamTypes streams, StreamIndexSelector selector)
{
    if (!selector)
        return false;
    // 清空上下文中保存的索引
    resetStreamContexts();
    bool result = true;
    streamTypesVisit(streams, [this, &result, &selector](StreamType streamType, std::any) -> bool {
        // 保存当前流类型
        std::span<AVStream*> streamsSpan(this->formatCtx->streams, this->formatCtx->nb_streams);
        StreamIndexType si = -1;
        //auto& sctx = this->streamContexts.at(streamType);
        //sctx.index = -1; // 重置流索引
        bool rst = selector(si, streamType, streamsSpan, this->formatCtx.get());
        if (rst && si >= 0 && si < static_cast<StreamIndexType>(this->formatCtx->nb_streams)
            && streamsSpan[si]->codecpar->codec_type == streamTypeToAVMediaType(streamType)
            ) {
            auto& sctx = this->streamContexts.at(streamType);
            sctx.type = streamType;
            sctx.index = si; // 检查选择的流索引是否合理，如果合理则选择该流
        }
        else if (this->getStreamTypes().contains(streamType))
            result = false; // 只有找到了该流类型但选择失败时，才返回false，如果是没找到该流类型则忽略
        return true; // 返回true继续遍历其他流类型
        });
    return result;
}
AVPacket* PlayerTypes::UnifiedDemuxer::getOnePacket()
{
    AVPacket* pkt = nullptr;
    if (!MediaDecodeUtils::readFrame(&logger, formatCtx.get(), pkt, true))
    {
        if (pkt) av_packet_free(&pkt); // 释放包
        logger.trace("Read frame finished.");
        return nullptr;
    }
    return pkt;
}

bool PlayerTypes::UnifiedDemuxer::readOnePacket(AVPacket** outPkt)
{
    AVPacket* pkt = nullptr;
    while (true)
    {
        pkt = getOnePacket();
        if (!pkt) return false;
        // 检测当前包是否符合已选择的流类型
        bool foundStreamContext = false;
        for (auto& [stype, sctx] : streamContexts)
        {
            if (pkt->stream_index != sctx.index)
                continue;
            foundStreamContext = true;
            if (outPkt) *outPkt = pkt;
            // 找到对应流类型，放入对应队列
            auto oldPktQueueSize = ConcurrentQueueOps::getQueueSize(sctx.packetQueue);
            if (oldPktQueueSize >= sctx.maxPacketQueueSize)
                threadStateController.pause(); // 暂停当前流的读取线程
            ConcurrentQueueOps::enqueue(sctx.packetQueue, pkt);
            logger.trace("Pushed audio packet, queue size: {}", oldPktQueueSize + 1);
            sctx.packetEnqueueCallback();
            break;
        }
        if (foundStreamContext)
            return true;
        av_packet_free(&pkt); // 释放不需要的包
    }
    return false;
}


void PlayerTypes::UnifiedDemuxer::flushPacketQueue(StreamType streamType)
{
    if (!streamContexts.count(streamType))
        return;
    AVPacket* pkt = nullptr;
    while (ConcurrentQueueOps::tryDequeue(streamContexts.at(streamType).packetQueue, pkt))
        av_packet_free(&pkt);
}
void PlayerTypes::UnifiedDemuxer::reset()
{
    foundStreamTypes = StreamType::STNone;
    streamContexts.clear();
}
//void PlayerTypes::UnifiedDemuxer::readPackets(StreamContext* streamContext, std::function<bool()> stopCondition)
//{
//    auto& streamIndex = streamContext->index;
//    auto& threadStateManager = streamContext->threadStateManager;
//    auto& packetQueue = streamContext->packetQueue;
//    auto& maxPacketQueueSize = streamContext->maxPacketQueueSize;
//    auto& minPacketQueueSize = streamContext->minPacketQueueSize;
//    auto&& waitObj = threadStateManager->addThread(ThreadIdentifier::ReadingThread);
//    ThreadStateManager::AutoRemovedThreadObj autoRemoveWaitObj{ *threadStateManager, waitObj };
//
//    // 视频解码和显示循环
//    while (true)
//    {
//        if (waitObj.isBlocking())
//            waitObj.block();
//
//        if (stopCondition())
//            break;
//
//        auto oldPktQueueSize = ConcurrentQueueOps::getQueueSize(packetQueue);
//        if (oldPktQueueSize >= maxPacketQueueSize)
//        {
//            waitObj.pause();
//            continue;
//        }
//        // Read frame from the format context 读取帧
//        AVPacket* pkt = nullptr;
//        if (!MediaDecodeUtils::readFrame(&logger, formatCtx.get(), pkt, true))
//        {
//            if (pkt)
//                av_packet_free(&pkt); // 释放包
//            logger.trace("Read frame finished.");
//            //break; // 读取结束，退出循环
//            // 读取结束，暂停线程，等待通知
//            waitObj.pause();
//            continue;
//        }
//        if (pkt->stream_index != streamIndex)
//        {
//            av_packet_free(&pkt); // 释放不需要的包
//            continue;
//        }
//        ConcurrentQueueOps::enqueue(packetQueue, pkt);
//        //auto pktQueueSize = getQueueSize(packetQueue);
//        logger.trace("Pushed audio packet, queue size: {}", oldPktQueueSize + 1);
//        if (oldPktQueueSize + 1 <= minPacketQueueSize)
//            threadStateManager->wakeUpById(ThreadIdentifier::DecodingThread);
//    }
//}
void PlayerTypes::UnifiedDemuxer::readPackets()
{
    waitStopped.set(false);
    // 视频解码和显示循环
    while (true)
    {
        if (threadStateController.isBlocking())
            threadStateController.block();

        if (shouldStop())
            break;

        // Read frame from the format context 读取帧
        AVPacket* pkt = nullptr;
        if (!MediaDecodeUtils::readFrame(&logger, formatCtx.get(), pkt, true))
        {
            if (pkt) av_packet_free(&pkt); // 释放包
            logger.trace("Read frame finished.");
            //break; // 读取结束，退出循环
            // 读取结束，暂停线程，等待通知
            threadStateController.pause();
            continue;
        }
        bool foundStreamContext = false;
        for (auto& [stype, sctx] : streamContexts)
        {
            if (pkt->stream_index != sctx.index)
                continue;
            // 找到对应流类型，放入对应队列
            foundStreamContext = true;
            auto oldPktQueueSize = ConcurrentQueueOps::getQueueSize(sctx.packetQueue);
            if (oldPktQueueSize >= sctx.maxPacketQueueSize)
                threadStateController.pause(); // 暂停当前流的读取线程
            ConcurrentQueueOps::enqueue(sctx.packetQueue, pkt);
            logger.trace("Pushed audio packet, queue size: {}", oldPktQueueSize + 1);
            sctx.packetEnqueueCallback();
            break;
        }
        if (!foundStreamContext)
        {
            av_packet_free(&pkt); // 释放不需要的包
            continue;
        }
    }
    waitStopped.setAndNotifyAll(true);
}

void PlayerTypes::RequestTaskQueueHandler::push(RequestTaskType type, std::vector<ThreadIdentifier> blockTargetThreadIds, MediaRequestHandleEvent* event, std::function<void(MediaRequestHandleEvent* e, std::any userData)> handler, std::any userData)
{
    std::unique_lock lockMtxQueueRequestTasks(mtxQueueRequestTasks); // 独占锁
    auto&& beforeEnqueueEvent = event->clone(RequestHandleState::BeforeEnqueue);
    eventDispatcher(beforeEnqueueEvent.get()); // 触发入队前事件
    if (beforeEnqueueEvent->isAccepted())
    {
        queueRequestTasks.emplace(type, handler, event, userData, blockTargetThreadIds); // 构造入队
        auto&& afterEnqueueEvent = event->clone(RequestHandleState::AfterEnqueue);
        eventDispatcher(afterEnqueueEvent.get()); // 触发入队后事件
    }
    // 记得通知处于等待的请求任务处理线程
    cvQueueRequestTasks.notify_all();
}

void PlayerTypes::RequestTaskQueueHandler::reset()
{
    std::unique_lock lockMtxQueueRequestTasks(mtxQueueRequestTasks);
    Queue<RequestTaskItem> emptyQueue;
    std::swap(queueRequestTasks, emptyQueue);
}

bool PlayerTypes::RequestTaskQueueHandler::eventDispatcher(MediaEvent* e)
{
    if (player)
        return player->event(e);
    return true;
}

void PlayerTypes::RequestTaskQueueHandler::requestTaskHandler()
{
    waitStopped.set(false);
    while (true)
    {
        std::unique_lock lockMtxQueueRequestTasks(mtxQueueRequestTasks);
        while (queueRequestTasks.empty())
        {
            if (shouldStop())
                break;
            cvQueueRequestTasks.wait(lockMtxQueueRequestTasks); // 等待有任务到来
        }
        if (shouldStop())
            break;
        // 处理任务
        RequestTaskItem taskItem = queueRequestTasks.front();
        queueRequestTasks.pop(); // 先弹出任务
        // 构造智能指针，自动释放事件对象内存
        UniquePtrD<MediaRequestHandleEvent> smartEvent{ taskItem.event };
        lockMtxQueueRequestTasks.unlock();
        // 通知需要等待的线程
        for (auto& [streamType, handlers] : threadStateHandlersMap)
            handlers.threadBlocker(taskItem, handlers.blockerAwakenerUserData);
        //std::vector<ThreadStateManager::ThreadStateController> waitObjs;
        //for (const auto& threadId : taskItem.blockTargetThreadIds)
        //{
        //    try {
        //        auto&& tsc = threadStateManager.get(threadId);
        //        waitObjs.push_back(tsc);
        //        tsc.disableWakeUp();
        //        tsc.setBlockedAndWaitChanged(true);
        //    }
        //    catch (std::exception e) {
        //        player->logger.error("{}", e.what());
        //    }
        //}
        // 执行任务处理函数
        auto&& requestBeforeHandleEvent = taskItem.event->clone(RequestHandleState::BeforeHandle);
        eventDispatcher(requestBeforeHandleEvent.get());
        if (requestBeforeHandleEvent->isAccepted()) // 只有被接受的任务才处理
        {
            if (taskItem.handler)
                taskItem.handler(taskItem.event, taskItem.userData);
        }
        auto&& requestAfterHandleEvent = taskItem.event->clone(RequestHandleState::AfterHandle);
        eventDispatcher(requestAfterHandleEvent.get());
        // 通知所有暂停的线程继续运行
        for (auto& [streamType, handlers] : threadStateHandlersMap)
            handlers.threadAwakener(taskItem, handlers.blockerAwakenerUserData);
        //for (auto& waitObj : waitObjs)
        //{
        //    waitObj.enableWakeUp();
        //    waitObj.wakeUp(); // 唤醒线程
        //}
    }
    waitStopped.setAndNotifyAll(true);
}


//void PlayerInterface::requestTaskHandlerSeek(AVFormatContext* formatCtx, std::vector<AVCodecContext*> codecCtxList,
//    std::function<void(PlayerState)> playerStateSetter, std::function<void()> clearPktAndFrameQueuesFunc,
//    MediaRequestHandleEvent* e, std::any userData)
//{
//    // 先将播放器状态调整至非播放中状态
//    playerStateSetter(PlayerState::Seeking);
//    // 此时已经所有相关线程均已暂停
//    auto* seekEvent = static_cast<MediaSeekEvent*>(e);
//    uint64_t pts = seekEvent->timestamp();
//    StreamIndexType streamIndex = seekEvent->streamIndex();
//    //int rst = avformat_seek_file(playbackStateVariables.formatCtx.get(), streamIndex, INT64_MIN, pts, INT64_MAX, 0);
//    //if (rst < 0) // 寻找失败
//    //{
//    //    logger.error("Error seeking to pts: {} in stream index: {}", pts, streamIndex);
//    //    return;
//    //}
//    int rst = av_seek_frame(formatCtx, streamIndex, pts, AVSEEK_FLAG_BACKWARD);
//    if (rst < 0) // 寻找失败
//    {
//        logger.error("Error video seeking to pts: {} in stream index: {}, duration: {}", pts, streamIndex, formatCtx->duration);
//        // 恢复播放状态
//        playerStateSetter(PlayerState::Playing);
//        return;
//    }
//    // 清空队列
//    clearPktAndFrameQueuesFunc();
//    // 刷新解码器buffer
//    for (auto& codecCtx : codecCtxList)
//        avcodec_flush_buffers(codecCtx);
//    // 先重置一下时钟
//    if (streamIndex >= 0)
//        videoClock.store(pts * av_q2d(formatCtx->streams[streamIndex]->time_base));
//    else
//        videoClock.store(pts / (double)AV_TIME_BASE);
//    // 读取下一帧
//    AVPacket* pkt = nullptr;
//    while (true)
//    {
//        if (MediaDecodeUtils::readFrame(&logger, formatCtx, pkt, true))
//        {
//            if (pkt->stream_index != playbackStateVariables.streamIndex)
//            {
//                av_packet_free(&pkt); // 释放不需要的包
//                continue;
//            }
//            // 重置时钟
//            if (pkt->pts && pkt->stream_index >= 0) // 如果存在pts，否则pkt->stream_index不可取
//                playbackStateVariables.videoClock.store(pkt->pts * av_q2d(playbackStateVariables.formatCtx->streams[pkt->stream_index]->time_base));
//            // 入队
//            ConcurrentQueueOps::enqueue(*playbackStateVariables.packetQueue, pkt);
//        }
//        else
//            if (pkt) av_packet_free(&pkt); // 释放包
//        break;
//    }
//    if (playOptions.clockSyncFunction)
//    {
//        int64_t sleepTime = 0;
//        isVideoClockStable.store(false);
//        playOptions.clockSyncFunction(playbackStateVariables.videoClock, playbackStateVariables.isVideoClockStable, playbackStateVariables.realtimeClock, sleepTime);
//    }
//    // 恢复播放状态
//    playerStateSetter(PlayerState::Playing);
//}

void PlayerTypes::threadBlocker(Logger& logger, const std::vector<ThreadIdentifier>& blockTargetThreadIds, ThreadStateManager& threadStateManager, DemuxerInterface* demuxer, std::vector<ThreadStateManager::ThreadStateController>& outWaitObjs, bool& outDemuxerPaused)
{
    for (const auto& threadId : blockTargetThreadIds)
    {
        if (threadId == ThreadIdentifier::Demuxer)
        {
            if (demuxer)
            {
                demuxer->pause();
                outDemuxerPaused = true;
                continue;
            }
        }

        try {
            auto&& tsc = threadStateManager.get(threadId);
            outWaitObjs.push_back(tsc);
            tsc.disableWakeUp();
            tsc.setBlockedAndWaitChanged(true);
        }
        catch (std::exception e) {
            logger.error("{}", e.what());
        }
    }
}

void PlayerTypes::threadAwakener(std::vector<ThreadStateManager::ThreadStateController>& waitObjs, DemuxerInterface* demuxer, bool demuxerPaused)
{
    for (auto& waitObj : waitObjs)
    {
        waitObj.enableWakeUp();
        waitObj.wakeUp(); // 唤醒线程
    }
    if (demuxerPaused && demuxer)
        demuxer->resume();
}
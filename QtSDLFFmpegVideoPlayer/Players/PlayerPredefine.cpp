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
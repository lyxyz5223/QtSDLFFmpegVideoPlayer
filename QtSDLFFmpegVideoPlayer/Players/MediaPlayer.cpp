#include "MediaPlayer.h"

MediaPlayer::StreamTypes MediaPlayer::findStreams(AVFormatContext* formatCtx)
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

void MediaPlayer::requestTaskHandlerSeek(MediaRequestHandleEvent* e, std::any userData)
{
    // 先将播放器状态调整至非播放中状态
    setPlayerState(PlayerState::Seeking);
    // 此时已经所有相关线程均已暂停
    auto* seekEvent = static_cast<MediaSeekEvent*>(e);
    uint64_t pts = seekEvent->timestamp();
    StreamIndexType streamIndex = seekEvent->streamIndex();
    int rst = av_seek_frame(demuxer->getFormatContext(), streamIndex, pts, AVSEEK_FLAG_BACKWARD);
    if (rst < 0) // 寻找失败
    {
        logger.error("Error seeking to pts: {} in stream index: {}, duration: {}", pts, streamIndex, demuxer->getFormatContext()->duration);
        // 恢复播放状态
        setPlayerState(PlayerState::Playing);
        return;
    }
    videoPlayer->clearBuffers();
    audioPlayer->clearBuffers();
    AVPacket* packet = nullptr;
    demuxer->readOnePacket(&packet);
    if (packet)
    {
        videoPlayer->clockSync(pts, streamIndex, false);
        audioPlayer->clockSync(pts, streamIndex, false);
    }
    // 恢复播放状态
    setPlayerState(PlayerState::Playing);
}

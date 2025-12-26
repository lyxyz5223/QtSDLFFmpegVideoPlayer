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


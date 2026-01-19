#include "QtMediaPlayer.h"
#include <QVideoSink>
#include <QVideoFrame>

std::optional<QVideoFrameFormat::PixelFormat> QtMultiMediaPlayer::avPixelFormatToQVideoFrameFormat(AVPixelFormat pixelFormat)
{
    switch (pixelFormat)
    {
    case AV_PIX_FMT_NONE:
        break;
    case AV_PIX_FMT_YUV420P:
        return QVideoFrameFormat::Format_YUV420P;
    case AV_PIX_FMT_YUYV422:
        break;
    case AV_PIX_FMT_RGB24:
        break;
    case AV_PIX_FMT_BGR24:
        break;
    case AV_PIX_FMT_YUV422P:
        return QVideoFrameFormat::Format_YUV422P;
    case AV_PIX_FMT_YUV444P:
        break;
    case AV_PIX_FMT_YUV410P:
        break;
    case AV_PIX_FMT_YUV411P:
        break;
    case AV_PIX_FMT_GRAY8:
        return QVideoFrameFormat::Format_Y8;
    case AV_PIX_FMT_MONOWHITE:
        break;
    case AV_PIX_FMT_MONOBLACK:
        break;
    case AV_PIX_FMT_PAL8:
        break;
    case AV_PIX_FMT_YUVJ420P:
        break;
    case AV_PIX_FMT_YUVJ422P:
        break;
    case AV_PIX_FMT_YUVJ444P:
        break;
    case AV_PIX_FMT_UYVY422:
        break;
    case AV_PIX_FMT_UYYVYY411:
        break;
    case AV_PIX_FMT_BGR8:
        break;
    case AV_PIX_FMT_BGR4:
        break;
    case AV_PIX_FMT_BGR4_BYTE:
        break;
    case AV_PIX_FMT_RGB8:
        break;
    case AV_PIX_FMT_RGB4:
        break;
    case AV_PIX_FMT_RGB4_BYTE:
        break;
    case AV_PIX_FMT_NV12:
        return QVideoFrameFormat::Format_NV12;
    case AV_PIX_FMT_NV21:
        return QVideoFrameFormat::Format_NV21;
    case AV_PIX_FMT_ARGB:
        return QVideoFrameFormat::Format_ARGB8888;
    case AV_PIX_FMT_RGBA:
        return QVideoFrameFormat::Format_RGBA8888;
    case AV_PIX_FMT_ABGR:
        break;
    case AV_PIX_FMT_BGRA:
        return QVideoFrameFormat::Format_BGRA8888;
    case AV_PIX_FMT_GRAY16BE:
        break;
    case AV_PIX_FMT_GRAY16LE:
        break;
    case AV_PIX_FMT_YUV440P:
        break;
    case AV_PIX_FMT_YUVJ440P:
        break;
    case AV_PIX_FMT_YUVA420P:
        break;
    case AV_PIX_FMT_RGB48BE:
        break;
    case AV_PIX_FMT_RGB48LE:
        break;
    case AV_PIX_FMT_RGB565BE:
        break;
    case AV_PIX_FMT_RGB565LE:
        break;
    case AV_PIX_FMT_RGB555BE:
        break;
    case AV_PIX_FMT_RGB555LE:
        break;
    case AV_PIX_FMT_BGR565BE:
        break;
    case AV_PIX_FMT_BGR565LE:
        break;
    case AV_PIX_FMT_BGR555BE:
        break;
    case AV_PIX_FMT_BGR555LE:
        break;
    case AV_PIX_FMT_VAAPI:
        break;
    case AV_PIX_FMT_YUV420P16LE:
        break;
    case AV_PIX_FMT_YUV420P16BE:
        break;
    case AV_PIX_FMT_YUV422P16LE:
        break;
    case AV_PIX_FMT_YUV422P16BE:
        break;
    case AV_PIX_FMT_YUV444P16LE:
        break;
    case AV_PIX_FMT_YUV444P16BE:
        break;
    case AV_PIX_FMT_DXVA2_VLD:
        break;
    case AV_PIX_FMT_RGB444LE:
        break;
    case AV_PIX_FMT_RGB444BE:
        break;
    case AV_PIX_FMT_BGR444LE:
        break;
    case AV_PIX_FMT_BGR444BE:
        break;
    case AV_PIX_FMT_YA8:
        break;
    //case AV_PIX_FMT_Y400A: // == AV_PIX_FMT_YA8
    //    break;
    //case AV_PIX_FMT_GRAY8A: // == AV_PIX_FMT_YA8
    //    break;
    case AV_PIX_FMT_BGR48BE:
        break;
    case AV_PIX_FMT_BGR48LE:
        break;
    case AV_PIX_FMT_YUV420P9BE:
        break;
    case AV_PIX_FMT_YUV420P9LE:
        break;
    case AV_PIX_FMT_YUV420P10BE:
        break;
    case AV_PIX_FMT_YUV420P10LE:
        break;
    case AV_PIX_FMT_YUV422P10BE:
        break;
    case AV_PIX_FMT_YUV422P10LE:
        break;
    case AV_PIX_FMT_YUV444P9BE:
        break;
    case AV_PIX_FMT_YUV444P9LE:
        break;
    case AV_PIX_FMT_YUV444P10BE:
        break;
    case AV_PIX_FMT_YUV444P10LE:
        break;
    case AV_PIX_FMT_YUV422P9BE:
        break;
    case AV_PIX_FMT_YUV422P9LE:
        break;
    case AV_PIX_FMT_GBRP:
        break;
    //case AV_PIX_FMT_GBR24P: // == AV_PIX_FMT_GBRP
    //    break;
    case AV_PIX_FMT_GBRP9BE:
        break;
    case AV_PIX_FMT_GBRP9LE:
        break;
    case AV_PIX_FMT_GBRP10BE:
        break;
    case AV_PIX_FMT_GBRP10LE:
        break;
    case AV_PIX_FMT_GBRP16BE:
        break;
    case AV_PIX_FMT_GBRP16LE:
        break;
    case AV_PIX_FMT_YUVA422P:
        break;
    case AV_PIX_FMT_YUVA444P:
        break;
    case AV_PIX_FMT_YUVA420P9BE:
        break;
    case AV_PIX_FMT_YUVA420P9LE:
        break;
    case AV_PIX_FMT_YUVA422P9BE:
        break;
    case AV_PIX_FMT_YUVA422P9LE:
        break;
    case AV_PIX_FMT_YUVA444P9BE:
        break;
    case AV_PIX_FMT_YUVA444P9LE:
        break;
    case AV_PIX_FMT_YUVA420P10BE:
        break;
    case AV_PIX_FMT_YUVA420P10LE:
        break;
    case AV_PIX_FMT_YUVA422P10BE:
        break;
    case AV_PIX_FMT_YUVA422P10LE:
        break;
    case AV_PIX_FMT_YUVA444P10BE:
        break;
    case AV_PIX_FMT_YUVA444P10LE:
        break;
    case AV_PIX_FMT_YUVA420P16BE:
        break;
    case AV_PIX_FMT_YUVA420P16LE:
        break;
    case AV_PIX_FMT_YUVA422P16BE:
        break;
    case AV_PIX_FMT_YUVA422P16LE:
        break;
    case AV_PIX_FMT_YUVA444P16BE:
        break;
    case AV_PIX_FMT_YUVA444P16LE:
        break;
    case AV_PIX_FMT_VDPAU:
        break;
    case AV_PIX_FMT_XYZ12LE:
        break;
    case AV_PIX_FMT_XYZ12BE:
        break;
    case AV_PIX_FMT_NV16:
        break;
    case AV_PIX_FMT_NV20LE:
        break;
    case AV_PIX_FMT_NV20BE:
        break;
    case AV_PIX_FMT_RGBA64BE:
        break;
    case AV_PIX_FMT_RGBA64LE:
        break;
    case AV_PIX_FMT_BGRA64BE:
        break;
    case AV_PIX_FMT_BGRA64LE:
        break;
    case AV_PIX_FMT_YVYU422:
        break;
    case AV_PIX_FMT_YA16BE:
        break;
    case AV_PIX_FMT_YA16LE:
        break;
    case AV_PIX_FMT_GBRAP:
        break;
    case AV_PIX_FMT_GBRAP16BE:
        break;
    case AV_PIX_FMT_GBRAP16LE:
        break;
    case AV_PIX_FMT_QSV:
        break;
    case AV_PIX_FMT_MMAL:
        break;
    case AV_PIX_FMT_D3D11VA_VLD:
        break;
    case AV_PIX_FMT_CUDA:
        break;
    case AV_PIX_FMT_0RGB:
        break;
    case AV_PIX_FMT_RGB0:
        break;
    case AV_PIX_FMT_0BGR:
        break;
    case AV_PIX_FMT_BGR0:
        break;
    case AV_PIX_FMT_YUV420P12BE:
        break;
    case AV_PIX_FMT_YUV420P12LE:
        break;
    case AV_PIX_FMT_YUV420P14BE:
        break;
    case AV_PIX_FMT_YUV420P14LE:
        break;
    case AV_PIX_FMT_YUV422P12BE:
        break;
    case AV_PIX_FMT_YUV422P12LE:
        break;
    case AV_PIX_FMT_YUV422P14BE:
        break;
    case AV_PIX_FMT_YUV422P14LE:
        break;
    case AV_PIX_FMT_YUV444P12BE:
        break;
    case AV_PIX_FMT_YUV444P12LE:
        break;
    case AV_PIX_FMT_YUV444P14BE:
        break;
    case AV_PIX_FMT_YUV444P14LE:
        break;
    case AV_PIX_FMT_GBRP12BE:
        break;
    case AV_PIX_FMT_GBRP12LE:
        break;
    case AV_PIX_FMT_GBRP14BE:
        break;
    case AV_PIX_FMT_GBRP14LE:
        break;
    case AV_PIX_FMT_YUVJ411P:
        break;
    case AV_PIX_FMT_BAYER_BGGR8:
        break;
    case AV_PIX_FMT_BAYER_RGGB8:
        break;
    case AV_PIX_FMT_BAYER_GBRG8:
        break;
    case AV_PIX_FMT_BAYER_GRBG8:
        break;
    case AV_PIX_FMT_BAYER_BGGR16LE:
        break;
    case AV_PIX_FMT_BAYER_BGGR16BE:
        break;
    case AV_PIX_FMT_BAYER_RGGB16LE:
        break;
    case AV_PIX_FMT_BAYER_RGGB16BE:
        break;
    case AV_PIX_FMT_BAYER_GBRG16LE:
        break;
    case AV_PIX_FMT_BAYER_GBRG16BE:
        break;
    case AV_PIX_FMT_BAYER_GRBG16LE:
        break;
    case AV_PIX_FMT_BAYER_GRBG16BE:
        break;
    case AV_PIX_FMT_YUV440P10LE:
        break;
    case AV_PIX_FMT_YUV440P10BE:
        break;
    case AV_PIX_FMT_YUV440P12LE:
        break;
    case AV_PIX_FMT_YUV440P12BE:
        break;
    case AV_PIX_FMT_AYUV64LE:
        break;
    case AV_PIX_FMT_AYUV64BE:
        break;
    case AV_PIX_FMT_VIDEOTOOLBOX:
        break;
    case AV_PIX_FMT_P010LE:
        break;
    case AV_PIX_FMT_P010BE:
        break;
    case AV_PIX_FMT_GBRAP12BE:
        break;
    case AV_PIX_FMT_GBRAP12LE:
        break;
    case AV_PIX_FMT_GBRAP10BE:
        break;
    case AV_PIX_FMT_GBRAP10LE:
        break;
    case AV_PIX_FMT_MEDIACODEC:
        break;
    case AV_PIX_FMT_GRAY12BE:
        break;
    case AV_PIX_FMT_GRAY12LE:
        break;
    case AV_PIX_FMT_GRAY10BE:
        break;
    case AV_PIX_FMT_GRAY10LE:
        break;
    case AV_PIX_FMT_P016LE:
        break;
    case AV_PIX_FMT_P016BE:
        break;
    case AV_PIX_FMT_D3D11:
        break;
    case AV_PIX_FMT_GRAY9BE:
        break;
    case AV_PIX_FMT_GRAY9LE:
        break;
    case AV_PIX_FMT_GBRPF32BE:
        break;
    case AV_PIX_FMT_GBRPF32LE:
        break;
    case AV_PIX_FMT_GBRAPF32BE:
        break;
    case AV_PIX_FMT_GBRAPF32LE:
        break;
    case AV_PIX_FMT_DRM_PRIME:
        break;
    case AV_PIX_FMT_OPENCL:
        break;
    case AV_PIX_FMT_GRAY14BE:
        break;
    case AV_PIX_FMT_GRAY14LE:
        break;
    case AV_PIX_FMT_GRAYF32BE:
        break;
    case AV_PIX_FMT_GRAYF32LE:
        break;
    case AV_PIX_FMT_YUVA422P12BE:
        break;
    case AV_PIX_FMT_YUVA422P12LE:
        break;
    case AV_PIX_FMT_YUVA444P12BE:
        break;
    case AV_PIX_FMT_YUVA444P12LE:
        break;
    case AV_PIX_FMT_NV24:
        break;
    case AV_PIX_FMT_NV42:
        break;
    case AV_PIX_FMT_VULKAN:
        break;
    case AV_PIX_FMT_Y210BE:
        break;
    case AV_PIX_FMT_Y210LE:
        break;
    case AV_PIX_FMT_X2RGB10LE:
        break;
    case AV_PIX_FMT_X2RGB10BE:
        break;
    case AV_PIX_FMT_X2BGR10LE:
        break;
    case AV_PIX_FMT_X2BGR10BE:
        break;
    case AV_PIX_FMT_P210BE:
        break;
    case AV_PIX_FMT_P210LE:
        break;
    case AV_PIX_FMT_P410BE:
        break;
    case AV_PIX_FMT_P410LE:
        break;
    case AV_PIX_FMT_P216BE:
        break;
    case AV_PIX_FMT_P216LE:
        break;
    case AV_PIX_FMT_P416BE:
        break;
    case AV_PIX_FMT_P416LE:
        break;
    case AV_PIX_FMT_VUYA:
        break;
    case AV_PIX_FMT_RGBAF16BE:
        break;
    case AV_PIX_FMT_RGBAF16LE:
        break;
    case AV_PIX_FMT_VUYX:
        break;
    case AV_PIX_FMT_P012LE:
        break;
    case AV_PIX_FMT_P012BE:
        break;
    case AV_PIX_FMT_Y212BE:
        break;
    case AV_PIX_FMT_Y212LE:
        break;
    case AV_PIX_FMT_XV30BE:
        break;
    case AV_PIX_FMT_XV30LE:
        break;
    case AV_PIX_FMT_XV36BE:
        break;
    case AV_PIX_FMT_XV36LE:
        break;
    case AV_PIX_FMT_RGBF32BE:
        break;
    case AV_PIX_FMT_RGBF32LE:
        break;
    case AV_PIX_FMT_RGBAF32BE:
        break;
    case AV_PIX_FMT_RGBAF32LE:
        break;
    case AV_PIX_FMT_P212BE:
        break;
    case AV_PIX_FMT_P212LE:
        break;
    case AV_PIX_FMT_P412BE:
        break;
    case AV_PIX_FMT_P412LE:
        break;
    case AV_PIX_FMT_GBRAP14BE:
        break;
    case AV_PIX_FMT_GBRAP14LE:
        break;
    case AV_PIX_FMT_D3D12:
        break;
    case AV_PIX_FMT_AYUV:
        break;
    case AV_PIX_FMT_UYVA:
        break;
    case AV_PIX_FMT_VYU444:
        break;
    case AV_PIX_FMT_V30XBE:
        break;
    case AV_PIX_FMT_V30XLE:
        break;
    case AV_PIX_FMT_RGBF16BE:
        break;
    case AV_PIX_FMT_RGBF16LE:
        break;
    case AV_PIX_FMT_RGBA128BE:
        break;
    case AV_PIX_FMT_RGBA128LE:
        break;
    case AV_PIX_FMT_RGB96BE:
        break;
    case AV_PIX_FMT_RGB96LE:
        break;
    case AV_PIX_FMT_Y216BE:
        break;
    case AV_PIX_FMT_Y216LE:
        break;
    case AV_PIX_FMT_XV48BE:
        break;
    case AV_PIX_FMT_XV48LE:
        break;
    case AV_PIX_FMT_GBRPF16BE:
        break;
    case AV_PIX_FMT_GBRPF16LE:
        break;
    case AV_PIX_FMT_GBRAPF16BE:
        break;
    case AV_PIX_FMT_GBRAPF16LE:
        break;
    case AV_PIX_FMT_GRAYF16BE:
        break;
    case AV_PIX_FMT_GRAYF16LE:
        break;
    case AV_PIX_FMT_AMF_SURFACE:
        break;
    case AV_PIX_FMT_GRAY32BE:
        break;
    case AV_PIX_FMT_GRAY32LE:
        break;
    case AV_PIX_FMT_YAF32BE:
        break;
    case AV_PIX_FMT_YAF32LE:
        break;
    case AV_PIX_FMT_YAF16BE:
        break;
    case AV_PIX_FMT_YAF16LE:
        break;
    case AV_PIX_FMT_GBRAP32BE:
        break;
    case AV_PIX_FMT_GBRAP32LE:
        break;
    case AV_PIX_FMT_YUV444P10MSBBE:
        break;
    case AV_PIX_FMT_YUV444P10MSBLE:
        break;
    case AV_PIX_FMT_YUV444P12MSBBE:
        break;
    case AV_PIX_FMT_YUV444P12MSBLE:
        break;
    case AV_PIX_FMT_GBRP10MSBBE:
        break;
    case AV_PIX_FMT_GBRP10MSBLE:
        break;
    case AV_PIX_FMT_GBRP12MSBBE:
        break;
    case AV_PIX_FMT_GBRP12MSBLE:
        break;
    case AV_PIX_FMT_OHCODEC:
        break;
    case AV_PIX_FMT_NB:
        break;
    default:
        break;
    }
    return std::optional<QVideoFrameFormat::PixelFormat>();
}

QVideoFrame QtMultiMediaPlayer::createVideoFrameFromAVFrame(AVFrame* avFrame) {
    if (!avFrame || !avFrame->data[0])
        return QVideoFrame();
    // 将AVPixelFormat转换为QVideoFrameFormat::PixelFormat
    auto frameFormat = avPixelFormatToQVideoFrameFormat(static_cast<AVPixelFormat>(avFrame->format));
    if (!frameFormat.has_value())
        return QVideoFrame();
    
    QSize frameSize(avFrame->width, avFrame->height);
    QVideoFrameFormat format(frameSize, frameFormat.value());
    // 设置颜色空间
    if (avFrame->colorspace == AVCOL_SPC_BT709)
        format.setColorSpace(QVideoFrameFormat::ColorSpace_BT709);
    else if (avFrame->colorspace == AVCOL_SPC_BT470BG)
        format.setColorSpace(QVideoFrameFormat::ColorSpace_BT601);

    QVideoFrame frame(format);
    if (!frame.map(QVideoFrame::WriteOnly))
        return QVideoFrame();
    int planes = av_pix_fmt_count_planes(static_cast<AVPixelFormat>(avFrame->format));
    if (planes < 0)
        return QVideoFrame();
    // 复制数据到QVideoFrame
    for (int i = 0; i < planes; ++i)
    {
        int planeHeight = (i == 0) ? avFrame->height : avFrame->height / 2;
        uint8_t* dst = frame.bits(i);
        uint8_t* src = avFrame->data[i];
        int dstStride = frame.bytesPerLine(i);
        int srcStride = avFrame->linesize[i];

        // 逐行复制
        for (int row = 0; row < planeHeight; ++row)
        {
            memcpy(dst, src, std::min(dstStride, srcStride));
            dst += dstStride;
            src += srcStride;
        }
    }
    frame.unmap();
    return frame;
}

void QtMultiMediaPlayer::setupPlayer()
{
    
}

void QtMultiMediaPlayer::renderVideoFrame(const VideoFrameContext& frameCtx, VideoUserDataType userData)
{
    auto renderFrame = frameCtx.newFormatFrame;
    if (!renderFrame) // 转换格式失败
        return;
    QVideoFrame frame{ createVideoFrameFromAVFrame(renderFrame) }; //QAbstractVideoBuffer;
    currentWidget->videoSink()->setVideoFrame(frame);
}

void QtMultiMediaPlayer::cleanupPlayer()
{
    // 播放完毕，清空窗口内容
    currentWidget->videoSink()->setVideoFrame(QVideoFrame());
}

void QtMultiMediaPlayer::frameSwitchOptionsCallback(VideoFrameSwitchOptions& to, const VideoFrameContext& frameCtx, VideoUserDataType userData)
{
    to.enabled = true;
    to.format = AV_PIX_FMT_YUV420P;
    to.size = SizeI{};
}


bool QtMultiMediaPlayer::play(const std::string& filePath, QVideoWidget* widget, bool enableHardwareDecoding)
{
    return play(filePath, widget, enableHardwareDecoding ? VideoDecodeType::Hardware : VideoDecodeType::Software);
}

bool QtMultiMediaPlayer::play(const std::string& filePath, QVideoWidget* widget, VideoDecodeType videoDecodeType)
{
    this->currentWidget = widget;
    setupPlayer();
    MediaPlayer::MediaPlayOptions mediaOptions;
    mediaOptions.renderer = std::bind(&QtMultiMediaPlayer::renderVideoFrame, this, std::placeholders::_1, std::placeholders::_2);
    mediaOptions.rendererUserData = std::any{};
    mediaOptions.frameSwitchOptionsCallback = std::bind(&QtMultiMediaPlayer::frameSwitchOptionsCallback, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
    mediaOptions.frameSwitchOptionsCallbackUserData = std::any{};
    mediaOptions.decodeType = videoDecodeType;
    bool r = MediaPlayer::play(filePath, mediaOptions);
    return r;
}

void QtMultiMediaPlayer::startEvent(MediaPlaybackStateChangeEvent* e)
{
}

void QtMultiMediaPlayer::stopEvent(MediaPlaybackStateChangeEvent* e)
{
    cleanupPlayer();
}


#pragma once
#include "MediaPlayer.h"

class FrameProcessor : protected PlayerTypes {
protected:
    Logger& logger;

public:
    FrameProcessor(Logger& logger) : logger(logger) {}
    
    SharedPtr<AVFrame> filterFrame(AVFrame* frame, IFrameFilterGraph* filterGraph, bool& needMoreFrame) {
        AVFrame* ref = av_frame_alloc();
        int refRst = av_frame_ref(ref, frame);
        if (refRst < 0)
        {
            av_frame_free(&ref);
            ref = frame;
        }
        if (!filterGraph->addFrame(ref, IFrameFilter::SrcFlagKeepReference))
            logger.error("Error add frame to filter graph.");
        auto&& filteredFrame = filterGraph->getOutputFrame(&needMoreFrame, IFrameFilter::SinkFlagNone);
        if (refRst >= 0)
            av_frame_free(&ref);
        return filteredFrame;
    };

};

class VideoFrameProcessor : FrameProcessor {
    AVPixelFormat fmt{ AV_PIX_FMT_NONE };
    SharedPtr<FFmpegFrameFilterGraph> filterGraph{ nullptr };
    SharedPtr<SwsContext> swsCtx{ nullptr };
    SharedPtr<SwsContext> scaleSwsCtx{ nullptr };
    SharedPtr<AVFrame> tempSwsFrame{ nullptr };
    SharedPtr<AVFrame> scaledFrame{ nullptr };
    SharedPtr<FFmpegFrameVideoHueFilter> hueFilter{ nullptr };
    std::array<float, 4> prevColorParams{ 0.0f, 1.0f, 1.0f, 0.0f }; // brightness, contrast, saturation, hue

    Atomic<float>& brightness;
    Atomic<float>& contrast;
    Atomic<float>& saturation;
    Atomic<float>& hue;

    SizeI scaledFrameSize;

public:
    VideoFrameProcessor(Logger& logger,
        Atomic<float>& brightness,
        Atomic<float>& contrast,
        Atomic<float>& saturation,
        Atomic<float>& hue
    ) : FrameProcessor(logger), brightness(brightness), contrast(contrast), saturation(saturation), hue(hue) {}

    SharedPtr<AVFrame> process(const VideoPlayer::DecodedFrameContext& frameCtx) {
        auto rawFrame = frameCtx.filteredFrame;
        if (!rawFrame) return nullptr;
        SharedPtr<AVFrame> swFrame{ makeSharedFrame() };
        if (!filterGraph) init(frameCtx);
        updateFilterParams();
        if (frameCtx.isHardwareDecoded)
        {
            if (fmt != AV_PIX_FMT_NONE)
            {
                bool b = VideoPlayer::hwToSwFrame(swFrame.get(), rawFrame, fmt);
                if (!b)
                {
                    logger.error("Error transferring the data from GPU memory to system memory");
                    return nullptr;
                }
                av_buffer_unref(&swFrame->hw_frames_ctx);
            }
            else
                logger.error("Error getting the data transfer format from GPU memory");
        }
        if (!VideoPlayer::swsScaleFrame(swsCtx.get(), frameCtx.isHardwareDecoded ? swFrame.get() : rawFrame, tempSwsFrame.get(), &logger))
            return nullptr;
        bool needMore = false;
        auto fltdFrame = filterFrame(tempSwsFrame.get(), filterGraph.get(), needMore);
        if (!fltdFrame) return nullptr;
        if (scaledFrameSize.width() != scaledFrame->width || scaledFrameSize.height() != scaledFrame->height || !swsCtx)
            updateSwsScaleFrameSize(frameCtx);
        if (!VideoPlayer::swsScaleFrame(scaleSwsCtx.get(), fltdFrame.get(), scaledFrame.get(), &logger))
            return nullptr;
        return scaledFrame;
    }

    void setProcessedFrameSize(SizeI size) {
        scaledFrameSize = size;
    }
private:
    void init(const VideoPlayer::DecodedFrameContext& frameCtx) {
        auto& rawFrame = frameCtx.filteredFrame;
        auto& formatCtx = frameCtx.formatCtx;
        auto& codecCtx = frameCtx.codecCtx;
        auto& streamIndex = frameCtx.streamIndex;
        fmt = (frameCtx.isHardwareDecoded ? VideoPlayer::getHwFramePixelFormat(rawFrame->hw_frames_ctx) : AV_PIX_FMT_NONE);
        filterGraph = std::make_shared<FFmpegFrameFilterGraph>(StreamType::STVideo, formatCtx, codecCtx, streamIndex);
        hueFilter = std::make_shared<FFmpegFrameVideoHueFilter>(StreamType::STVideo, formatCtx, codecCtx, streamIndex);
        filterGraph->addFilter(hueFilter);
        filterGraph->configureFilterGraph();
        tempSwsFrame = makeSharedFrame();
        tempSwsFrame->width = frameCtx.rawFrame->width;
        tempSwsFrame->height = frameCtx.rawFrame->height;
        tempSwsFrame->format = AV_PIX_FMT_YUV420P;
        if (av_frame_get_buffer(tempSwsFrame.get(), 0) < 0) return;
        swsCtx = VideoPlayer::createSwsContext(tempSwsFrame.get(), static_cast<AVPixelFormat>(frameCtx.isHardwareDecoded ? fmt : frameCtx.rawFrame->format), SizeI{ frameCtx.rawFrame->width, frameCtx.rawFrame->height }, &logger);
        updateSwsScaleFrameSize(frameCtx);
    }

    void updateFilterParams() {
        auto b = brightness.load();
        auto c = contrast.load();
        auto s = saturation.load();
        auto h = hue.load();
        if (prevColorParams[0] != b)
        {
            hueFilter->setBrightness(brightness.load());
            prevColorParams[0] = brightness.load();
        }
        if (prevColorParams[1] != c)
        {
            prevColorParams[1] = contrast.load();
        }
        if (prevColorParams[2] != s)
        {
            hueFilter->setSaturation(saturation.load());
            prevColorParams[2] = saturation.load();
        }
        if (prevColorParams[3] != h)
        {
            hueFilter->setHue(hue.load());
            prevColorParams[3] = hue.load();
        }
    }

    void updateSwsScaleFrameSize(const VideoPlayer::DecodedFrameContext& frameCtx) {
        if (!scaledFrameSize.isValid()) scaledFrameSize = SizeI{ frameCtx.rawFrame->width, frameCtx.rawFrame->height };
        scaledFrame = makeSharedFrame();
        scaledFrame->width = scaledFrameSize.width();
        scaledFrame->height = scaledFrameSize.height();
        scaledFrame->format = AV_PIX_FMT_YUV420P;
        if (av_frame_get_buffer(scaledFrame.get(), 0) < 0) return;
        scaleSwsCtx = VideoPlayer::createSwsContext(scaledFrame.get(), AV_PIX_FMT_YUV420P, SizeI{ frameCtx.rawFrame->width, frameCtx.rawFrame->height }, &logger);
    }
};


class AudioFrameProcessor : public FrameProcessor {

};
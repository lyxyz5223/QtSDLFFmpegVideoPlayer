#include "AudioPlayer.h"

bool AudioPlayer::playAudioFile()
{
    waitStopped.set(false);
    auto demuxerMode = this->demuxerMode;
    auto requestTaskQueueHandlerMode = this->requestTaskQueueHandlerMode;
    // 初始化解复用器
    try {
        auto& psv = playbackStateVariables;
        if (demuxerMode == ComponentWorkMode::Internal)
        {
            psv.demuxer = internalDemuxer;
            psv.demuxer->setPacketEnqueueCallback(playbackStateVariables.demuxerStreamType, std::bind(&AudioPlayer::packetEnqueueCallback, this));
            psv.demuxer->openAndSelectStreams(psv.filePath, psv.demuxerStreamType, psv.playOptions.streamIndexSelector);
        }
        else/* if (demuxerMode == DemuxerMode::External)*/
            psv.demuxer = externalDemuxer;
        if (!psv.demuxer) // 解复用器不存在，通常为外部解复用器指针为空
            throw std::runtime_error("Demuxer is nullptr.");
        psv.formatCtx = psv.demuxer->getFormatContext();
        psv.streamIndex = psv.demuxer->getStreamIndex(psv.demuxerStreamType);
        psv.packetQueue = psv.demuxer->getPacketQueue(psv.demuxerStreamType);

        if (requestTaskQueueHandlerMode == ComponentWorkMode::Internal)
        {
            psv.requestQueueHandler = internalRequestTaskQueueHandler.get();
            addThreadStateHandlersContext(internalRequestTaskQueueHandler.get());
        }
        else
            psv.requestQueueHandler = externalRequestTaskQueueHandler;
        if (!psv.requestQueueHandler) // 请求任务队列处理器不存在，通常为外部请求任务队列处理器指针为空
            throw std::runtime_error("RequestTaskQueueHandler is nullptr.");
    }
    catch (std::runtime_error e) {
        logger.error("Failed to open and select streams: {}", e.what());
        resetPlayer();
        return false;
    }
    if (playbackStateVariables.streamIndex < 0)
    {
        logger.warning("No audio stream selected.");
        resetPlayer();
        return false;
    }

    //// 打开文件
    //if (!openInput())
    //{
    //    resetPlayer();
    //    return false;
    //}
    //// 查找流信息
    //if (!findStreamInfo())
    //{
    //    resetPlayer();
    //    return false;
    //}
    //// 查找并选择音频流
    //if (!findAndSelectAudioStream())
    //{
    //    resetPlayer();
    //    return false;
    //}
    // 寻找并打开解码器 
    if (!findAndOpenAudioDecoder())
    {
        resetPlayer();
        return false;
    }
    // 打开音频设备
    if (!this->playbackStateVariables.codecCtx->frame_size)
        this->playbackStateVariables.codecCtx->frame_size = DEFAULT_AUDIO_OUTPUT_STREAM_BUFFER_SIZE;
    unsigned int frameBufferSize = this->playbackStateVariables.codecCtx->frame_size;
    if (!openAndStartOutputAudioStream(this->playbackStateVariables.codecCtx->sample_rate, this->playbackStateVariables.codecCtx->ch_layout, AUDIO_OUTPUT_FORMAT, &frameBufferSize))
    {
        resetPlayer();
        return false;
    }

    // 修改状态为：播放中
    setPlayerState(PlayerState::Playing);
    // 启动处理线程
    if (requestTaskQueueHandlerMode == ComponentWorkMode::Internal)
        playbackStateVariables.requestQueueHandler->start();
    // 启动读包
    //std::thread threadReadPackets(&AudioPlayer::readPackets, this);
    if (demuxerMode == ComponentWorkMode::Internal)
        playbackStateVariables.demuxer->start(); // 启动解复用器读取线程
    // 启动包转音频流
    std::thread threadPacket2AudioStreams(&AudioPlayer::packet2AudioStreams, this);
    // 启动渲染音频
    std::thread threadRenderAudio(&AudioPlayer::renderAudio, this);
    // 等待线程结束
    if (requestTaskQueueHandlerMode == ComponentWorkMode::Internal)
        playbackStateVariables.requestQueueHandler->waitStop();
    //if (threadReadPackets.joinable())
    //    threadReadPackets.join();
    if (threadPacket2AudioStreams.joinable())
        threadPacket2AudioStreams.join();
    if (threadRenderAudio.joinable())
        threadRenderAudio.join();
    if (demuxerMode == ComponentWorkMode::Internal)
        playbackStateVariables.demuxer->waitStop(); // 等待解复用器停止
    // 关闭音频输出流
    stopAndCloseOutputAudioStream();
    // 恢复播放器状态
    resetPlayer();
    // 通知停止
    waitStopped.setAndNotifyAll(true);
    return true;
}

void AudioPlayer::audioOutputStreamErrorCallback(AudioAdapter::AudioErrorType type, const std::string& errorText)
{
    logger.error("AudioAdapter Error ({}): {}", static_cast<int>(type), errorText.c_str());
}

bool AudioPlayer::startOutputAudioStream()
{
    if (!playbackStateVariables.audioDevice->isStreamRunning())
    {
        AudioAdapter::AudioErrorType result = playbackStateVariables.audioDevice->startStream();
        if (result != AudioAdapter::NoError && result != AudioAdapter::Warning)
            return false;
    }
    return true;
}

void AudioPlayer::closeOutputAudioStream()
{
    if (playbackStateVariables.audioDevice && playbackStateVariables.audioDevice->isStreamOpen())
        playbackStateVariables.audioDevice->closeStream();
}

void AudioPlayer::stopOutputAudioStream()
{
    if (playbackStateVariables.audioDevice && playbackStateVariables.audioDevice->isStreamRunning())
        playbackStateVariables.audioDevice->stopStream();
}

void AudioPlayer::stopAndCloseOutputAudioStream()
{
    stopOutputAudioStream();
    closeOutputAudioStream();
}

bool AudioPlayer::openOutputAudioStream(int sampleRate, AVChannelLayout channelLayout, AVSampleFormat sampleFmt, unsigned int* frameBufferSize)
{
    // 删除已有的音频设备实例
    // audioDevice.reset();
    constexpr AudioAdapterFactory::AudioAdapterAdapter audioDeviceApiType = AudioAdapterFactory::RtAudioAdapter;
    constexpr AudioAdapter::AudioApi audioApiType = AudioAdapter::WindowsWasapi;
    // 创建新的RtAudio实例
    playbackStateVariables.audioDevice.reset(AudioAdapterFactory::create(audioDeviceApiType, audioApiType));
    if (!playbackStateVariables.audioDevice)
        return false;
    if (playbackStateVariables.audioDevice->getDeviceCount() == 0) // 没有可用音频设备
    {
        playbackStateVariables.audioDevice.reset();
        return false;
    }
    // 设置错误回调函数
    try {
        playbackStateVariables.audioDevice->setErrorCallback([this](AudioAdapter::AudioErrorType type, const std::string& errorText) {
            this->audioOutputStreamErrorCallback(type, errorText);
            });
    }
    catch (const std::exception& e) {
        logger.error("Cannot set audio error callback: {}", e.what());
    }
    // 音频输出流
    AudioAdapter::AudioStreamParameters outputParams;
    outputParams.deviceId = playbackStateVariables.audioDevice->getDefaultOutputDevice();
    outputParams.firstChannel = 0;
    outputParams.nChannels = channelLayout.nb_channels;
    AudioAdapter::AudioStreamOptions options;
    if (audioDeviceApiType == AudioAdapterFactory::PortAudioAdapter)
        options.flags = AudioAdapter::ClipOff;
    unsigned int defaultBufferSize = DEFAULT_AUDIO_OUTPUT_STREAM_BUFFER_SIZE;
    unsigned int* pFrameBufferSize = frameBufferSize;
    if (!pFrameBufferSize)
        pFrameBufferSize = &defaultBufferSize;
    // 如果传入的缓冲区大小为0，则使用最小允许值，即使默认值是0也会被替换为最小允许值
    if (*pFrameBufferSize == 0 && audioDeviceApiType == AudioAdapterFactory::RtAudioAdapter)
        options.flags = AudioAdapter::MinimizeLatency;
    AudioAdapter::AudioStreamOptions* pOptions = &options;
    // 采用S16格式播放：RTAUDIO_SINT16
    AudioAdapter::AudioErrorType result = playbackStateVariables.audioDevice->openStream(
        &outputParams, nullptr,
        AudioAdapter::avSampleFormatToTargetAudioFormat(sampleFmt), sampleRate, pFrameBufferSize,
        [this](void* outputBuffer, void* inputBuffer, unsigned int nFrames, double streamTime, AudioAdapter::AudioStreamStatuses status, AudioAdapter::RawArgsType rawArgs, AudioAdapter::UserDataType userData) -> AudioAdapter::AudioCallbackResult {
            return this->renderAudioAsyncCallback(outputBuffer, inputBuffer, nFrames, streamTime, status, rawArgs, userData);
        },
        this, pOptions);

    if (result != AudioAdapter::NoError)
    {
        playbackStateVariables.audioDevice.reset();
        return false;
    }

    playbackStateVariables.numberOfAudioOutputChannels.store(channelLayout.nb_channels);
    playbackStateVariables.audioOutputStreamBufferSize.store(*pFrameBufferSize);
    //PaError err = Pa_Initialize();
    //if (err != paNoError)
    //    return false;
    //PaStreamParameters outputParameters;
    //outputParameters.device = Pa_GetDefaultOutputDevice();
    //if (outputParameters.device == paNoDevice)
    //    return false;
    //outputParameters.channelCount = channelLayout.nb_channels;
    //outputParameters.sampleFormat = paInt16; // 统一转换为S16格式
    //outputParameters.suggestedLatency = Pa_GetDeviceInfo(outputParameters.device)->defaultLowOutputLatency;
    //outputParameters.hostApiSpecificStreamInfo = nullptr;
    //unsigned long framesPerBuffer = frameBufferSize ? *frameBufferSize : DEFAULT_AUDIO_OUTPUT_STREAM_BUFFER_SIZE;
    //PaStream* stream = nullptr;
    //err = Pa_OpenStream(
    //    &stream,
    //    nullptr, // no input
    //    &outputParameters,
    //    sampleRate,
    //    framesPerBuffer,
    //    paClipOff, // we won't output out of range samples so don't bother clipping them
    //    [](const void* inputBuffer, void* outputBuffer,
    //        unsigned long framesPerBuffer,
    //        const PaStreamCallbackTimeInfo* timeInfo,
    //        PaStreamCallbackFlags statusFlags,
    //        void* userData)->int {
    //            return static_cast<VideoPlayer*>(userData)->audioCallback(inputBuffer, outputBuffer, framesPerBuffer, timeInfo, statusFlags, userData);
    //    },
    //    this // 传递this指针作为用户数据
    //);
    //if (err != paNoError)
    //    return false;
    //audioDevice.reset(stream);

    return true;
}

bool AudioPlayer::openAndStartOutputAudioStream(int sampleRate, AVChannelLayout channelLayout, AVSampleFormat sampleFmt, unsigned int* frameBufferSize)
{
    if (!openOutputAudioStream(sampleRate, channelLayout, sampleFmt, frameBufferSize))
        return false;
    return startOutputAudioStream();
}

//bool AudioPlayer::findAndSelectAudioStream()
//{
//    std::vector<StreamIndexType> streamIndicesList;
//    // 查找视频流和音频流
//    for (size_t i = 0; i < playbackStateVariables.formatCtx->nb_streams; ++i)
//        if (playbackStateVariables.formatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
//            streamIndicesList.emplace_back(i);
//    StreamIndexType si = -1;
//    auto& streamIndexSelector = playbackStateVariables.playOptions.streamIndexSelector;
//    if (!streamIndexSelector)
//        return false;
//    bool rst = streamIndexSelector(si, StreamType::Audio, streamIndicesList, playbackStateVariables.formatCtx.get(), playbackStateVariables.codecCtx.get());
//    if (rst)
//    {
//        if (si >= 0 && si < static_cast<StreamIndexType>(playbackStateVariables.formatCtx->nb_streams))
//            playbackStateVariables.streamIndex = si;
//        else
//            playbackStateVariables.streamIndex = -1;
//    }
//    return rst;
//}

bool AudioPlayer::findAndOpenAudioDecoder()
{
    return MediaDecodeUtils::findAndOpenAudioDecoder(&logger, playbackStateVariables.formatCtx, playbackStateVariables.streamIndex, playbackStateVariables.codecCtx);
}


//void AudioPlayer::readPackets()
//{
//    auto& threadStateManager = playbackStateVariables.threadStateManager;
//    auto&& waitObj = threadStateManager.addThread(ThreadIdentifier::ReadingThread);
//    ThreadStateManager::AutoRemovedThreadObj autoRemoveWaitObj{ threadStateManager, waitObj };
//
//    // 视频解码和显示循环
//    while (true)
//    {
//        if (waitObj.isBlocking())
//            waitObj.block();
//
//        if (shouldStop())
//            break;
//
//        auto oldPktQueueSize = getQueueSize(playbackStateVariables.packetQueue);
//        if (oldPktQueueSize >= MAX_AUDIO_PACKET_QUEUE_SIZE)
//        {
//            waitObj.pause();
//            continue;
//        }
//        // Read frame from the format context 读取帧
//        AVPacket* pkt = nullptr;
//        if (!MediaDecodeUtils::readFrame(&logger, playbackStateVariables.formatCtx, pkt, true))
//        {
//            if (pkt)
//                av_packet_free(&pkt); // 释放包
//            logger.trace("Read frame finished.");
//            //break; // 读取结束，退出循环
//            // 读取结束，暂停线程，等待通知
//            waitObj.pause();
//            continue;
//        }
//        if (pkt->stream_index != playbackStateVariables.streamIndex)
//        {
//            av_packet_free(&pkt); // 释放不需要的包
//            continue;
//        }
//        enqueue(playbackStateVariables.packetQueue, pkt);
//        //auto pktQueueSize = getQueueSize(playbackStateVariables.packetQueue);
//        logger.trace("Pushed audio packet, queue size: {}", oldPktQueueSize + 1);
//        if (oldPktQueueSize + 1 <= MIN_AUDIO_PACKET_QUEUE_SIZE)
//            threadStateManager.wakeUpById(ThreadIdentifier::DecodingThread);
//    }
//}

// 音频包解码线程函数
void AudioPlayer::packet2AudioStreams()
{
    auto& threadStateManager = playbackStateVariables.threadStateManager;
    auto&& waitObj = threadStateManager.addThread(ThreadIdentifier::Decoder);
    ThreadStateManager::AutoRemovedThreadObj autoRemoveWaitObj{ threadStateManager, waitObj };

    AVRational timeBaseRational = playbackStateVariables.formatCtx->streams[playbackStateVariables.streamIndex]->time_base;
    double timeBase = av_q2d(timeBaseRational);
    AVFrame* frame = av_frame_alloc();
    //int64_t startTime = 0;
    //bool started = false;

    // 转换音频格式
    UniquePtr<AVFrame> convertedFrame = { nullptr, constDeleterAVFrame };
    // 使用swresample进行格式转换
    SwrContext* swr = nullptr;
    while (1)
    {
        if (waitObj.isBlocking())
            waitObj.block();

        if (shouldStop())
            break;
        else if (playerState != PlayerState::Playing)
        {
            waitObj.pause();
            continue;
        }
        std::unique_lock lockMtxStreamQueue(playbackStateVariables.mtxStreamQueue);
        auto streamQueueSize = playbackStateVariables.streamQueue.size();/*getQueueSize(streamQueue)*/
        lockMtxStreamQueue.unlock();
        if (streamQueueSize >= MAX_AUDIO_OUTPUT_STREAM_QUEUE_SIZE)
        {
            waitObj.pause();
            continue; // 如果音频流队列中有太多数据，等待消费掉一些再继续解码
        }
        if (getQueueSize(*playbackStateVariables.packetQueue) < MIN_AUDIO_PACKET_QUEUE_SIZE)
            playbackStateVariables.demuxer->wakeUp(); // 包队列数据过少，唤醒解复用器读取更多数据


        logger.trace("Current audio stream queue size: {}", streamQueueSize);
        // 如果队列中有包，则取出解码
        AVPacket* pkt = nullptr;
        if (!tryDequeue(*playbackStateVariables.packetQueue, pkt))
        {
            waitObj.pause();
            continue; // 出队失败，说明队列为空
        }
        if (!pkt) // 一定要过滤空包，否则avcodec_send_packet将会设置为EOF，之后将无法继续解包
            continue;
        logger.trace("Got audio packet, current audio packet queue size: {}", getQueueSize(*playbackStateVariables.packetQueue));
        UniquePtr<AVPacket> pktPtr{ pkt, constDeleterAVPacket };
        int aspRst = avcodec_send_packet(playbackStateVariables.codecCtx.get(), pkt);
        if (aspRst < 0 && aspRst != AVERROR(EAGAIN) && aspRst != AVERROR_EOF)
            continue;
        while (avcodec_receive_frame(playbackStateVariables.codecCtx.get(), frame) == 0)
        {
            if (!convertedFrame)
            {
                convertedFrame.reset(av_frame_alloc());
                convertedFrame->sample_rate = frame->sample_rate;
                convertedFrame->ch_layout = frame->ch_layout;
                convertedFrame->format = AUDIO_OUTPUT_FORMAT;
                convertedFrame->nb_samples = playbackStateVariables.audioOutputStreamBufferSize; // 输出固定样本数
                int ret = av_frame_get_buffer(convertedFrame.get(), 0);
                if (ret < 0)
                {
                    char err[AV_ERROR_MAX_STRING_SIZE];
                    logger.error("av_frame_get_buffer error: {}", av_make_error_string(err, AV_ERROR_MAX_STRING_SIZE, ret));
                    continue;
                }
                ret = swr_alloc_set_opts2(&swr,
                    // out
                    &frame->ch_layout/*ch_layout*/, AUDIO_OUTPUT_FORMAT/*sample_fmt*/, frame->sample_rate, /*sample_rate*/
                    // in
                    &frame->ch_layout/*ch_layout*/, (AVSampleFormat)frame->format/*sample_fmt*/, frame->sample_rate, /*sample_rate*/
                    0, nullptr);
                if (ret < 0)
                {
                    char err[AV_ERROR_MAX_STRING_SIZE];
                    logger.error("swr_alloc_set_opts2 error: {}", av_make_error_string(err, AV_ERROR_MAX_STRING_SIZE, ret));
                    continue;
                }
                if (swr)
                    swr_init(swr);
            }
            if (!swr)
                continue;
            int ret = swr_convert(swr, convertedFrame->data, convertedFrame->nb_samples,
                (const uint8_t**)frame->data, frame->nb_samples);
            if (ret <= 0)
            {
                char err[AV_ERROR_MAX_STRING_SIZE];
                logger.error("swr_convert error: {}", av_make_error_string(err, AV_ERROR_MAX_STRING_SIZE, ret));
                continue;
            }
            // 播放音频
            int dataSize = av_samples_get_buffer_size(nullptr, convertedFrame->ch_layout.nb_channels, convertedFrame->nb_samples, AUDIO_OUTPUT_FORMAT, 1);
            //std::vector<uint8_t> audioData(dataSize);
            //memcpy(audioData.data(), convertedFrame->data[0], dataSize);
            if (!dataSize)
                continue;
            // 将音频数据写入AudioAdapter音频流队列
            auto* audioData = reinterpret_cast<uint8_t*>(convertedFrame->data[0]);
            //std::span<uint8_t> spanAudioData{ audioData, static_cast<size_t>(dataSize) };
            std::vector<uint8_t> vecAudioData(static_cast<size_t>(dataSize), 0);
            std::copy(audioData, audioData + dataSize, vecAudioData.data());
            // data数组，如果是packed（交错）格式，则每个采样点的所有通道数据依次排列存储；
            // 如果是planar（平面）格式，则每个通道的数据依次排列存储，通道1：data[0]，通道2：data[1]，依此类推
            // 使用emplace减少一次vector构造拷贝
            //audioStreamQueue.emplace((signed int*)convertedFrame->data[0], (signed int*)convertedFrame->data[0] + num);
            std::unique_lock lockMtxStreamQueue(playbackStateVariables.mtxStreamQueue);
            //auto oneSize = playbackStateVariables.numberOfAudioOutputChannels * sizeof(uint16_t);
            //size_t count = dataSize / oneSize;
            //for (size_t i = 0; i < count; ++i)
            //{
            //    AudioStreamInfo one{
            //        {},
            //        frame->pts * timeBase
            //    };
            //    for (size_t j = 0; j < oneSize; ++j)
            //        one.dataBytes.push_back(audioData[i * oneSize + j]);
            //    //enqueue(streamQueue, one);
            //    playbackStateVariables.streamQueue.emplace(one);
            //}
            playbackStateVariables.streamQueue.emplace(std::move(vecAudioData), frame->pts, timeBaseRational, frame->pts * timeBase);
        }
    }
    swr_free(&swr);
    av_frame_free(&frame);
}

// 音频渲染
void AudioPlayer::renderAudio()
{
    auto& threadStateManager = playbackStateVariables.threadStateManager;
    auto&& waitObj = threadStateManager.addThread(ThreadIdentifier::Renderer);
    ThreadStateManager::AutoRemovedThreadObj autoRemoveWaitObj{ threadStateManager, waitObj };
    while (true)
    {
        if (waitObj.isBlocking())
            waitObj.block();
        if (shouldStop())
            break;
        // 睡眠，直到唤醒
        waitObj.pause();
    }
}

// 异步音频输出
AudioAdapter::AudioCallbackResult AudioPlayer::renderAudioAsyncCallback(void*& outputBuffer, void*& inputBuffer, unsigned int& nFrames, double& streamTime, AudioAdapter::AudioStreamStatuses& status, AudioAdapter::RawArgsType& rawArgs, AudioAdapter::UserDataType& userData)
{
    uint8_t* outBuffer = static_cast<uint8_t*>(outputBuffer);
    auto oneSize = playbackStateVariables.numberOfAudioOutputChannels * sizeof(uint16_t);
    std::unique_lock lockMtxStreamQueue(playbackStateVariables.mtxStreamQueue);
    //for (size_t i = 0; i < nFrames; ++i)
    {
        if (nFrames && isPlaying() && !playbackStateVariables.streamQueue.empty()/*tryDequeue(streamQueue, one)*/)
        {
            auto& one = playbackStateVariables.streamQueue.front();
            // logger.info("Audio sample: {}", outBuffer[i]);
            playbackStateVariables.audioClock.store(one.frameTime); // 更新音频时钟
            auto pts = one.pts;
            auto timeBase = one.timeBase;
            auto frameTime = one.frameTime;
            std::copy(one.dataBytes.begin(), one.dataBytes.end(), outBuffer); // std::span<uint8_t> dataBytes{};
            //std::copy(one.dataBytes.begin(), one.dataBytes.end(), outBuffer + i * oneSize); // std::vector<uint8_t> dataBytes{}
            //std::memcpy(outBuffer + i * oneSize, one.dataBytes.data(), one.dataBytes.size());
            playbackStateVariables.streamQueue.pop();

            // 填充音频数据后调用事件回调
            FrameContext frameCtx{
                playbackStateVariables.formatCtx,
                playbackStateVariables.codecCtx.get(),
                playbackStateVariables.streamIndex,
                { static_cast<uint8_t*>(outputBuffer), oneSize * nFrames },
                oneSize * nFrames,
                nFrames,
                playbackStateVariables.codecCtx->sample_rate,
                playbackStateVariables.numberOfAudioOutputChannels,
                pts,
                timeBase,
                frameTime,
                streamTime
            };
            AudioRenderEvent audioEvent{ &frameCtx };
            event(&audioEvent);

        }
        else
        {
            for (size_t i = 0; i < nFrames; ++i)
                for (size_t j = 0; j < oneSize && i < nFrames; ++j)
                    outBuffer[i * oneSize + j] = 0; // 静音填充
            // logger.info("音频缓冲区下溢，填充静音数据。Audio sample: {}", outBuffer[i]);
        }
    }
    //lockMtxStreamQueue.unlock();
    // 同步时钟，放在数据填充后可以免去seek后音频时钟跳动的问题（seek后未来得及修改时钟到正确的时钟就执行时钟同步）
    if (playbackStateVariables.playOptions.clockSyncFunction)
    {
        int64_t sleepTime = 0;
        playbackStateVariables.realtimeClock = streamTime;
        auto rst = playbackStateVariables.playOptions.clockSyncFunction(playbackStateVariables.audioClock, true, playbackStateVariables.realtimeClock, sleepTime); // 同步时钟
        if (rst && sleepTime != 0)
        {
            if (sleepTime > 0)
            {
                // 需要等待
                logger.info("Audio sleep: {} ms", sleepTime);
                ThreadSleepMs(sleepTime);
            }
            else //if (sleepTime < 0)
            {
                // 落后太多，跳过帧
                logger.info("Audio drop frame to catch up: {} ms", -sleepTime);
                //std::unique_lock lockMtxStreamQueue(playbackStateVariables.mtxStreamQueue);
                //for (playbackStateVariables.streamQueue.size() && playbackStateVariables.streamQueue.front().pts < playbackStateVariables.streamQueue.front().pts + sleepTime)
                //    playbackStateVariables.streamQueue.pop();
            }
        }
    }
    if (playbackStateVariables.streamQueue.size() < MIN_AUDIO_OUTPUT_STREAM_QUEUE_SIZE)
        playbackStateVariables.threadStateManager.wakeUpById(ThreadIdentifier::Decoder);
    
    //logger.info("Got audio streams, current audio stream queue size: {}", getQueueSize(streamQueue));
    if (playerState == PlayerState::Stopped || playerState == PlayerState::Stopping
        || (playbackStateVariables.streamUploadFinished && playbackStateVariables.streamQueue.empty()/*!getQueueSize(streamQueue)*/))
    {
        // 状态改变，播放结束
        if (playerState == PlayerState::Stopped || playerState == PlayerState::Stopping)
        {
            logger.info("Audio playback stopped by user.");
            return AudioAdapter::Abort;
        }
        logger.info("Audio playback finished.");
        return AudioAdapter::Complete;
    }
    return AudioAdapter::Continue;
}

void AudioPlayer::requestTaskHandlerSeek(MediaRequestHandleEvent* e, std::any userData)
{
    // 先将播放器状态调整至非播放中状态
    setPlayerState(PlayerState::Seeking);
    // 此时已经所有相关线程均已暂停
    auto* seekEvent = static_cast<MediaSeekEvent*>(e);
    uint64_t pts = seekEvent->timestamp();
    StreamIndexType streamIndex = seekEvent->streamIndex();
    int rst = av_seek_frame(playbackStateVariables.formatCtx, streamIndex, pts, AVSEEK_FLAG_BACKWARD);
    if (rst < 0) // 寻找失败
    {
        logger.error("Error audio seeking to pts: {} in stream index: {}, duration: {}", pts, streamIndex, playbackStateVariables.formatCtx->duration);
        // 恢复播放状态
        setPlayerState(PlayerState::Playing);
        return;
    }
    // 清空缓冲区
    clearBuffers();
    // 先重置一下时钟
    if (streamIndex >= 0)
        playbackStateVariables.audioClock.store(pts * av_q2d(playbackStateVariables.formatCtx->streams[streamIndex]->time_base));
    else
        playbackStateVariables.audioClock.store(pts / (double)AV_TIME_BASE);
    // 读取下一帧
    AVPacket* pkt = nullptr;
    while (true)
    {
        if (MediaDecodeUtils::readFrame(&logger, playbackStateVariables.formatCtx, pkt, true))
        {
            if (pkt->stream_index != playbackStateVariables.streamIndex)
            {
                av_packet_free(&pkt); // 释放不需要的包
                continue;
            }
            // 重置时钟
            if (pkt->pts && pkt->stream_index >= 0) // 如果存在pts，否则pkt->stream_index不可取
                playbackStateVariables.audioClock.store(pkt->pts * av_q2d(playbackStateVariables.formatCtx->streams[pkt->stream_index]->time_base));
            // 入队
            enqueue(*playbackStateVariables.packetQueue, pkt);
        }
        else
            if (pkt) av_packet_free(&pkt); // 释放包
        break;
    }
    if (playbackStateVariables.playOptions.clockSyncFunction)
    {
        int64_t sleepTime = 0;
        playbackStateVariables.playOptions.clockSyncFunction(playbackStateVariables.audioClock, false, playbackStateVariables.realtimeClock, sleepTime);
    }
    // 恢复播放状态
    setPlayerState(PlayerState::Playing);
}

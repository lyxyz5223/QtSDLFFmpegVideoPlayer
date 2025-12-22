#pragma once

// 预定义
#include "PlayerPredefine.h"

#include "ThreadUtils.h"

#include <Logger.h>

class OldVideoPlayer : public PlayerTypes {
public:


private: // 这个区域用于定义私有类型
    enum class RequestType {
        Seek = 0,
    };

    enum class SeekDirection {
        SeekBackward = 1,
        SeekForward = 2
    };
    struct SeekData {
        StreamIndexType streamIndex;
        size_t timestamp;
        SeekDirection direction;
        bool accurate; // 是否精确定位
        bool relative; // 是否为相对定位
    };
    using RequestQueueItem = std::variant<std::any, SeekData>;
    template <typename T=RequestQueueItem>
    using RequestQueueMap = std::unordered_map<RequestType, std::queue<T>>;

private: // 与视频播放本身无关的私有成员，每次播放不需要处理的变量，比如一些播放设置

private: // 专用于视频播放的私有成员，每次播放视频都需要重置的变量，比如音视频包队列

public:

public:
private:
    // internal use
    // StreamType 可以使用 & 运算符组合多个类型进行查找存在的类型流
    // 如：if (streamTypes & StreamTypes::Audio) { ... }
    StreamTypes findStreams(AVFormatContext* formatCtx);

};

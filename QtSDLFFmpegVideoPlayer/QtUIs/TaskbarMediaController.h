#pragma once
#include <memory>
#include <string>
#include <vector>
#include <QIcon>
#include <QString>

struct ITaskbarMediaController {
    struct ThumbBarButton {
        int64_t id = 0;
        QIcon icon;
        QString tipText;
    };
    ~ITaskbarMediaController() = default;
    virtual bool initialize(long winId) = 0;
    virtual void addThumbBarButtons(const std::vector<ThumbBarButton>& btnList) = 0;
    // 根据newBtn的id更新对应按钮的内容
    virtual void updateThumbBarButton(ThumbBarButton newBtn) = 0;
};

class TaskbarMediaController : public ITaskbarMediaController {
    class Impl;
    std::unique_ptr<Impl> pImpl{ nullptr };
public:
    explicit TaskbarMediaController();
    ~TaskbarMediaController();
    virtual bool initialize(long winId) override;
    virtual void addThumbBarButtons(const std::vector<ThumbBarButton>& btnList) override;
    virtual void updateThumbBarButton(ThumbBarButton newBtn) override;
};
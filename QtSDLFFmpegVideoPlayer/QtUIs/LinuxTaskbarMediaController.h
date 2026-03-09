#pragma once
#include "TaskbarMediaController.h"

#ifdef __linux__

class LinuxTaskbarMediaController : public ITaskbarMediaController {
public:
    virtual bool initialize(long winId) override {
        return true;
    }
    virtual void addThumbBarButtons(const std::vector<ThumbBarButton>& btnList) override {

    }
    virtual void updateThumbBarButton(ThumbBarButton newBtn) override {

    }
};

#endif

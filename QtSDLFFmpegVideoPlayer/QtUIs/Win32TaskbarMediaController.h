#pragma once
#include <string>
#include "TaskbarMediaController.h"

#ifdef _WIN32

#include <shobjidl.h>
#include <thumbcache.h>
#include <QIcon>
#include <QPixmap>

class Win32TaskbarMediaController : public ITaskbarMediaController {
private:
    ITaskbarList4* taskbarList;
    HWND windowHandle;
    HICON iconToHICON(const QIcon& icon, int width = 32, int height = 32) {
        if (icon.isNull())
            return nullptr;
        // 获取适合尺寸的pixmap
        QPixmap pixmap = icon.pixmap(width, height);
        QImage image = pixmap.toImage().convertToFormat(QImage::Format_ARGB32);
        // 创建位图信息头
        BITMAPINFO bmi;
        ZeroMemory(&bmi, sizeof(BITMAPINFO));
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = image.width();
        bmi.bmiHeader.biHeight = -image.height();  // 负值表示从上到下的DIB
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;
        bmi.bmiHeader.biSizeImage = image.width() * image.height() * 4;
        // 创建掩码位图（Alpha通道）
        HDC hdc = GetDC(nullptr);
        HBITMAP hColor = CreateDIBitmap(hdc, &bmi.bmiHeader, CBM_INIT,
            image.constBits(),
            &bmi, DIB_RGB_COLORS);
        ReleaseDC(nullptr, hdc);
        if (!hColor)
            return nullptr;
        // 创建掩码位图（1位单色）
        QImage maskImage = image.createAlphaMask();
        HBITMAP hMask = CreateBitmap(image.width(), image.height(), 1, 1, maskImage.constBits());
        // 创建ICONINFO结构
        ICONINFO iconInfo;
        iconInfo.fIcon = TRUE;  // TRUE表示图标，FALSE表示光标
        iconInfo.xHotspot = 0;
        iconInfo.yHotspot = 0;
        iconInfo.hbmMask = hMask;
        iconInfo.hbmColor = hColor;
        // 创建HICON
        HICON hIcon = CreateIconIndirect(&iconInfo);
        // 清理临时资源
        DeleteObject(hColor);
        DeleteObject(hMask);
        return hIcon;
    }
    THUMBBUTTONMASK getMaskFromBtn(const ThumbBarButton& btn) {
        THUMBBUTTONMASK mask = { (THUMBBUTTONMASK)0 };
        if (!btn.icon.isNull())
            mask |= THB_ICON;
        if (btn.tipText.size())
            mask |= THB_TOOLTIP;
        return mask;
    }

    THUMBBUTTON createThumbButton(const ThumbBarButton& btn) {
        THUMBBUTTON thumbBtn{ (THUMBBUTTONMASK)0 };
        thumbBtn.iId = btn.id;
        thumbBtn.hIcon = iconToHICON(btn.icon);
        wcscpy_s(thumbBtn.szTip, btn.tipText.toStdWString().c_str());
        thumbBtn.dwMask = getMaskFromBtn(btn);
        thumbBtn.dwMask |= THB_FLAGS;
        thumbBtn.dwFlags = THBF_ENABLED;
        return thumbBtn;
    }
public:
    bool initialize(HWND hwnd) {
        windowHandle = hwnd;
        HRESULT hr = CoCreateInstance(CLSID_TaskbarList, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&taskbarList));
        if (FAILED(hr))
            return false;
        hr = taskbarList->HrInit();
        return SUCCEEDED(hr);
    }

    virtual bool initialize(long winId) override {
        return initialize(reinterpret_cast<HWND>(winId));
    }

    virtual void addThumbBarButtons(const std::vector<ThumbBarButton>& btnList) override {
        std::vector<THUMBBUTTON> thumbBtns;
        for (const auto& btn : btnList)
            thumbBtns.push_back(createThumbButton(btn));
        if (thumbBtns.empty())
            return;
        HRESULT hr = taskbarList->ThumbBarAddButtons(windowHandle, thumbBtns.size(), thumbBtns.data());
    }

    virtual void updateThumbBarButton(ThumbBarButton newBtn) override {
        auto thumbBtn = createThumbButton(newBtn);
        HRESULT hr = taskbarList->ThumbBarUpdateButtons(windowHandle, 1, &thumbBtn);
    }

    ~Win32TaskbarMediaController() {
        if (taskbarList)
            taskbarList->Release();
    }
}; 

#endif

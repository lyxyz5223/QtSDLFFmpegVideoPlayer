#pragma once
#include <string>

#ifdef _WIN32

#include <shobjidl.h>
#include <thumbcache.h>
#include <QIcon>
#include <QPixmap>


class TaskbarMediaController {
private:
    ITaskbarList3* taskbarList;
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
public:
    bool initialize(HWND hwnd) {
        windowHandle = hwnd;
        HRESULT hr = CoCreateInstance(CLSID_TaskbarList, NULL,
            CLSCTX_INPROC_SERVER,
            IID_PPV_ARGS(&taskbarList));
        if (SUCCEEDED(hr)) {
            hr = taskbarList->HrInit();
            return SUCCEEDED(hr);
        }
        return false;
    }

    void addThumbBarButtons() {
        THUMBBUTTON buttons[3] = {};

        // 上一首按钮
        buttons[0].dwMask = THB_ICON | THB_TOOLTIP | THB_FLAGS;
        buttons[0].iId = 0;
        buttons[0].hIcon = iconToHICON(QIcon(":/svgs/svgs/skip-prev.svg"));
        wcscpy_s(buttons[0].szTip, L"上一首");
        buttons[0].dwFlags = THBF_ENABLED;

        // 播放/暂停按钮
        buttons[1].dwMask = THB_ICON | THB_TOOLTIP | THB_FLAGS;
        buttons[1].iId = 1;
        buttons[1].hIcon = iconToHICON(QIcon(":/svgs/svgs/play.svg"));
        wcscpy_s(buttons[1].szTip, L"播放/暂停");
        buttons[1].dwFlags = THBF_ENABLED;

        // 下一首按钮
        buttons[2].dwMask = THB_ICON | THB_TOOLTIP | THB_FLAGS;
        buttons[2].iId = 2;
        buttons[2].hIcon = iconToHICON(QIcon(":/svgs/svgs/skip-next.svg"));
        wcscpy_s(buttons[2].szTip, L"下一首");
        buttons[2].dwFlags = THBF_ENABLED;

        HRESULT hr = taskbarList->ThumbBarAddButtons(windowHandle, 3, buttons);
        if (SUCCEEDED(hr)) {
            // 按钮添加成功
        }
    }

    ~TaskbarMediaController() {
        if (taskbarList) {
            taskbarList->Release();
        }
    }
}; 

#else

#endif

#include "QtUIs/QtSDLFFmpegVideoPlayer.h"
#include <QtWidgets/QApplication>
#include <SDLApp.h>
#include <SDL3/SDL_main.h>
#include <Logger.h>

class FFmpegInfo
{
    mutable Logger logger{ "FFmpegFilterInfo" };
public:
    FFmpegInfo() {}
    void printAllFilterName() const {
        const AVFilter* filter = nullptr;
        void* opaque = nullptr;
        logger.info("Built-in filters: ");
        while (filter = av_filter_iterate(&opaque))
        {
            std::string logText;
            if (filter->name)
                logText += std::string("  ") + filter->name;
            if (filter->description)
                logText += std::string(" - ") + filter->description;
            logger.info(logText);
        }
    }
    void printAllHwDecoders() const {
        MediaDecodeUtils::listAllHardwareDecoders(&logger);
    }
};

int main(int argc, char *argv[])
{
    Logger logger{ "main" };
    SDLApp::init(argc, argv, SDL_INIT_VIDEO);
    QApplication a(argc, argv);
    FFmpegInfo filterInfo;
    filterInfo.printAllFilterName();
    filterInfo.printAllHwDecoders();
    QtSDLFFmpegVideoPlayer w;
    w.setArgcArgv(argc, argv);
    w.show();
    return a.exec();
}

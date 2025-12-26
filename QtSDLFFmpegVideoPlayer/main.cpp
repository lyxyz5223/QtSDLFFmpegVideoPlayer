#include "QtUIs/QtSDLFFmpegVideoPlayer.h"
#include <QtWidgets/QApplication>
#include <SDLApp.h>
#include <SDL3/SDL_main.h>

int main(int argc, char *argv[])
{
    SDLApp::init(argc, argv, SDL_INIT_VIDEO);
    QApplication a(argc, argv);
    QtSDLFFmpegVideoPlayer w;
    w.setArgcArgv(argc, argv);
    w.show();
    return a.exec();
}

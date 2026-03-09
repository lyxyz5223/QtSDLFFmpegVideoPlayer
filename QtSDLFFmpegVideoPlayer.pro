QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

QT += svg multimedia multimediawidgets

CONFIG += c++20

# 宏定义
DEFINES += HAVE_PORTAUDIO HAVE_RTAUDIO
DEFINES += USE_QT_MULTIMEDIA_WIDGET # USE_SDL_WIDGET # USE_QT_MULTIMEDIA_WIDGET
DEFINES += # USE_STD_FORMAT

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0
INCLUDEPATH += ../Libraries/ConcurrentQueue
INCLUDEPATH += ./QtSDLFFmpegVideoPlayer
INCLUDEPATH += \
    ./QtSDLFFmpegVideoPlayer/Utils \
    ./QtSDLFFmpegVideoPlayer/Audio/AudioAdapter \
    ./QtSDLFFmpegVideoPlayer/Audio/VolumeController \
    ./QtSDLFFmpegVideoPlayer/Players \
    ./QtSDLFFmpegVideoPlayer/SDLUtils \
    ./QtSDLFFmpegVideoPlayer/QtUtils \
    ./QtSDLFFmpegVideoPlayer/QtUIs \
    ./QtSDLFFmpegVideoPlayer/Logger


SOURCES += \
    QtSDLFFmpegVideoPlayer/main.cpp \
    QtSDLFFmpegVideoPlayer/SDLUtils/SDLApp.cpp \
    QtSDLFFmpegVideoPlayer/SDLUtils/SDLMediaPlayer.cpp \
    QtSDLFFmpegVideoPlayer/QtUtils/QtMediaPlayer.cpp \
    QtSDLFFmpegVideoPlayer/QtUIs/AnimatedMenu.cpp \
    QtSDLFFmpegVideoPlayer/QtUIs/AnimatedMenuAction.cpp \
    QtSDLFFmpegVideoPlayer/QtUIs/PlayerOptionsWidget.cpp \
    QtSDLFFmpegVideoPlayer/QtUIs/PlayListListView.cpp \
    QtSDLFFmpegVideoPlayer/QtUIs/PlayListWidget.cpp \
    QtSDLFFmpegVideoPlayer/QtUIs/QtSDLFFmpegVideoPlayer.cpp \
    QtSDLFFmpegVideoPlayer/QtUIs/RoundedIconButton.cpp \
    QtSDLFFmpegVideoPlayer/QtUIs/SDLWidget.cpp \
    QtSDLFFmpegVideoPlayer/QtUIs/TaskbarMediaController.cpp \
    QtSDLFFmpegVideoPlayer/Players/PlayerPredefine.cpp \
    QtSDLFFmpegVideoPlayer/Players/AudioPlayer.cpp \
    QtSDLFFmpegVideoPlayer/Players/VideoPlayer.cpp \
    QtSDLFFmpegVideoPlayer/Players/MediaPlayer.cpp \
    QtSDLFFmpegVideoPlayer/Audio/AudioAdapter/AudioAdapter.cpp \
    QtSDLFFmpegVideoPlayer/Audio/VolumeController/SystemVolumeController.cpp

HEADERS += \
    QtSDLFFmpegVideoPlayer/Utils/AtomicWaitObject.h \
    QtSDLFFmpegVideoPlayer/Utils/COMUtils.h \
    QtSDLFFmpegVideoPlayer/Utils/EnumDefine.h \
    QtSDLFFmpegVideoPlayer/Utils/MultiEnumTypeDefine.h \
    QtSDLFFmpegVideoPlayer/Utils/ThreadUtils.h \
    QtSDLFFmpegVideoPlayer/SDLUtils/SDLApp.h \
    QtSDLFFmpegVideoPlayer/SDLUtils/SDLMediaPlayer.h \
    QtSDLFFmpegVideoPlayer/QtUtils/QtMediaPlayer.h \
    QtSDLFFmpegVideoPlayer/QtUIs/AnimatedMenu.h \
    QtSDLFFmpegVideoPlayer/QtUIs/AnimatedMenuAction.h \
    QtSDLFFmpegVideoPlayer/QtUIs/PlayerOptionsWidget.h \
    QtSDLFFmpegVideoPlayer/QtUIs/PlayListListView.h \
    QtSDLFFmpegVideoPlayer/QtUIs/PlayListWidget.h \
    QtSDLFFmpegVideoPlayer/QtUIs/QtSDLFFmpegVideoPlayer.h \
    QtSDLFFmpegVideoPlayer/QtUIs/RoundedIconButton.h \
    QtSDLFFmpegVideoPlayer/QtUIs/SDLWidget.h \
    QtSDLFFmpegVideoPlayer/QtUIs/TaskbarMediaController.h \
    QtSDLFFmpegVideoPlayer/Players/PlayerPredefine.h \
    QtSDLFFmpegVideoPlayer/Players/AudioPlayer.h \
    QtSDLFFmpegVideoPlayer/Players/VideoPlayer.h \
    QtSDLFFmpegVideoPlayer/Players/MediaPlayer.h \
    QtSDLFFmpegVideoPlayer/Logger/LoggerPredefine.h \
    QtSDLFFmpegVideoPlayer/Audio/AudioAdapter/AudioAdapter.h \
    QtSDLFFmpegVideoPlayer/Audio/VolumeController/SystemVolumeController.h

RESOURCES += QtSDLFFmpegVideoPlayer/resources/QtSDLFFmpegVideoPlayer.qrc

FORMS += \
    QtSDLFFmpegVideoPlayer/QtUIs/PlayListWidget.ui \
    QtSDLFFmpegVideoPlayer/QtUIs/PlayerOptionsWidget.ui \
    QtSDLFFmpegVideoPlayer/QtUIs/QtSDLFFmpegVideoPlayer.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

# 库

win32:CONFIG(release, debug|release): LIBS += -L$$PWD/../Libraries/SDL3/lib/x64/ -lSDL3
else:win32:CONFIG(debug, debug|release): LIBS += -L$$PWD/../Libraries/SDL3/lib/x64/ -lSDL3
else:unix: LIBS += -L$$PWD/../Libraries/SDL3/lib/linux/ -lSDL3

INCLUDEPATH += $$PWD/../Libraries/SDL3/include
DEPENDPATH += $$PWD/../Libraries/SDL3/include


win32:CONFIG(release, debug|release): LIBS += -L$$PWD/../Libraries/ffmpeg-shared/lib/  -lavcodec -lavdevice -lavfilter -lavformat -lavutil -lswresample -lswscale

else:win32:CONFIG(debug, debug|release): LIBS += -L$$PWD/../Libraries/ffmpeg-shared/lib/  -lavcodec -lavdevice -lavfilter -lavformat -lavutil -lswresample -lswscale
# else:unix: LIBS += -L$$PWD/../Libraries/ffmpeg-shared/lib/ -l:libavcodec.dll.a -l:libavdevice.dll.a -l:libavfilter.dll.a -l:libavformat.dll.a -l:libavutil.dll.a -l:libswresample.dll.a -l:libswscale.dll.a
else:unix: LIBS += -L$$PWD/../Libraries/ffmpeg-shared/lib/linux/ -lavcodec -lavdevice -lavfilter -lavformat -lavutil -lswresample -lswscale

INCLUDEPATH += $$PWD/../Libraries/ffmpeg-shared/include
DEPENDPATH += $$PWD/../Libraries/ffmpeg-shared/include


win32:CONFIG(release, debug|release): LIBS += -L$$PWD/../Libraries/rtaudio-6.0.1/build/Release/ -lrtaudio
else:win32:CONFIG(debug, debug|release): LIBS += -L$$PWD/../Libraries/rtaudio-6.0.1/build/Debug/ -lrtaudiod
else:unix: LIBS += -L$$PWD/../Libraries/rtaudio-6.0.1/buildLinux/ -lrtaudio

INCLUDEPATH += $$PWD/../Libraries/rtaudio-6.0.1
DEPENDPATH += $$PWD/../Libraries/rtaudio-6.0.1

win32:CONFIG(release, debug|release): LIBS += -L$$PWD/../Libraries/portaudio-19.7.0/build/msvc/x64/release/ -lportaudio_x64
else:win32:CONFIG(debug, debug|release): LIBS += -L$$PWD/../Libraries/portaudio-19.7.0/build/msvc/x64/debug/ -lportaudio_x64
else:unix: LIBS += -L$$PWD/../Libraries/portaudio-19.7.0/build/cmake/ -lportaudio

INCLUDEPATH += $$PWD/../Libraries/portaudio-19.7.0/include
DEPENDPATH += $$PWD/../Libraries/portaudio-19.7.0/include

LIBS += -lpthread

win32:CONFIG(release, debug|release): LIBS += -L$$PWD/../Libraries/Logger/build/MinGWx64/ -llibLogger
else:win32:CONFIG(debug, debug|release): LIBS += -L$$PWD/../Libraries/Logger/build/MinGWx64/ -llibLogger_d
else:unix: LIBS += -L$$PWD/../Libraries/Logger/build/MinGWx64/ -lLogger

INCLUDEPATH += $$PWD/../Libraries/Logger/include
DEPENDPATH += $$PWD/../Libraries/Logger/include

DISTFILES +=

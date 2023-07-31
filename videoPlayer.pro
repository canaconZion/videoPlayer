#-------------------------------------------------
#
# Project created by QtCreator 2023-07-08T14:40:10
#
#-------------------------------------------------

QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = videoPlayer
TEMPLATE = app

# The following define makes your compiler emit warnings if you use
# any feature of Qt which as been marked as deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if you use deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0


SOURCES += \
        main.cpp \
    demuxworker.cpp \
    mediawindow.cpp

HEADERS += \
    demuxworker.h \
    mediawindow.h

FORMS += \
    mediawindow.ui

INCLUDEPATH += D:\soft\qt\project\videoPlayer\lib\win\ffmpeg\include
INCLUDEPATH += D:\soft\qt\project\videoPlayer\lib\win\SDL2\include
#INCLUDEPATH += /usr/include/SDL2/

LIBS += -LD:\soft\qt\project\videoPlayer\lib\win\ffmpeg\lib\x86 -lswresample -lavformat -lswscale -lavutil  -lavcodec  -lavdevice -lavfilter
LIBS += -LD:\soft\qt\project\videoPlayer\lib\win\SDL2\lib\x86
LIBS += -lpthread

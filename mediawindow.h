#ifndef MEDIAWINDOW_H
#define MEDIAWINDOW_H

#include <QWidget>
#include <QLabel>
#include <QDebug>
#include <QPushButton>
#include <QString>
#include <QSlider>
#include "demuxworker.h"
extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/time.h>
#include <libswresample/swresample.h>
#include <SDL.h>
#include <SDL_video.h>
#include <SDL_render.h>
#include <SDL_rect.h>
}

class MediaWindow : public QWidget
{
    Q_OBJECT
public:
    explicit MediaWindow(QWidget *parent = nullptr);
    ~MediaWindow() ;

    QString gVideoFilePath;
    QString selectedFile;
    QLabel *imgLabel;
    QRect rect;
    QSlider *playSlider;
    QPushButton *startButton;
    QPushButton *pauseButton;
    QPushButton *stopButton;
    QPushButton *chooseButton;
    long pos;
private:
    QThread *demuxThread;
    DemuxWorker *demuxWorker;
    SDL_Renderer *sdlRenderer;
    SDL_Texture *sdlTexture;
    SDL_Window *sdlWindow;
    int freeSdl();

signals:
    void sigStartPlay(QString file);

public slots:
    void startPlay();
    void pausePlay();
    void stopPlay();
    void selectFile();
    void updateVideo(AVFrame *pFrame);
    int initSdl(int mWidth,int mHeight);
    void updateSlider(long TotalTime, long currentTime);
    void doSeek();

};

#endif // MEDIAWINDOW_H

#ifndef PALY_WINDOW_H
#define PALY_WINDOW_H

#include <QWidget>
#include <QPushButton>
#include <QSlider>
#include <QLabel>
#include <QString>
#include <QThread>
#include "decode_video_thread.h"
#include <QInputDialog>
extern "C"
{
#include "SDL.h"
#include "SDL_video.h"
#include "SDL_rect.h"
#include "SDL_render.h"
}

namespace Ui {
class paly_window;
}

class paly_window : public QWidget
{
    Q_OBJECT

public:
    explicit paly_window(QWidget *parent = 0);
    ~paly_window();
    QString source_file; // 视频源
    QString source_path; // 初始目录
    QLabel *imgLabel;
    QPushButton *play;
    QPushButton *pause;
    QPushButton *select;
    QPushButton *quit;
    QPushButton *net_butt;
    QSlider *video_slider;
    QLabel *curr_time;
    QLabel *total_time;
    QString totalTime;
    QString c_timeStr;


private:
    Ui::paly_window *ui;
    QThread *decode_thread;
    DecodeThread *do_decode;
    SDL_Renderer *sdlRenderer;
    SDL_Texture *sdlTexture;
    SDL_Window *sdlWindow;
    int freeSdl();

signals:
    void sigStartPlay(QString file);

public slots:
    void startPlay();
    void pausePlay();
    void quitPlay();
    void selectFile();
    void inputNetUrl();
    void updateVideo(AVFrame *pFrame);
    int initSdl(int mWidth,int mHeight);
    void updateSlider(long TotalTime, long currentTime);
};

#endif // PALY_WINDOW_H

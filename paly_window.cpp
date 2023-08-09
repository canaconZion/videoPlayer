#include "paly_window.h"
#include "ui_paly_window.h"
#include "decode_video_thread.h"
#include <QDebug>
#include <QFileDialog>

paly_window::paly_window(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::paly_window)
{
    ui->setupUi(this);
    play = ui->play;
    pause = ui->pause;
    select = ui->select;
    quit = ui->quit;
    imgLabel = ui->playback;
    video_slider = ui ->videoSlider;

    connect(select,&QPushButton::clicked,this,&paly_window::selectFile);
    connect(play,&QPushButton::clicked,this,&paly_window::startPlay);
    connect(pause,&QPushButton::clicked,this,&paly_window::pausePlay);
    connect(quit,&QPushButton::clicked,this,&paly_window::quitPlay);

    decode_thread = new QThread;
    do_decode = new DecodeThread;
    do_decode -> moveToThread(decode_thread);

    connect(decode_thread,&QThread::finished,do_decode,&QObject::deleteLater);
    connect(do_decode,SIGNAL(sigGetCurrentPts(long, long)),this,SLOT(updateSlider(long, long)));
    connect(do_decode,SIGNAL(sigGetVideoInfo(int,int)),this,SLOT(initSdl(int ,int)));
    connect(do_decode,SIGNAL(sigGetFrame(AVFrame *)),this,SLOT(updateVideo(AVFrame *)));
    connect(this,SIGNAL(sigStartPlay(QString)),do_decode,SLOT(slotDoWork(QString)));

}

paly_window::~paly_window()
{
    delete ui;
}

void paly_window::selectFile()
{
    source_path = "D:/video/videos";
    QString s = QFileDialog::getOpenFileName(
                this, QStringLiteral("选择要播放的文件"),
                source_path,
                QStringLiteral("视频文件 (*.flv *.rmvb *.avi *.MP4 *.mkv);;")
                +QStringLiteral("音频文件 (*.mp3 *.wma *.wav);;")
                +QStringLiteral("所有文件 (*.*)"));
    if (!s.isEmpty())
    {
        source_file = s;
        qDebug() << source_file;
    }

}

void paly_window::startPlay()
{

}

void paly_window::pausePlay()
{

}

void paly_window::quitPlay()
{

}
void paly_window::updateVideo(AVFrame *pFrame)
{

}

void paly_window::updateSlider(long TotalTime, long currentTime)
{

}

int paly_window::initSdl(int mWidth,int mHeight) {

    if (SDL_Init(SDL_INIT_EVERYTHING) < 0)
    {
        return -1;
    }
    // 创建窗体
    sdlWindow = SDL_CreateWindowFrom((void *)imgLabel->winId());

    if (sdlWindow == nullptr)
    {

        return -1;
    }
    // 从窗体创建渲染器
    sdlRenderer = SDL_CreateRenderer(sdlWindow, -1, 0);
    // 创建渲染器纹理
    sdlTexture = SDL_CreateTexture(sdlRenderer, SDL_PIXELFORMAT_IYUV, SDL_TEXTUREACCESS_STREAMING, mWidth, mHeight);
}

int paly_window::freeSdl() {
    SDL_DestroyRenderer(sdlRenderer);
    SDL_DestroyTexture(sdlTexture);
    SDL_DestroyWindow(sdlWindow);
    SDL_Quit();
    return 0;
}

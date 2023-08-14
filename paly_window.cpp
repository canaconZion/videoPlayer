#include "paly_window.h"
#include "ui_paly_window.h"
#include "decode_video_thread.h"
#include <QDebug>
#include <QFileDialog>
#include <QMessageBox>
#include <QTimer>

paly_window::paly_window(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::paly_window)
{
    ui->setupUi(this);
    play = ui->play;
    pause = ui->pause;
    select = ui->select;
    net_butt = ui->netStream;
    quit = ui->quit;
    imgLabel = ui->playback;
    curr_time = ui->curr_time;
    total_time = ui->total_time;

    video_slider = ui ->videoSlider;
    video_slider->setRange(0,10000);

    connect(select,&QPushButton::clicked,this,&paly_window::selectFile);
    connect(net_butt,&QPushButton::clicked,this,&paly_window::inputNetUrl);
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
    freeSdl();
    delete ui;
}

void paly_window::selectFile()
{
    //    play->setEnabled(true);
    do_decode->isPause = true;
    pause->setText("继续");
    source_path = "D:/video/videos";
    QString s = QFileDialog::getOpenFileName(
                this, QStringLiteral("选择要播放的文件"),
                source_path,
                QStringLiteral("视频文件 (*.flv *.rmvb *.avi *.MP4 *.mkv);;")
                +QStringLiteral("音频文件 (*.mp3 *.wma *.wav);;")
                +QStringLiteral("所有文件 (*.*)"));
    if (!s.isEmpty())
    {
        do_decode->isPlay = false;
        source_file = s;
        qDebug() << source_file;
        startPlay();
    }
    else
    {
        pausePlay();
    }
    //    select->setEnabled(false);


}

void paly_window::inputNetUrl()
{
    do_decode->isPause = true;
    pause->setText("继续");
    bool ok;
    QString text = QInputDialog::getText(nullptr, "打开媒体", "请输入网络URL:", QLineEdit::Normal, QString(), &ok);

    if (ok && !text.isEmpty()) {
        qDebug() << "User input: " << text;
        do_decode->isPlay = false;
        source_file = text;
        startPlay();
    } else {
        qDebug() << "User cancelled the input.";
        pausePlay();
    }

}

void paly_window::startPlay()
{
    pausePlay();
    decode_thread->start();
    qDebug() << "文件：" << source_file;
    emit sigStartPlay(source_file);
    play->setEnabled(false);
}

void paly_window::pausePlay()
{
    if(do_decode->isPause){
        do_decode->isPause = false;
        pause->setText("暂停");
    }
    else{
        do_decode->isPause = true;
        pause->setText("继续");
    }
}

void paly_window::quitPlay()
{
    QMessageBox::StandardButton button=QMessageBox::information(this,"提示","退出",QMessageBox::Ok,QMessageBox::Cancel);
    if(button==QMessageBox::Ok){
        do_decode->isPlay = false;
        QEventLoop loop;
        QTimer::singleShot(1.5*1000,&loop,SLOT(quit()));
        QTimer::singleShot(2*1000,this,SLOT(close()));
        loop.exec();
    }
    qDebug()<< "stop play";
}
void paly_window::updateVideo(AVFrame *pFrame)
{
    SDL_UpdateYUVTexture(sdlTexture, NULL, pFrame->data[0], pFrame->linesize[0],
            pFrame->data[1], pFrame->linesize[1], pFrame->data[2],
            pFrame->linesize[2]);
    SDL_RenderClear(sdlRenderer);
    SDL_RenderCopy(sdlRenderer, sdlTexture, NULL, NULL);
    SDL_RenderPresent(sdlRenderer);
}

void paly_window::updateSlider(long TotalTime, long currentTime)
{
    QString hStr = QString("0%1").arg(TotalTime/3600);
    QString mStr = QString("0%1").arg(TotalTime / 60 % 60);
    QString sStr = QString("0%1").arg(TotalTime % 60);
    totalTime = QString("%1:%2:%3").arg(hStr).arg(mStr.right(2)).arg(sStr.right(2));
    total_time->setText(totalTime);
    QString c_hStr = QString("0%1").arg((currentTime)/3600);
    QString c_mStr = QString("0%1").arg((currentTime)/ 60 % 60);
    QString c_sStr = QString("0%1").arg((currentTime)% 60);
    QString c_timeStr = QString("%1:%2:%3").arg(c_hStr).arg(c_mStr.right(2)).arg(c_sStr.right(2));
    curr_time->setText(c_timeStr);
    double rate = (currentTime+1)*10000/TotalTime;
    //    qDebug() << "Total time" << TotalTime;
    //    qDebug() << "Current time" << currentTime;
    //    qDebug() << "=====================================";
    if (!video_slider->isSliderDown())
    {
        video_slider->setValue(rate);
    }
}

int paly_window::initSdl(int mWidth,int mHeight) {

    if (SDL_Init(SDL_INIT_EVERYTHING) < 0)
    {
        return -1;
    }
    sdlWindow = SDL_CreateWindowFrom((void *)imgLabel->winId());

    if (sdlWindow == nullptr)
    {

        return -1;
    }
    sdlRenderer = SDL_CreateRenderer(sdlWindow, -1, 0);
    sdlTexture = SDL_CreateTexture(sdlRenderer, SDL_PIXELFORMAT_IYUV, SDL_TEXTUREACCESS_STREAMING, mWidth, mHeight);
}

int paly_window::freeSdl() {
    SDL_DestroyRenderer(sdlRenderer);
    SDL_DestroyTexture(sdlTexture);
    SDL_DestroyWindow(sdlWindow);
    SDL_Quit();
    return 0;
}

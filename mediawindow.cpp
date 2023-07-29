#include "mediawindow.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QMessageBox>
#include <QCloseEvent>
#include <QEventLoop>
#include <QTimer>
#include <QFileDialog>

MediaWindow::MediaWindow(QWidget *parent) : QWidget(parent)
{

    this->setFixedSize(661,465);
    startButton = new QPushButton("start");
    pauseButton = new QPushButton("pause");
    stopButton = new QPushButton("stop");
    chooseButton = new QPushButton("choose");
    playSlider = new QSlider(this);
    playSlider->setRange(0,10000);
    playSlider->setValue(0);
    playSlider ->setOrientation(Qt::Horizontal);
//    playSlider->setTickPosition(QSlider::T);
    imgLabel = new QLabel(this);
    imgLabel->resize(601,381);
    imgLabel->move(30,20);
    rect = imgLabel->geometry();//记录widget位置，恢复时使用

    demuxThread = new QThread;
    demuxWorker = new DemuxWorker;
    demuxWorker->moveToThread(demuxThread);


    connect(demuxThread, &QThread::finished, demuxWorker, &QObject::deleteLater);
    connect(demuxWorker,SIGNAL(sigGetCurrentPts(long, long)),this,SLOT(updateSlider(long, long)));
    connect(demuxWorker,SIGNAL(sigGetVideoInfo(int,int)),this,SLOT(initSdl(int ,int)));
    connect(demuxWorker,SIGNAL(sigGetFrame(AVFrame *)),this,SLOT(updateVideo(AVFrame *)));
    connect(this,SIGNAL(sigStartPlay(QString)),demuxWorker,SLOT(slotDoWork(QString)));

    QHBoxLayout *buttonLayout = new QHBoxLayout;
    buttonLayout->addWidget(chooseButton);
    buttonLayout->addWidget(startButton);
    buttonLayout->addWidget(pauseButton);
    buttonLayout->addWidget(stopButton);

    QHBoxLayout *silderLayout = new QHBoxLayout;
    silderLayout->addWidget(playSlider);
    QVBoxLayout *playerLayout = new QVBoxLayout;
    playerLayout->addWidget(imgLabel);
    playerLayout->addLayout(silderLayout);
    playerLayout->addLayout(buttonLayout);

    this->setLayout(playerLayout);

    connect(chooseButton,SIGNAL(clicked(bool)), this, SLOT(selectFile()));
    connect(startButton,SIGNAL(clicked(bool)),this,SLOT(startPlay()));
    connect(pauseButton,SIGNAL(clicked(bool)),this,SLOT(pausePlay()));
    connect(stopButton,&QPushButton::clicked,this,&MediaWindow::stopPlay);
    connect(playSlider, SIGNAL(sliderReleased()), this, SLOT(doSeek()));

}

MediaWindow::~MediaWindow()
{
    freeSdl();
}
void MediaWindow::startPlay(){

    demuxThread->start();
    qDebug() << "文件：" << selectedFile;
    emit sigStartPlay(selectedFile);
    startButton->setEnabled(false);
}


void MediaWindow::pausePlay(){
    if(demuxWorker->isPause)
    {
        demuxWorker->isPause = false;
        pauseButton->setText("pause");
    }
    else
    {
        demuxWorker->isPause = true;
        pauseButton ->setText("continue");
    }
}

void MediaWindow::stopPlay(){
    QMessageBox::StandardButton button=QMessageBox::information(this,"提示","退出",QMessageBox::Ok,QMessageBox::Cancel);
    if(button==QMessageBox::Ok){
        demuxWorker->isPlay = false;
        QEventLoop loop;
        QTimer::singleShot(1.5*1000,&loop,SLOT(quit()));
        QTimer::singleShot(2*1000,this,SLOT(close()));
        loop.exec();
    }
    qDebug()<< "stop play";
}

void MediaWindow::selectFile(){
    gVideoFilePath = "D:/video/videos";
    QString s = QFileDialog::getOpenFileName(
                this, QStringLiteral("选择要播放的文件"),
                gVideoFilePath,//初始目录
                QStringLiteral("视频文件 (*.flv *.rmvb *.avi *.MP4 *.mkv);;")
                +QStringLiteral("音频文件 (*.mp3 *.wma *.wav);;")
                +QStringLiteral("所有文件 (*.*)"));
    qDebug() << s;
    if (!s.isEmpty())
    {
        selectedFile = s;
        qDebug() << selectedFile;
    }
}

void MediaWindow::doSeek()
{
    qDebug()<< "DO SEEK" ;
    pos = (float)playSlider->value() / (float)(playSlider->maximum() + 1);
    demuxWorker->seekPos = pos;
    demuxWorker->doSeek = true;
}


void MediaWindow::updateSlider(long totalTime, long currentTime)
{
//    qDebug()<< "Total time:" << totalTime;
//    qDebug()<< "current time:" << currentTime;
    double rate = currentTime*10000/totalTime;
    qDebug()<< "rate:" << rate ;
    if (!playSlider->isSliderDown())
    {
        playSlider->setValue(rate);
    }
}

int MediaWindow::initSdl(int mWidth,int mHeight) {

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

int MediaWindow::freeSdl() {
    SDL_DestroyRenderer(sdlRenderer);
    SDL_DestroyTexture(sdlTexture);
    SDL_DestroyWindow(sdlWindow);
    SDL_Quit();
    return 0;
}

void MediaWindow::updateVideo(AVFrame *pFrame){
    SDL_UpdateYUVTexture(sdlTexture, NULL, pFrame->data[0], pFrame->linesize[0],
            pFrame->data[1], pFrame->linesize[1], pFrame->data[2],
            pFrame->linesize[2]);
    SDL_RenderClear(sdlRenderer);
    SDL_RenderCopy(sdlRenderer, sdlTexture, NULL, NULL);
    SDL_RenderPresent(sdlRenderer);
}

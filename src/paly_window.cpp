#include "paly_window.h"
#include "ui_paly_window.h"
#include "decode_video_thread.h"
#include <QDebug>
#include <QFileDialog>
#include <QMessageBox>
#include <QTimer>
#include <QWidget>
#include <QMenu>
#include <QAction>
#include <QContextMenuEvent>

paly_window::paly_window(QWidget *parent) : QWidget(parent),
    ui(new Ui::paly_window)
{
    ui->setupUi(this);

    createMenu();

    sdlRenderer = nullptr;
    sdlTexture = nullptr;

    play = ui->play;
    pause = ui->pause;
    select = ui->select;
    net_butt = ui->netStream;
    quit = ui->quit;
    imgLabel = ui->playback;
    curr_time = ui->curr_time;
    total_time = ui->total_time;

    video_slider = ui->videoSlider;
    video_slider->setRange(0, 1000);

    // 在imgLabel注册完成后，初始化SDL
    initSdl();

    connect(select, &QPushButton::clicked, this, &paly_window::selectFile);
    connect(net_butt, &QPushButton::clicked, this, &paly_window::inputNetUrl);
    connect(play, &QPushButton::clicked, this, &paly_window::startPlay);
    connect(pause, &QPushButton::clicked, this, &paly_window::pausePlay);
    connect(quit, &QPushButton::clicked, this, &paly_window::quitPlay);
    connect(video_slider, &QSlider::sliderReleased, this, &paly_window::doSeek);

    decode_thread = new QThread;
    //    do_decode = new DecodeThread;
    do_decode = new VideoPlayer;
    do_decode->moveToThread(decode_thread);

    connect(decode_thread, &QThread::finished, do_decode, &QObject::deleteLater);
    connect(do_decode, SIGNAL(sigGetCurrentPts(long, long)), this, SLOT(updateSlider(long, long)));
    connect(do_decode, SIGNAL(sigGetVideoInfo(int, int)), this, SLOT(settingSdl(int, int)));
    connect(do_decode, SIGNAL(sigGetFrame(AVFrame *)), this, SLOT(updateVideo(AVFrame *)));
    connect(this, SIGNAL(sigStartPlay(QString)), do_decode, SLOT(start_play(QString)));
}

paly_window::~paly_window()
{
    freeSdl();
    delete ui;
}

void paly_window::selectFile()
{
    //    play->setEnabled(true);
    do_decode->v_pause = true;
    pause->setText("继续");
    source_path = "D:/video/videos";
    QString s = QFileDialog::getOpenFileName(
                this, QStringLiteral("选择要播放的文件"),
                source_path,
                QStringLiteral("视频文件 (*.flv *.rmvb *.avi *.MP4 *.mkv);;") + QStringLiteral("音频文件 (*.mp3 *.wma *.wav);;") + QStringLiteral("所有文件 (*.*)"));
    if (!s.isEmpty())
    {
        do_decode->v_play = false;
        source_file = s;
        qDebug() << source_file;
        checkSdl();
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
    do_decode->v_pause = true;
    pause->setText("继续");
    bool ok;
    QString text = QInputDialog::getText(nullptr, "打开媒体", "请输入网络URL:", QLineEdit::Normal, QString(), &ok);

    if (ok && !text.isEmpty())
    {
        qDebug() << "User input: " << text;
        do_decode->v_play = false;
        source_file = text;
        checkSdl();
        startPlay();
    }
    else
    {
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
    if (do_decode->v_pause)
    {
        do_decode->v_pause = false;
        pause->setText("暂停⏸");
    }
    else
    {
        do_decode->v_pause = true;
        pause->setText("继续▶");
    }
}

void paly_window::quitPlay()
{
    QMessageBox::StandardButton button = QMessageBox::information(this, "提示", "退出", QMessageBox::Ok, QMessageBox::Cancel);
    if (button == QMessageBox::Ok)
    {
        do_decode->v_play = false;
        QEventLoop loop;
        QTimer::singleShot(1.5 * 1000, &loop, SLOT(quit()));
        QTimer::singleShot(2 * 1000, this, SLOT(close()));
        loop.exec();
    }
    qDebug() << "stop play";
}
void paly_window::updateVideo(AVFrame *pFrame)
{
    if(!pFrame)
    {
        qDebug() << "Bad frame";
        return;
    }
    if (!pFrame->data[0] || !pFrame->data[1] || !pFrame->data[2] ||
            pFrame->linesize[0] < 0 || pFrame->linesize[1] < 0 || pFrame->linesize[2] < 0) {
        qCritical() << "Bad frame";
        qDebug() << "Bad frame";
        return;
    }
    qDebug() << "update frame";
    if (SDL_UpdateYUVTexture(sdlTexture, NULL, pFrame->data[0], pFrame->linesize[0],
                             pFrame->data[1], pFrame->linesize[1], pFrame->data[2],
                             pFrame->linesize[2]) < 0) {
        fprintf(stderr, "SDL_UpdateYUVTexture error: %s\n", SDL_GetError());
        // 在此处理错误，例如释放资源并退出
    }

    SDL_RenderClear(sdlRenderer);
    SDL_RenderCopy(sdlRenderer, sdlTexture, NULL, NULL);
    SDL_RenderPresent(sdlRenderer);

}

void paly_window::updateSlider(long TotalTime, long currentTime)
{
    updateTimeLabel(TotalTime,currentTime);
    double rate = (currentTime + 1) * 1000 / TotalTime;
    //        qDebug() << "Total time" << TotalTime;
    //        qDebug() << "Current time" << currentTime;
    //        qDebug() << "=====================================";
    if (!video_slider->isSliderDown())
    {
        video_slider->setValue(rate);
    }
}

int paly_window::initSdl()
{

    if (SDL_Init(SDL_INIT_EVERYTHING) < 0)
    {
        return -1;
    }
    sdlWindow = SDL_CreateWindowFrom((void *)imgLabel->winId());

    if (sdlWindow == nullptr)
    {

        return -1;
    }
    qDebug() << "++++++++++Init SDL+++++++++++++";
}

void paly_window::settingSdl(int mWidth, int mHeight)
{
    video_height = QString::number(mHeight);
    video_width = QString::number(mWidth);
    sdlRenderer = SDL_CreateRenderer(sdlWindow, -1, 0);
    sdlTexture = SDL_CreateTexture(sdlRenderer, SDL_PIXELFORMAT_IYUV, SDL_TEXTUREACCESS_STREAMING, mWidth, mHeight);
    qDebug() << "++++++++++Update SDL+++++++++++++";
}

void paly_window::checkSdl()
{
    if(sdlRenderer != nullptr)
    {
        qDebug() << "clear sdl";
        SDL_DestroyRenderer(sdlRenderer);
    }
    if(sdlTexture != nullptr)
    {
        qDebug() << "clear text";
        SDL_DestroyTexture(sdlTexture);
    }
}

int paly_window::freeSdl()
{
    SDL_DestroyRenderer(sdlRenderer);
    SDL_DestroyTexture(sdlTexture);
    SDL_DestroyWindow(sdlWindow);
    SDL_Quit();
    return 0;
}

void paly_window::createMenu()
{
    QAction *m_ActionSelect = new QAction(tr("选择播放文件"), this);
    QAction *m_ActionNet = new QAction(tr("播放网络流"), this);
    QAction *m_ActionVideoMsg = new QAction(tr("查看媒体信息"), this);
    //    QAction* m_ActionModel = new QAction(tr("Model selecte"), this);
    QAction *m_ActionSetting = new QAction(tr("设置"), this);
    QAction *m_ActionQuit = new QAction(tr("Exit"), this);

    addAction(m_ActionSelect);
    addAction(m_ActionNet);
    addAction(m_ActionVideoMsg);
    //    addAction(m_ActionModel);
    addAction(m_ActionSetting);

    QAction *separator = new QAction();
    separator->setSeparator(true);
    addAction(separator);

    addAction(m_ActionQuit);

    setContextMenuPolicy(Qt::ActionsContextMenu);

    connect(m_ActionSelect, &QAction::triggered, this, &paly_window::selectFile);
    connect(m_ActionNet, &QAction::triggered, this, &paly_window::inputNetUrl);
    connect(m_ActionQuit, &QAction::triggered, this, &paly_window::quitPlay);
    connect(m_ActionVideoMsg, &QAction::triggered, this, &paly_window::videoMsgWin);
}

void paly_window::updateTimeLabel(long TotalTime, long currentTime)
{
    QString hStr = QString("0%1").arg(TotalTime / 3600);
    QString mStr = QString("0%1").arg(TotalTime / 60 % 60);
    QString sStr = QString("0%1").arg(TotalTime % 60);
    totalTime = QString("%1:%2:%3").arg(hStr).arg(mStr.right(2)).arg(sStr.right(2));
    total_time->setText(totalTime);
    QString c_hStr = QString("0%1").arg((currentTime) / 3600);
    QString c_mStr = QString("0%1").arg((currentTime) / 60 % 60);
    QString c_sStr = QString("0%1").arg((currentTime) % 60);
    QString c_timeStr = QString("%1:%2:%3").arg(c_hStr).arg(c_mStr.right(2)).arg(c_sStr.right(2));
    curr_time->setText(c_timeStr);
}

void paly_window::getMousePos()
{
}

void paly_window::doSeek()
{
    slider_pos = video_slider->value();
    //    double rate = (currentTime + 1) * 10000 / TotalTime;
    qDebug() << "Total time" << do_decode->v_total_time;
    qDebug() << "slider moved" << slider_pos;
    long seek_dts = do_decode->v_total_time * slider_pos;
    qDebug() << "seek_dts " << seek_dts;
    do_decode->seek_pos = seek_dts;
    do_decode->v_isSeek = true;
}

void paly_window::videoMsgWin()
{
    QMessageBox *msgBox = new QMessageBox;
    video_rate = QString::number(do_decode->v_frameRate);
    video_decoder = do_decode->v_decoder;
    qDebug() << "视频帧率 " << do_decode->v_frameRate;
    QString message = QString("\n🎞媒体源: %5\n\n🎞视频编解码器: %1\n"
                              "\n🎞视频分辨率: %2x%3\n\n🎞视频帧率: %4\n"
                              "\n🎞视频时长: %6\n"
                              "\n🎞视频码率: %7 kbps\n"
                              "\n")
            .arg(video_decoder,
                 video_width,
                 video_height,
                 video_rate,
                 source_file,
                 totalTime,
                 QString::number(do_decode->v_bitRate));
    msgBox->setAttribute(Qt::WA_DeleteOnClose);
    msgBox->setWindowTitle(tr("媒体信息"));
    //    msgBox->resize(300,400);
    msgBox->setText(message);
    msgBox->setDetailedText(tr("Get more messages📺📺📺📺📺📺..."));
    msgBox->setStandardButtons(QMessageBox::Close);
    msgBox->open();
}

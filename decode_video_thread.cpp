#include "decode_video_thread.h"
#include <QDebug>
#include <QTime>

DecodeThread::DecodeThread(QObject *parent):QObject (parent)
{

}

DecodeThread::~DecodeThread()
{

    qDebug() << "thread quit";
}

void mSleep(int msec)
{
    QTime n=QTime::currentTime();
    QTime now;
    do
    {
        now=QTime::currentTime();
    }while (n.msecsTo(now)<=msec);
}

bool DecodeThread::startPlay(QString filePath)
{
    source_file = filePath;
    std::thread([&](DecodeThread *pointer)
    {
        pointer->readVideoFile();
    },this).detach();
}

void DecodeThread::readVideoFile()
{
    pFormatCtx = nullptr;
    pCodecCtx = nullptr;
    pCodec = nullptr;

    aCodecCtx = nullptr;
    aCodec = nullptr;
    aFrame = nullptr;

    swrCtx = nullptr;

    mAudioStream = nullptr;
    mVideoStream = nullptr;

    audio_clock = 0;
    video_clock = 0;
    const char* file_path = source_file.toUtf8().data();

    pFormatCtx = avformat_alloc_context();

    int videoStream ,audioStream;

    AVDictionary* opts = NULL;
    av_dict_set(&opts, "rtsp_transport", "tcp", 0);
    av_dict_set(&opts, "stimeout", "60000000", 0);

    if (avformat_open_input(&pFormatCtx, file_path, nullptr, &opts) != 0)
    {
        fprintf(stderr, "can't open the file. \n");
        goto end;
    }
    if (avformat_find_stream_info(pFormatCtx, nullptr) < 0)
    {
        fprintf(stderr, "Could't find stream infomation.\n");
        goto end;
    }

    videoStream = -1;
    audioStream = -1;
    for (int i = 0; i < pFormatCtx->nb_streams; i++)
    {
        if (pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            videoStream = i;
        }
        if (pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO  && audioStream < 0)
        {
            audioStream = i;
        }
    }
    total_time = pFormatCtx->duration/1000000;
    if (videoStream >= 0)
    {
        ///查找视频解码器
        pCodecCtx = pFormatCtx->streams[videoStream]->codec;
        pCodec = avcodec_find_decoder(pCodecCtx->codec_id);

        if (pCodec == nullptr)
        {
            fprintf(stderr, "PCodec not found.\n");
            goto end;
        }

        ///打开视频解码器
        if (avcodec_open2(pCodecCtx, pCodec, NULL) < 0)
        {
            fprintf(stderr, "Could not open video codec.\n");
            goto end;
        }

        mVideoStream = pFormatCtx->streams[videoStream];

        ///创建一个线程专门用来解码视频
        std::thread([&](DecodeThread *pointer)
        {
            pointer->decodeVideoThread();

        }, this).detach();

    }

end:
    while((mVideoStream != nullptr && !mIsVideoThreadFinished) || (mAudioStream != nullptr && !mIsAudioThreadFinished))
    {
        mSleep(10);
    } //确保视频线程结束后 再销毁队列

    if (swrCtx != nullptr)
    {
        swr_free(&swrCtx);
        swrCtx = nullptr;
    }

    if (aFrame != nullptr)
    {
        av_frame_free(&aFrame);
        aFrame = nullptr;
    }

    if (aFrame_ReSample != nullptr)
    {
        av_frame_free(&aFrame_ReSample);
        aFrame_ReSample = nullptr;
    }

    if (aCodecCtx != nullptr)
    {
        avcodec_close(aCodecCtx);
        aCodecCtx = nullptr;
    }

    if (pCodecCtx != nullptr)
    {
        avcodec_close(pCodecCtx);
        pCodecCtx = nullptr;
    }

    avformat_close_input(&pFormatCtx);
    avformat_free_context(pFormatCtx);
    qDebug() << "end of play";
}

void DecodeThread::decodeVideoThread()
{
    int videoWidth  = 0;
    int videoHeight =  0;

    double video_pts = 0; //当前视频的pts
    double audio_pts = 0; //音频pts
    int err = -1;

    ///解码视频相关
    AVFrame *pFrame = nullptr;
    pFrame = av_frame_alloc();
    AVCodecContext *pCodecCtx = mVideoStream->codec; //视频解码器
    AVPacket *packet = (AVPacket *)malloc(sizeof(AVPacket));
    while(1)
    {
        if(isPause)
        {
            mSleep(1000);
        }
        else
        {
            err = avcodec_send_packet(pCodecCtx, packet);
            if(err != 0){
                if(AVERROR(EAGAIN) == err)
                    continue;
                qDebug() << "发送视频帧失败!"<<  err;
            }
            //解码
            while(avcodec_receive_frame(pCodecCtx, pFrame) == 0){
                double video_clock = av_q2d(mVideoStream->time_base) * packet ->dts;
                emit sigGetFrame(pFrame);
                emit sigGetCurrentPts(total_time,
                                      video_clock);
                mSleep(50);
            }
        }
    }
}

void DecodeThread::slotDoWork(QString palyFile)
{
    AVFormatContext *avFormatContext = nullptr;
    AVPacket *packet = (AVPacket *)malloc(sizeof(AVPacket));
    AVFrame	*pFrame = nullptr;
    int audioStream= -1;
    int videoStream = -1;
    int err = -1;
    const char* filePath = palyFile.toUtf8().data();
    isPlay = true;
    isPause = false;
    avFormatContext = avformat_alloc_context();
    if (!avFormatContext) {
        av_log(nullptr, AV_LOG_FATAL, "Could not allocate context.\n");
    }

    if(avformat_open_input(&avFormatContext, filePath, nullptr, nullptr) != 0){
        qDebug() << "Couldn't open file";
    }
    // Retrieve stream information
    if(avformat_find_stream_info(avFormatContext, nullptr)<0){
        qDebug() << "Couldn't find stream information";
        return;
    }

    for(unsigned int i=0; i < avFormatContext->nb_streams; i++){
        if(avFormatContext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO){
            audioStream = static_cast<int>(i);
        }
        else if(avFormatContext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO){
            videoStream = static_cast<int>(i);
        }
    }
    total_time = avFormatContext->duration/1000000;
    if(audioStream == -1 && videoStream == -1){
        qDebug() << "Didn't find a audio stream";
        return;
    }

    pFrame = av_frame_alloc();

    //视频
    AVCodecContext *pVideoCodecContext = nullptr;
    AVCodec			*pCodecVideo;
    AVCodecParameters *pVideoChannelCodecPara = nullptr;

    if(videoStream != -1)
    {
        //视频
        pVideoChannelCodecPara = avFormatContext->streams[videoStream]->codecpar;
        pVideoCodecContext =  avcodec_alloc_context3(nullptr);
        if (!pVideoCodecContext){
            qDebug() <<  "avcodec_alloc_context3";
            return;
        }

        err = avcodec_parameters_to_context(pVideoCodecContext, pVideoChannelCodecPara);
        if (err < 0){
            qDebug() << "avcodec_parameters_to_context";
            return;
        }

        pCodecVideo = avcodec_find_decoder(pVideoChannelCodecPara->codec_id);
        if(pCodecVideo == nullptr){
            qDebug() <<  "avcodec_find_decoder";
            return;
        }

        qDebug() << "编解码器名:" << pCodecVideo->long_name;

        err = avcodec_open2(pVideoCodecContext, pCodecVideo, nullptr);
        if(err){
            qDebug() << "avcodec_open2";
            return;
        }

        emit sigGetVideoInfo(pVideoCodecContext->width, pVideoCodecContext->height);
        qDebug() << "视频宽度:" << pVideoCodecContext->width << "高度:" << pVideoCodecContext->height;
    }
    for (;;) {
        if (isPause)
        {
            mSleep(1000);
        }
        else
        {
            if(av_read_frame(avFormatContext, packet) < 0){
                goto end;
            }
            if(packet->stream_index == videoStream)
            {
                err = avcodec_send_packet(pVideoCodecContext, packet);
                if(err != 0){
                    if(AVERROR(EAGAIN) == err)
                        continue;
                    qDebug() << "发送视频帧失败!"<<  err;
                }
                //解码
                while(avcodec_receive_frame(pVideoCodecContext, pFrame) == 0){
                    double video_clock = av_q2d(avFormatContext->streams[videoStream]->time_base) * packet ->dts;
                    emit sigGetFrame(pFrame);
                    emit sigGetCurrentPts(total_time,
                                          video_clock);
                    mSleep(50);
                }
            }
            else
            {
                av_packet_unref(packet);
                continue;
            }
            if (!isPlay)
            {
                goto end;
            }
        }
    }

end:
    avcodec_close(pVideoCodecContext);
    avformat_close_input(&avFormatContext);
    qDebug() << "end of play";
}

#include "demuxworker.h"
#include <QDebug>
#include <iostream>
#include <QLabel>
#include <QTime>
#include <QCoreApplication>
#include <sys/stat.h>
#include <QString>


void mSleep(int msec)
{
    QTime n=QTime::currentTime();
    QTime now;
    do
    {
        now=QTime::currentTime();
    }while (n.msecsTo(now)<=msec);
}

DemuxWorker::DemuxWorker(QObject *parent):QObject (parent)
{
    qDebug()<<"Thread构造函数ID:"<<QThread::currentThreadId();
}

DemuxWorker::~DemuxWorker()
{

    qDebug() << "thread quit";
}


void DemuxWorker::slotDoWork(QString palyFile)
{
    qDebug() << palyFile;
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
        else if(doSeek)
        {
//            double video_clock = av_q2d(mAudioStream->time_base) * pkt->pts;
            int64_t stamp = 0;
            stamp = seekPos * avFormatContext->streams[videoStream]->duration;//当前它实际的位置
            int ret = av_seek_frame(avFormatContext, videoStream, stamp,
                                    AVSEEK_FLAG_BACKWARD | AVSEEK_FLAG_FRAME);//将视频移至到当前点击滑动条位置

            emit sigGetCurrentPts(avFormatContext->duration/1000000 * av_q2d(pVideoCodecContext->time_base),
                                  stamp * av_q2d(pVideoCodecContext->time_base));
            qDebug() << seekPos;
            doSeek = false;
            mSleep(50);
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
                    qDebug() << "druation " << avFormatContext->duration/1000000;
//                    qDebug() << "frema pts" << pFrame->pts;
                    double video_clock = av_q2d(avFormatContext->streams[videoStream]->time_base) * packet ->dts;
                    qDebug() << "video clock" << video_clock;
                    emit sigGetFrame(pFrame);
                    emit sigGetCurrentPts(avFormatContext->duration/1000000,
                                          video_clock);
                    mSleep(50);
                }
            }
            else
            {
                av_packet_unref(packet); // 注意清理，容易造成内存泄漏
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

#include "video_player.h"
#include <QTime>
#include <Windows.h>
#include <QDebug>
#include <stdio.h>

VideoPlayer::VideoPlayer(QObject *parent) : QObject(parent)
{
    mConditon_Video = new Cond;
    mConditon_Audio = new Cond;
}
VideoPlayer::~VideoPlayer()
{
    qDebug() << "thread quit";
}
void v_mSleep(int msec)
{
    QTime n = QTime::currentTime();
    QTime now;
    do
    {
        now = QTime::currentTime();
    } while (n.msecsTo(now) <= msec);
}

bool VideoPlayer::init_player()
{
    return true;
}

//bool VideoPlayer::start_play(QString file_path)
//{
//    qDebug() << "start_play";
//    //    printf("%s\n",__func__);
//    v_quit = false;
//    v_pause = false;
//    v_file_path = file_path;
//    std::thread([&](VideoPlayer *pointer)
//    {
//        pointer->read_video_file();

//    }, this).detach();

//    return true;

//}

void VideoPlayer::read_video_file(QString file_path)
{
    qDebug() << "read video file";
    v_quit = false;
    v_pause = false;
    QByteArray byteArray = file_path.toUtf8();
    char *video_file = byteArray.data();

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

    int video_stream,audio_stream;

    pFormatCtx = avformat_alloc_context();

    AVDictionary* opts = NULL;
    av_dict_set(&opts, "rtsp_transport", "tcp", 0); //设置tcp or udp，默认一般优先tcp再尝试udp
    av_dict_set(&opts, "stimeout", "60000000", 0);//设置超时3秒

    if(avformat_open_input(&pFormatCtx,video_file,nullptr,&opts) !=0)
    {
        qCritical() << "Could not open video source!";
        goto end;
    }

    if(avformat_find_stream_info(pFormatCtx,nullptr)<0)
    {
        qCritical() << "Could not find stream information";
        goto end;
    }
    video_stream = -1;
    audio_stream = -1;
    for(int i = 0; i<pFormatCtx->nb_streams;i++)
    {
        if(pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            video_stream = i;
        }
        if(pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO)
        {
            audio_stream = i;
        }
    }

    if(video_stream >=0)
    {
        pCodecCtx = pFormatCtx->streams[video_stream]->codec;
        pCodec = avcodec_find_decoder(pCodecCtx->codec_id);
        if(pCodec == nullptr)
        {
            qCritical() << "Could not find PCodec";
            goto end;
        }
        if(avcodec_open2(pCodecCtx,pCodec,NULL) < 0)
        {
            qCritical() << "Could not open video codec";
            goto end;
        }
        mVideoStream = pFormatCtx->streams[video_stream];

        std::thread([&](VideoPlayer *pointer)
        {
            pointer->decode_video_thread();
        },this).detach();
    }
    if(audio_stream>=0)
    {
        aCodecCtx = pFormatCtx->streams[audio_stream]->codec;
        aCodec = avcodec_find_decoder(aCodecCtx->codec_id);

        if (aCodec == NULL)
        {
            qCritical() << "Could not find Acodec";
            audio_stream = -1;
        }
        else
        {
            if (avcodec_open2(aCodecCtx, aCodec, nullptr) < 0)
            {
                fprintf(stderr, "Could not open audio codec.\n");
                //                doOpenVideoFileFailed();
                goto end;
            }
            aFrame = av_frame_alloc();
        }
    }
    while(1)
    {
        if (mAudioPacktList.size() > MAX_AUDIO_SIZE || mVideoPacktList.size() > MAX_VIDEO_SIZE)
        {
            v_mSleep(10);
            continue;
        }
        AVPacket packet;
        if(av_read_frame(pFormatCtx,&packet) <0)
        {
            v_read_finished = true;
            if(v_quit)
            {
                break;
            }
            v_mSleep(10);
            continue;
        }
        if(packet.stream_index == video_stream)
        {
            inputVideoQuene(packet);
        }
        else if(packet.stream_index == audio_stream)
        {
            inputAudioQuene(packet);
        }
        else
        {
            av_packet_unref(&packet);
        }
    }
    while(!v_quit)
    {
        v_mSleep(100);
    }

end:
    clearAudioQuene();
    clearVideoQuene();

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

}

void VideoPlayer::decode_video_thread()
{
    qDebug() << "decode video file";
    int video_width =0 ;
    int video_height = 0;

    double video_pts = 0;
    double audio_pts = 0;

//    const char *outputFileName = "output_frame.png";

    AVFrame *pFrame = nullptr;
    AVFrame *pFrameYUV = nullptr;
    uint8_t *yuv420pBuffer = nullptr;
    struct SwsContext *imgConvertCtx = nullptr;

    AVCodecContext *pCodecCtx = mVideoStream->codec;

    pFrame = av_frame_alloc();
    mConditon_Video->Lock();
    while(1)
    {
        if(mVideoPacktList.size()<0)
        {
            v_mSleep(1);
            continue;
        }
        AVPacket pkt1 = mVideoPacktList.front();
        mVideoPacktList.pop_front();
        mConditon_Video->Unlock();

        AVPacket *packet = &pkt1;

        if(strcmp((char*)packet->data, FLUSH_DATA) == 0)
        {
            avcodec_flush_buffers(mVideoStream->codec);
            av_packet_unref(packet);
            continue;
        }
        if (avcodec_send_packet(pCodecCtx, packet) != 0)
        {
            qDebug("input AVPacket to decoder failed!\n");
            av_packet_unref(packet);
            continue;
        }
        while(avcodec_receive_frame(pCodecCtx,pFrame) == 0)
        {
            if(packet->dts == AV_NOPTS_VALUE && pFrame->opaque&& *(uint64_t*) pFrame->opaque != AV_NOPTS_VALUE)
            {
                video_pts = *(uint64_t *) pFrame->opaque;
            }
            else if (packet->dts != AV_NOPTS_VALUE)
            {
                video_pts = packet->dts;
            }
            else
            {
                video_pts = 0;
            }
            video_pts *= av_q2d(mVideoStream->time_base);
            video_clock = video_pts;
            // TODO:发生跳转
            // TODO:音视频同步
            qDebug() << "-----send one frame-------" << video_pts;
            emit sigGetFrame(pFrame);
            // TODO:发送时间戳
            v_mSleep(50);
        }
        av_packet_unref(packet);
    }
    av_free(pFrame);
    if (pFrameYUV != nullptr)
    {
        av_free(pFrameYUV);
    }

    if (yuv420pBuffer != nullptr)
    {
        av_free(yuv420pBuffer);
    }

    if (imgConvertCtx != nullptr)
    {
        sws_freeContext(imgConvertCtx);
    }

    if (!v_quit)
    {
        v_quit = true;
    }

    v_video_thread_finished = true;
    return;
}

void VideoPlayer::decode_audio_thread()
{

}


bool VideoPlayer::inputVideoQuene(const AVPacket &pkt)
{
    if (av_dup_packet((AVPacket*)&pkt) < 0)
    {
        return false;
    }
    //    qDebug() << "+++++Input 1 video packet++++++" ;
    mConditon_Video->Lock();
    mVideoPacktList.push_back(pkt);
    mConditon_Video->Signal();
    mConditon_Video->Unlock();

    return true;
}

void VideoPlayer::clearVideoQuene()
{
    mConditon_Video->Lock();
    for (AVPacket pkt : mVideoPacktList)
    {
        //        av_free_packet(&pkt);
        av_packet_unref(&pkt);
    }
    mVideoPacktList.clear();
    mConditon_Video->Unlock();
}

bool VideoPlayer::inputAudioQuene(const AVPacket &pkt)
{
    if (av_dup_packet((AVPacket*)&pkt) < 0)
    {
        return false;
    }

    mConditon_Audio->Lock();
    mAudioPacktList.push_back(pkt);
    mConditon_Audio->Signal();
    mConditon_Audio->Unlock();

    return true;
}

void VideoPlayer::clearAudioQuene()
{
    mConditon_Audio->Lock();
    for (AVPacket pkt : mAudioPacktList)
    {
        //        av_free_packet(&pkt);
        av_packet_unref(&pkt);
    }
    mAudioPacktList.clear();
    mConditon_Audio->Unlock();
}


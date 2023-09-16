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
        //        qDebug() << "Sleeping";
    } while (n.msecsTo(now) <= msec);
}

bool VideoPlayer::init_player()
{
    return true;
}

bool VideoPlayer::start_play(QString file_path)
{
    qDebug() << "start_play";
    //    printf("%s\n",__func__);
    v_quit = false;
    v_pause = false;
    v_read_finished = false;
    v_file_path = file_path;
    std::thread([&](VideoPlayer *pointer)
    {
        pointer->read_video_file();

    }, this).detach();

    return true;

}

void VideoPlayer::read_video_file()
{
    qDebug() << "read video file";
    QByteArray byteArray = v_file_path.toUtf8();
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
        // get video msg
        v_bitRate = pFormatCtx->bit_rate/1000;
        v_total_time = pFormatCtx->duration/1000000;
        v_frameRate = av_q2d(pFormatCtx->streams[video_stream]->r_frame_rate);

        pCodecCtx = pFormatCtx->streams[video_stream]->codec;
        pCodec = avcodec_find_decoder(pCodecCtx->codec_id);
        if(pCodec == nullptr)
        {
            qCritical() << "Could not find PCodec";
            goto end;
        }
        v_decoder = QString(pCodec->long_name);
        if(avcodec_open2(pCodecCtx,pCodec,NULL) < 0)
        {
            qCritical() << "Could not open video codec";
            goto end;
        }
        mVideoStream = pFormatCtx->streams[video_stream];

        // send video size
        emit sigGetVideoInfo(pCodecCtx->width, pCodecCtx->height);

        // decode video
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
            qCritical() <<  "Could not find audio codec.";
            audio_stream = -1;
        }
        else
        {
            if (avcodec_open2(aCodecCtx, aCodec, nullptr) < 0)
            {
                qCritical() << "Could not open audio codec.";
                goto end;
            }

            aFrame = av_frame_alloc();
            aFrame_ReSample = nullptr;
            swrCtx = nullptr;
            int in_ch_layout;
            out_ch_layout = av_get_default_channel_layout(audio_tgt_channels); ///AV_CH_LAYOUT_STEREO

            out_ch_layout &= ~AV_CH_LAYOUT_STEREO_DOWNMIX;

            in_sample_fmt = aCodecCtx->sample_fmt;
            out_sample_fmt = AV_SAMPLE_FMT_S16;
            in_sample_rate = aCodecCtx->sample_rate;
            in_ch_layout = aCodecCtx->channel_layout;
            out_sample_rate = aCodecCtx->sample_rate;

            //输出的声道布局

            audio_tgt_channels = 2; ///av_get_channel_layout_nb_channels(out_ch_layout);
            out_ch_layout = av_get_default_channel_layout(audio_tgt_channels); ///AV_CH_LAYOUT_STEREO

            out_ch_layout &= ~AV_CH_LAYOUT_STEREO_DOWNMIX;

            /// 2019-5-13添加
            /// wav/wmv 文件获取到的aCodecCtx->channel_layout为0会导致后面的初始化失败，因此这里需要加个判断。
            if (in_ch_layout <= 0)
            {
                in_ch_layout = av_get_default_channel_layout(aCodecCtx->channels);
            }

            swrCtx = swr_alloc_set_opts(nullptr, out_ch_layout, out_sample_fmt, out_sample_rate,
                                        in_ch_layout, in_sample_fmt, in_sample_rate, 0, nullptr);

            /** Open the resampler with the specified parameters. */
            int ret = swr_init(swrCtx);
            if (ret < 0)
            {
                char buff[128]={0};
                av_strerror(ret, buff, 128);

                qCritical() << "Could not open resample context "<< buff;
                swr_free(&swrCtx);
                swrCtx = nullptr;
                goto end;
            }

            //存储pcm数据
            int out_linesize = out_sample_rate * audio_tgt_channels;

            //        out_linesize = av_samples_get_buffer_size(NULL, audio_tgt_channels, av_get_bytes_per_sample(out_sample_fmt), out_sample_fmt, 1);
            out_linesize = AVCODEC_MAX_AUDIO_FRAME_SIZE;


            mAudioStream = pFormatCtx->streams[audio_stream];

            ///打开SDL播放声音
            int code = set_audio_SDL();

            if (code == 0)
            {
                SDL_LockAudioDevice(mAudioID);
                SDL_PauseAudioDevice(mAudioID,0);
                SDL_UnlockAudioDevice(mAudioID);

                v_audio_thread_finished = false;
            }
            else
            {
                qCritical() << "Open audio sdl failed!";
            }
        }
    }
    v_start_play_time = av_gettime();
    while(1)
    {
        //        if(v_pause == false)
        //        {
        // if (mVideoPacktList.size() > MAX_VIDEO_SIZE)
        if(v_quit == true)
        {
            break;
        }
        if (mAudioPacktList.size() > MAX_AUDIO_SIZE || mVideoPacktList.size() > MAX_VIDEO_SIZE)
        {
            // qDebug() << mVideoPacktList.size();
            v_mSleep(10);
            continue;
        }
        if(v_pause == true)
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
            if(v_audio_thread_finished == true)
            {
                av_packet_unref(&packet);
            }
            else
            {
                inputAudioQuene(packet);
            }
        }
        else
        {
            av_packet_unref(&packet);
        }
        //        }
        //        else
        //        {
        //            v_mSleep(5);
        //        }

    }
    while(!v_quit)
    {
        v_mSleep(100);
    }

end:
    qDebug() << "play video finished!";
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

    close_audio_SDL();
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
        if(v_quit == true)
        {
            qDebug() << "break decode";
            break;
        }
        if(mVideoPacktList.size()<=0)
        {
            mConditon_Video->Unlock();
            if(v_read_finished == true)
            {
                //                v_video_thread_finished=true;
                qDebug() << "video decode finished.";
                break;
            }
            else
            {
                qDebug() << "empty list";
                v_mSleep(1);
                continue;
            }
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
            qDebug() << "-----send one frame-------" << video_pts;
            while(1)
            {
                if (v_quit)
                {
                    break;
                }

                if (mAudioStream != NULL && !v_audio_thread_finished)
                {
                    if (v_read_finished && mAudioPacktList.size() <= 0)
                    {
                        break;
                    }
                    audio_pts = audio_clock;
                }
                else
                {
                    audio_pts = (av_gettime() - v_start_play_time) / 1000000.0;
                    audio_clock = audio_pts;
                }
                video_pts = video_clock;

                if (video_pts <= audio_pts) break;
                if (v_pause) break;

                int delayTime = (video_pts - audio_pts) * 1000;
                qDebug() << "delayTime ------" << delayTime;

                delayTime = delayTime > 5 ? 5:delayTime;
                v_mSleep(delayTime);

            }
            emit sigGetFrame(pFrame);
            emit sigGetCurrentPts(v_total_time,video_clock);

            //            qDebug() << "---sleep---" << 1000/v_frameRate;
            //                v_mSleep(1000/v_frameRate);
            v_mSleep(1);
            while(v_pause == true)
            {
                emit sigGetFrame(pFrame);
                if(v_quit)
                {
                    break;
                }
                v_mSleep(40);
            }
        }
        av_packet_unref(packet);
    }
    qDebug() << "clear video";
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


int VideoPlayer::set_audio_SDL()
{
    SDL_AudioSpec wanted_spec, spec;
    int wanted_nb_channels = 2;
    int samplerate = out_sample_rate;

    wanted_spec.channels = wanted_nb_channels;
    wanted_spec.samples = FFMAX(512, 2 << av_log2(wanted_spec.freq / 30));
    wanted_spec.freq = samplerate;
    wanted_spec.format = AUDIO_S16SYS;
    wanted_spec.silence = 0;

    wanted_spec.callback = sdlAudioCallBackFunc;
    wanted_spec.userdata = this;

    int num = SDL_GetNumAudioDevices(0);
    for (int i=0;i<num;i++)
    {
        mAudioID = SDL_OpenAudioDevice(SDL_GetAudioDeviceName(i,0), false, &wanted_spec, &spec,0);
        if (mAudioID > 0)
        {
            break;
        }
    }

    if (mAudioID <= 0)
    {
        v_audio_thread_finished = true;
        return -1;
    }
    qCritical() << "mAudioID = " << mAudioID;
    return 0;
}

void VideoPlayer::close_audio_SDL()
{
    if (mAudioID > 0)
    {
        SDL_LockAudioDevice(mAudioID);
        SDL_PauseAudioDevice(mAudioID, 1);
        SDL_UnlockAudioDevice(mAudioID);
        SDL_CloseAudioDevice(mAudioID);
    }

    mAudioID = 0;
}


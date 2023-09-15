#include "video_player.h"


void VideoPlayer::sdlAudioCallBackFunc(void *userdata, Uint8 *stream, int len)
{
    VideoPlayer *player = (VideoPlayer*)userdata;
    player->sdlAudioCallBack(stream, len);
}

void VideoPlayer::sdlAudioCallBack(Uint8 *stream, int len)
{
    int len1, audio_data_size;

    while (len > 0)
    {
        if (audio_buf_index >= audio_buf_size)
        {
            audio_data_size = decode_audio_thread();

            if (audio_data_size <= 0)
            {
                audio_buf_size = 1024;
                memset(audio_buf, 0, audio_buf_size);
            }
            else
            {
                audio_buf_size = audio_data_size;
            }
            audio_buf_index = 0;
        }
        len1 = audio_buf_size - audio_buf_index;

        if (len1 > len)
        {
            len1 = len;
        }

        if (audio_buf == NULL) return;

//        if (mIsMute || mIsNeedPause) //静音 或者 是在暂停的时候跳转了
//        {
//            memset(audio_buf + audio_buf_index, 0, len1);
//        }
//        else
//        {
////            PcmVolumeControl::RaiseVolume((char*)audio_buf + audio_buf_index, len1, 1, mVolume);
//        }

        memcpy(stream, (uint8_t *)audio_buf + audio_buf_index, len1);

        //        SDL_memset(stream, 0x0, len);// make sure this is silence.
        //        SDL_MixAudio(stream, (uint8_t *) audio_buf + audio_buf_index, len1, SDL_MIX_MAXVOLUME);

        //        SDL_MixAudio(stream, (uint8_t * )is->audio_buf + is->audio_buf_index, len1, 50);
        //        SDL_MixAudioFormat(stream, (uint8_t * )is->audio_buf + is->audio_buf_index, AUDIO_S16SYS, len1, 50);

        len -= len1;
        stream += len1;
        audio_buf_index += len1;

    }

}

int VideoPlayer::decode_audio_thread(bool isBlock)
{
    int audioBufferSize = 0;

    while(1)
    {
        if (v_quit)
        {
            v_audio_thread_finished= true;
            clearAudioQuene(); //清空队列
            break;
        }

        if (v_pause == true) //判断暂停
        {
            break;
        }

        mConditon_Audio->Lock();

        if (mAudioPacktList.size() <= 0)
        {
            if (isBlock)
            {
                mConditon_Audio->Wait();
            }
            else
            {
                mConditon_Audio->Unlock();
                break;
            }
        }

        AVPacket packet = mAudioPacktList.front();
        mAudioPacktList.pop_front();
        mConditon_Audio->Unlock();

        AVPacket *pkt = &packet;

        if (pkt->pts != AV_NOPTS_VALUE)
        {
            audio_clock = av_q2d(mAudioStream->time_base) * pkt->pts;
        }

        if(strcmp((char*)pkt->data,FLUSH_DATA) == 0)
        {
            avcodec_flush_buffers(mAudioStream->codec);
            av_packet_unref(pkt);
            continue;
        }

        //        if (seek_flag_audio)
        //        {
        //            //发生了跳转 则跳过关键帧到目的时间的这几帧
        //           if (audio_clock < seek_time)
        //           {
        //               continue;
        //           }
        //           else
        //           {
        //               seek_flag_audio = 0;
        //           }
        //        }

        //解码AVPacket->AVFrame
        int got_frame = 0;
        int size = avcodec_decode_audio4(aCodecCtx, aFrame, &got_frame, &packet);

        av_packet_unref(&packet);

        if (got_frame)
        {
            int nb_samples = av_rescale_rnd(swr_get_delay(swrCtx, out_sample_rate) + aFrame->nb_samples, out_sample_rate, in_sample_rate, AV_ROUND_UP);

            if (aFrame_ReSample != nullptr)
            {
                if (aFrame_ReSample->nb_samples != nb_samples)
                {
                    av_frame_free(&aFrame_ReSample);
                    aFrame_ReSample = nullptr;
                }
            }

            ///解码一帧后才能获取到采样率等信息，因此将初始化放到这里
            if (aFrame_ReSample == nullptr)
            {
                aFrame_ReSample = av_frame_alloc();

                aFrame_ReSample->format = out_sample_fmt;
                aFrame_ReSample->channel_layout = out_ch_layout;
                aFrame_ReSample->sample_rate = out_sample_rate;
                aFrame_ReSample->nb_samples = nb_samples;

                int ret = av_samples_fill_arrays(aFrame_ReSample->data, aFrame_ReSample->linesize, audio_buf, audio_tgt_channels, aFrame_ReSample->nb_samples, out_sample_fmt, 0);
                if (ret < 0)
                {
                    fprintf(stderr, "Error allocating an audio buffer\n");
                }
            }

            int len2 = swr_convert(swrCtx, aFrame_ReSample->data, aFrame_ReSample->nb_samples, (const uint8_t**)aFrame->data, aFrame->nb_samples);
            int resampled_data_size = av_samples_get_buffer_size(NULL, audio_tgt_channels, aFrame_ReSample->nb_samples, out_sample_fmt, 1);
            audioBufferSize = resampled_data_size;
            break;
        }
    }

    return audioBufferSize;
}

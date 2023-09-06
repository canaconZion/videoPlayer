#include "decode_media.h"
#include <stdio.h>

static Uint8 *audio_chunk;
static Uint32 audio_len;
static Uint8 *audio_pos;

void fill_audio(void *udata, Uint8 *stream, int len)
{
    // SDL 2.0
    SDL_memset(stream, 0, len);
    if (audio_len == 0)
        return;

    len = (len > audio_len ? audio_len : len); /*  Mix  as  much  data  as  possible  */

    SDL_MixAudio(stream, audio_pos, len, SDL_MIX_MAXVOLUME);
    audio_pos += len;
    audio_len -= len;
}

MediaProcess::MediaProcess()
{
    int index = 0;
    std::cout << "----------Init decode process----------------\n"
              << std::endl;
    sdl_init();
    mConditon_Video = new Cond;
    mConditon_Audio = new Cond;
}

MediaProcess::~MediaProcess()
{
}

int MediaProcess::srp_refresh_thread(void *opaque)
{
    thread_exit = 0;
    thread_pause = 0;

    while (!thread_exit)
    {
        if (!thread_pause)
        {
            SDL_Event event;
            event.type = SFM_REFRESH_EVENT;
            SDL_PushEvent(&event);
        }
        SDL_Delay(40);
    }
    thread_exit = 0;
    thread_pause = 0;

    SDL_Event event;
    event.type = SFM_BREAK_EVENT;
    SDL_PushEvent(&event);

    return 0;
}

int MediaProcess::media_decode()
{
    pFormatCtx = avformat_alloc_context();

    if (avformat_open_input(&pFormatCtx, filepath, NULL, NULL) != 0)
    {
        std::cout << "Could not open input stream: " << filepath << "\n"
                  << std::endl;
        goto end;
    }
    if (avformat_find_stream_info(pFormatCtx, NULL) < 0)
    {
        std::cout << "Could not find stream information.\n"
                  << std::endl;
        goto end;
    }
    videoindex = -1;
    audioindex = -1;
    for (i = 0; i < pFormatCtx->nb_streams; i++)
    {
        if (pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            videoindex = i;
        }
        else if (pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO)
        {
            audioindex = i;
        }
    }
    std::cout << "------------------File Information-----------------\n"
              << std::endl;
    av_dump_format(pFormatCtx, 0, filepath, 0);
    std::cout << "--------------------------------------------------\n"
              << std::endl;
    if (videoindex >= 0)
    {
        pCodecCtx = pFormatCtx->streams[videoindex]->codec;
        // pCodecParams = pFormatCtx->streams[videoindex]->codecpar;
        pCodec = avcodec_find_decoder(pCodecCtx->codec_id);
        if (pCodec == NULL)
        {
            std::cout << "Could not found video codec.\n"
                      << std::endl;
            goto end;
        }
        if (avcodec_open2(pCodecCtx, pCodec, NULL) < 0)
        {
            std::cout << "Could not open video codec.\n"
                      << std::endl;
            goto end;
        }
        pFrame = av_frame_alloc();
        pFrameYUV = av_frame_alloc();

        out_buffer = (unsigned char *)av_malloc(av_image_get_buffer_size(AV_PIX_FMT_YUV420P, pCodecCtx->width, pCodecCtx->height, 1));
        av_image_fill_arrays(pFrameYUV->data, pFrameYUV->linesize, out_buffer,
                             AV_PIX_FMT_YUV420P, pCodecCtx->width, pCodecCtx->height, 1);

        img_convert_ctx = sws_getContext(pCodecCtx->width, pCodecCtx->height, pCodecCtx->pix_fmt,
                                         pCodecCtx->width, pCodecCtx->height, AV_PIX_FMT_YUV420P, SWS_BICUBIC, NULL, NULL, NULL);
        screen_w = pCodecCtx->width + 6;
        screen_h = pCodecCtx->height + 6;
        screen = SDL_CreateWindow("video Player", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, screen_w, screen_h, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
        if (!screen)
        {
            std::cout << "SDL: could not creeate window - exiting:\n"
                      << SDL_GetError() << "\n"
                      << std::endl;
            goto end;
        }
        sdlRenderer = SDL_CreateRenderer(screen, -1, 0);
        sdlTexture = SDL_CreateTexture(sdlRenderer, SDL_PIXELFORMAT_IYUV, SDL_TEXTUREACCESS_STREAMING, pCodecCtx->width, pCodecCtx->height);
        mVideoStream = pFormatCtx->streams[videoindex];
        std::thread([&](MediaProcess *v_decoder)
                    { v_decoder->video_decode(); },
                    this)
            .detach();
    }
    if (audioindex >= 0)
    {
        aCodecCtx = pFormatCtx->streams[audioindex]->codec;
        aCodec = avcodec_find_decoder(aCodecCtx->codec_id);
        if (aCodec == NULL)
        {
            printf("Audio Codec not found.\n");
            return -1;
        }
        if (avcodec_open2(aCodecCtx, aCodec, NULL) < 0)
        {
            printf("Could not open codec.\n");
            return -1;
        }
        uint64_t out_channel_layout = AV_CH_LAYOUT_STEREO;
        // nb_samples: AAC-1024 MP3-1152
        int out_nb_samples = aCodecCtx->frame_size;
        AVSampleFormat out_sample_fmt = AV_SAMPLE_FMT_S16;
        int out_sample_rate = 44100;
        int out_channels = av_get_channel_layout_nb_channels(out_channel_layout);
        // Out Buffer Size
        out_buffer_size = av_samples_get_buffer_size(NULL, out_channels, out_nb_samples, out_sample_fmt, 1);

        a_out_buffer = (uint8_t *)av_malloc(MAX_AUDIO_FRAME_SIZE * 2);
        aFrame = av_frame_alloc();
        std::cout << "VideoIndex" << videoindex << "\nAudioINdex" << audioindex << std::endl;
        wanted_spec.freq = out_sample_rate;
        wanted_spec.format = AUDIO_S16SYS;
        wanted_spec.channels = out_channels;
        wanted_spec.silence = 0;
        wanted_spec.samples = out_nb_samples;
        wanted_spec.callback = fill_audio;
        wanted_spec.userdata = aCodecCtx;

        if (SDL_OpenAudio(&wanted_spec, NULL) < 0)
        {
            printf("can't open audio.\n");
            return -1;
        }
        // FIX:Some Codec's Context Information is missing
        in_channel_layout = av_get_default_channel_layout(aCodecCtx->channels);
        // Swr

        au_convert_ctx = swr_alloc();
        au_convert_ctx = swr_alloc_set_opts(au_convert_ctx, out_channel_layout, out_sample_fmt, out_sample_rate,
                                            in_channel_layout, pCodecCtx->sample_fmt, aCodecCtx->sample_rate, 0, NULL);
        swr_init(au_convert_ctx);

        // Play
        SDL_PauseAudio(0);
        std::thread([&](MediaProcess *v_decoder)
                    { v_decoder->audio_decode(); },
                    this)
            .detach();
    }
    // packet = (AVPacket *)av_malloc(sizeof(AVPacket));
    // av_init_packet(packet);
    while (1)
    {
        // int stream_index = -1;
        // if (videoindex >= 0)
        //     stream_index = videoindex;
        // else if (audioindex >= 0)
        //     stream_index = audioindex;

        // if (videoindex >= 0)
        // {
        //     AVPacket packet;
        //     av_new_packet(&packet, 10);
        //     strcpy((char *)packet.data, "FLUSH");
        //     clearVideoQuene();       // 清除队列
        //     inputVideoQuene(packet); // 往队列中存入用来清除的包
        // }
        // std::cout << "In to cycle\n"
        //           << std::endl;
        if (mVideoPacktList.size() > MAX_VIDEO_SIZE)
        {
            std::this_thread::sleep_for(std::chrono::seconds(10));
            continue;
        }
        AVPacket packet;
        if (av_read_frame(pFormatCtx, &packet) < 0)
        {
            mIsReadFinished = true;
            std::cout << "end of file" << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(10));
            goto end;
        }
        if (packet.stream_index == videoindex)
        {
            std::cout << "Try to input video to queue\n"
                      << std::endl;
            inputVideoQuene(packet);
        }
        else if (packet.stream_index == audioindex)
        {
            std::cout << "Try to input audio to queue\n"
                      << std::endl;
            inputAudioQuene(packet);
        }
        else
        {
            // Free the packet that was allocated by av_read_frame
            av_packet_unref(&packet);
        }
        // if (av_read_frame(pFormatCtx, packet) < 0)
        //     goto end;
        // if (packet->stream_index == videoindex)
        // {
        //     // SDL_WaitEvent(&event);
        //     // if (event.type == SFM_REFRESH_EVENT)
        //     // {
        //     sdlRect_1.y = 2;
        //     sdlRect_1.x = 2;
        //     sdlRect_1.w = screen_w;
        //     sdlRect_1.h = screen_h;
        //     if (packet->stream_index == videoindex)
        //     {
        //         ret = avcodec_decode_video2(pCodecCtx, pFrame, &got_picture, packet);
        //         if (ret < 0)
        //         {
        //             std::cout << "Decode Error.\n"
        //                     << std::endl;
        //             goto end;
        //         }
        //         if (got_picture)
        //         {
        //             sws_scale(img_convert_ctx, (const unsigned char *const *)pFrame->data, pFrame->linesize, 0, pCodecCtx->height, pFrameYUV->data, pFrameYUV->linesize);
        //             SDL_UpdateTexture(sdlTexture, NULL, pFrameYUV->data[0], pFrameYUV->linesize[0]);
        //             SDL_RenderClear(sdlRenderer);
        //             SDL_RenderCopy(sdlRenderer, sdlTexture, NULL, &sdlRect_1);
        //             SDL_RenderPresent(sdlRenderer);
        //         }
        //     }
        //     SDL_Delay(40);
        //     av_free_packet(packet);
        // }
    }
end:
    // clearAudioQuene();
    std::cout << "go to end" << std::endl;
    /** Thread decode **/
    // clearVideoQuene();
    // if (aFrame != nullptr)
    // {
    //     av_frame_free(&aFrame);
    //     aFrame = nullptr;
    // }

    // if (aCodecCtx != nullptr)
    // {
    //     avcodec_close(aCodecCtx);
    //     aCodecCtx = nullptr;
    // }

    // if (pCodecCtx != nullptr)
    // {
    //     avcodec_close(pCodecCtx);
    //     pCodecCtx = nullptr;
    // }

    // avformat_close_input(&pFormatCtx);
    // avformat_free_context(pFormatCtx);

    // SDL_Quit();
    sws_freeContext(img_convert_ctx);

    SDL_Quit();
    SDL_CloseAudio();

    swr_free(&au_convert_ctx);

    av_free(a_out_buffer);
    avcodec_close(aCodecCtx);

    av_free(out_buffer);
    av_frame_free(&pFrameYUV);
    av_frame_free(&pFrame);
    avcodec_close(pCodecCtx);
    avformat_close_input(&pFormatCtx);
    return 0;
}

int MediaProcess::video_decode()
{
    while (1)
    {
        std::cout << "Begin to decode video\n"
                  << std::endl;
        mConditon_Video->Lock();
        if (mVideoPacktList.size() <= 0)
        {
            mConditon_Video->Unlock();
            if (mIsReadFinished)
            {
                // 队列里面没有数据了且读取完毕了
                std::cout << "Read packet finished, quit process\n"
                          << std::endl;
                break;
            }
            else
            {
                std::cout << "No packet in queue\n"
                          << std::endl;
                std::this_thread::sleep_for(std::chrono::seconds(1)); // 队列只是暂时没有数据而已
                continue;
            }
        }
        AVPacket pkt1 = mVideoPacktList.front();
        mVideoPacktList.pop_front();

        mConditon_Video->Unlock();

        AVPacket *packet = &pkt1;
        sdlRect_1.y = 2;
        sdlRect_1.x = 2;
        sdlRect_1.w = screen_w;
        sdlRect_1.h = screen_h;
        ret = avcodec_decode_video2(pCodecCtx, pFrame, &got_picture, packet);
        if (ret < 0)
        {
            std::cout << "Decode Error.\n"
                      << std::endl;
            return -1;
        }
        std::cout << "decode 1 video frame.\n"
                  << std::endl;
        if (got_picture)
        {
            sws_scale(img_convert_ctx, (const unsigned char *const *)pFrame->data, pFrame->linesize, 0, pCodecCtx->height, pFrameYUV->data, pFrameYUV->linesize);
            SDL_UpdateTexture(sdlTexture, NULL, pFrameYUV->data[0], pFrameYUV->linesize[0]);
            SDL_RenderClear(sdlRenderer);
            SDL_RenderCopy(sdlRenderer, sdlTexture, NULL, &sdlRect_1);
            SDL_RenderPresent(sdlRenderer);
        }
        SDL_Delay(40);
        // av_free_packet(packet); // 已弃用
        av_packet_unref(packet);
    }

    av_frame_free(&pFrameYUV);
    av_frame_free(&pFrame);
    avcodec_close(pCodecCtx);
    return 0;
}

int MediaProcess::audio_decode()
{
    while (1)
    {
        std::cout << "Begin to decode audio packet" << std::endl;
        mConditon_Audio->Lock();
        if (mAudioPacktList.size() <= 0)
        {
            mConditon_Audio->Unlock();
            if (mIsReadFinished)
            {
                // 队列里面没有数据了且读取完毕了
                std::cout << "Read packet finished, quit process\n"
                          << std::endl;
                break;
            }
            else
            {
                std::cout << "No audio packet in queue\n"
                          << std::endl;
                std::this_thread::sleep_for(std::chrono::seconds(1)); // 队列只是暂时没有数据而已
                continue;
            }
        }
        AVPacket pkt1 = mAudioPacktList.front();
        mAudioPacktList.pop_front();

        mConditon_Audio->Unlock();

        AVPacket *packet = &pkt1;
        ;
        ret = avcodec_decode_audio4(aCodecCtx, aFrame, &a_got_picture, packet);
        if (ret < 0)
        {
            printf("Error in decoding audio frame.\n");
            return -1;
        }
        if (a_got_picture > 0)
        {
            swr_convert(au_convert_ctx, &a_out_buffer, MAX_AUDIO_FRAME_SIZE, (const uint8_t **)pFrame->data, pFrame->nb_samples);
            printf("index:%5d\t pts:%lld\t packet size:%d\n", index, packet->pts, packet->size);
            index++;
        }

        while (audio_len > 0) // Wait until finish
            SDL_Delay(1);

        // Set audio buffer (PCM data)
        audio_chunk = (Uint8 *)a_out_buffer;
        // Audio buffer length
        audio_len = out_buffer_size;
        audio_pos = audio_chunk;
        av_packet_unref(packet);
    }
    av_free(a_out_buffer);
    av_frame_free(&aFrame);
    avcodec_close(aCodecCtx);
    swr_free(&au_convert_ctx);
}

int MediaProcess::sdl_init()
{
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER))
    {
        std::cout << "Could not initialize SDL - " << SDL_GetError() << "\n"
                  << std::endl;
        return -1;
    }
    std::cout << "Init sdl successfull\n"
              << std::endl;
    // screen = SDL_CreateWindow("video Player", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, screen_w, screen_h, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    // if (!screen)
    // {
    //     std::cout << "SDL: could not creeate window - exiting:\n"
    //               << SDL_GetError() << "\n"
    //               << std::endl;
    //     return -1;
    // }
    // sdlRenderer = SDL_CreateRenderer(screen, -1, 0);
    // sdlTexture = SDL_CreateTexture(sdlRenderer, SDL_PIXELFORMAT_IYUV, SDL_TEXTUREACCESS_STREAMING, pCodecCtx->width, pCodecCtx->height);
}

bool MediaProcess::inputVideoQuene(const AVPacket &pkt)
{
    int iret = 0;
    std::cout << "In input\n"
              << std::endl;
    if (iret = av_dup_packet((AVPacket *)&pkt) < 0)
    {
        std::cout << "Input failed: " << iret << "\n"
                  << std::endl;
        return false;
    }
    try
    {
        mConditon_Video->Lock();
        mVideoPacktList.push_back(pkt);
        mConditon_Video->Signal();
        mConditon_Video->Unlock();
    }
    catch (const std::exception &ex)
    {
        std::cerr << "Caught exception: " << ex.what() << std::endl;
    }
    std::cout << "inputVideoQuene: successful" << std::endl;

    return true;
}

bool MediaProcess::inputAudioQuene(const AVPacket &pkt)
{
    if (av_dup_packet((AVPacket *)&pkt) < 0)
    {
        return false;
    }

    mConditon_Audio->Lock();
    mAudioPacktList.push_back(pkt);
    mConditon_Audio->Signal();
    mConditon_Audio->Unlock();
    std::cout << "inputVideoQuene: successful" << std::endl;

    return true;
}

void MediaProcess::clearVideoQuene()
{
    mConditon_Video->Lock();
    for (AVPacket pkt : mVideoPacktList)
    {
        av_packet_unref(&pkt);
    }
    mVideoPacktList.clear();
    mConditon_Video->Unlock();
}

void MediaProcess::clearAudioQuene()
{
    mConditon_Audio->Lock();
    for (AVPacket pkt : mAudioPacktList)
    {
        av_packet_unref(&pkt);
    }
    mAudioPacktList.clear();
    mConditon_Audio->Unlock();
}

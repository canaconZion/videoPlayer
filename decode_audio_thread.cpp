#include "decode_video_thread.h"
#include "paly_window.h"
#include "ui_paly_window.h"
#include <QDebug>
#include <QTime>
#include <iostream>


void DecodeThread::fill_audio(void *udata,Uint8 *stream,int len){
    //SDL 2.0
    SDL_memset(stream, 0, len);
    if(audio_len==0)
        return;

    len=(len>audio_len?audio_len:len);	/*  Mix  as  much  data  as  possible  */

    SDL_MixAudio(stream,audio_pos,len,SDL_MIX_MAXVOLUME);
    audio_pos += len;
    audio_len -= len;
}

int DecodeThread::decodeAudioThread()
{
    AVFormatContext	*pFormatCtx;
    int				i;
    AVPacket		*packet;

    int ret;
    uint32_t len = 0;
    int got_picture;
    int index = 0;
    int64_t in_channel_layout;

    const char *filepath = source_file.toUtf8().data();
    av_register_all();
    avformat_network_init();
    pFormatCtx = avformat_alloc_context();
    //Open
    if(avformat_open_input(&pFormatCtx,filepath,NULL,NULL)!=0){
        printf("Couldn't open input stream.\n");
        return -1;
    }
    // Retrieve stream information
    if(avformat_find_stream_info(pFormatCtx,NULL)<0){
        printf("Couldn't find stream information.\n");
        return -1;
    }
    // Dump valid information onto standard error
    av_dump_format(pFormatCtx, 0, filepath, false);

    // Find the first audio stream
    audioStream=-1;
    for(i=0; i < pFormatCtx->nb_streams; i++)
        if(pFormatCtx->streams[i]->codec->codec_type==AVMEDIA_TYPE_AUDIO){
            audioStream=i;
            break;
        }

    if(audioStream==-1){
        printf("Didn't find a audio stream.\n");
        return -1;
    }

    // Get a pointer to the codec context for the audio stream
    aCodecCtx=pFormatCtx->streams[audioStream]->codec;

    // Find the decoder for the audio stream
    aCodec=avcodec_find_decoder(aCodecCtx->codec_id);
    if(aCodec==NULL){
        printf("Codec not found.\n");
        return -1;
    }

    // Open codec
    if(avcodec_open2(aCodecCtx, aCodec,NULL)<0){
        printf("Could not open codec.\n");
        return -1;
    }


#if OUTPUT_PCM
    pFile=fopen("output.pcm", "wb");
#endif

    packet=(AVPacket *)av_malloc(sizeof(AVPacket));
    av_init_packet(packet);

    //Out Audio Param
    uint64_t out_channel_layout=AV_CH_LAYOUT_STEREO;
    //nb_samples: AAC-1024 MP3-1152
    int out_nb_samples=aCodecCtx->frame_size;
    AVSampleFormat out_sample_fmt=AV_SAMPLE_FMT_S16;
    int out_sample_rate=44100;
    int out_channels=av_get_channel_layout_nb_channels(out_channel_layout);
    //Out Buffer Size
    int out_buffer_size=av_samples_get_buffer_size(NULL,out_channels ,out_nb_samples,out_sample_fmt, 1);

    out_buffer=(uint8_t *)av_malloc(MAX_AUDIO_FRAME_SIZE*2);
    aFrame=av_frame_alloc();
    //SDL------------------
#if USE_SDL
    //Init
    if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER)) {
        printf( "Could not initialize SDL - %s\n", SDL_GetError());
        return -1;
    }
    //SDL_AudioSpec
    wanted_spec.freq = out_sample_rate;
    wanted_spec.format = AUDIO_S16SYS;
    wanted_spec.channels = out_channels;
    wanted_spec.silence = 0;
    wanted_spec.samples = out_nb_samples;
    wanted_spec.callback = fill_audio;
    wanted_spec.userdata = aCodecCtx;

    if (SDL_OpenAudio(&wanted_spec, NULL)<0){
        printf("can't open audio.\n");
        return -1;
    }
#endif

    //FIX:Some Codec's Context Information is missing
    in_channel_layout=av_get_default_channel_layout(aCodecCtx->channels);
    //Swr

    au_convert_ctx = swr_alloc();
    au_convert_ctx=swr_alloc_set_opts(au_convert_ctx,out_channel_layout, out_sample_fmt, out_sample_rate,
                                      in_channel_layout,aCodecCtx->sample_fmt , aCodecCtx->sample_rate,0, NULL);
    swr_init(au_convert_ctx);

    //Play
    SDL_PauseAudio(0);

    while(av_read_frame(pFormatCtx, packet)>=0){
        if(packet->stream_index==audioStream){
            ret = avcodec_decode_audio4( aCodecCtx, aFrame,&got_picture, packet);
            if ( ret < 0 ) {
                printf("Error in decoding audio frame.\n");
                return -1;
            }
            if ( got_picture > 0 ){
                swr_convert(au_convert_ctx,&out_buffer, MAX_AUDIO_FRAME_SIZE,(const uint8_t **)pFrame->data , pFrame->nb_samples);
#if 1
                printf("index:%5d\t pts:%lld\t packet size:%d\n",index,packet->pts,packet->size);
#endif


#if OUTPUT_PCM
                //Write PCM
                fwrite(out_buffer, 1, out_buffer_size, pFile);
#endif
                index++;
            }

#if USE_SDL
            while(audio_len>0)//Wait until finish
                SDL_Delay(1);

            //Set audio buffer (PCM data)
            audio_chunk = (Uint8 *) out_buffer;
            //Audio buffer length
            audio_len =out_buffer_size;
            audio_pos = audio_chunk;

#endif
        }
        av_free_packet(packet);
    }

    swr_free(&au_convert_ctx);

#if USE_SDL
    SDL_CloseAudio();//Close SDL
    SDL_Quit();
#endif

#if OUTPUT_PCM
    fclose(pFile);
#endif
    av_free(out_buffer);
    avcodec_close(aCodecCtx);
    avformat_close_input(&pFormatCtx);

    return 0;
}

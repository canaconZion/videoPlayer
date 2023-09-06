#ifndef DECODE_MEDIA_H
#define DECODE_MEDIA_H

#include <iostream>
#include <thread>
#include <list>
#include <time.h>
#include <chrono>
extern "C"
{
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libswscale/swscale.h"
#include "libavutil/imgutils.h"
#include "libswresample/swresample.h"
#include "SDL.h"
}
#include "cond.h"

#define MAX_AUDIO_FRAME_SIZE 192000 // 1 second of 48khz 32bit audio

#define SFM_REFRESH_EVENT (SDL_USEREVENT + 1)
#define SFM_BREAK_EVENT (SDL_USEREVENT + 2)
#define MAX_VIDEO_SIZE (25 * 20)

class MediaProcess : public QObject
{
    Q_OBJECT
public:
    MediaProcess(QObject *parent = nullptr);
    ~MediaProcess();
    int srp_refresh_thread(void *opaque);
    int media_decode();
    int video_decode();
    int audio_decode();
    int sdl_init();
    // void fill_audio(void *udata, Uint8 *stream, int len);

    int thread_exit;
    int thread_pause;
    // static Uint8 *audio_chunk;
    // static Uint32 audio_len;
    // static Uint8 *audio_pos;

    AVFormatContext *pFormatCtx;

    //play control
    bool mIsReadFinished;

    // video stream
    int i, videoindex;
    AVCodecContext *pCodecCtx;
    AVCodec *pCodec;
    AVCodecParameters *pCodecParams;
    AVFrame *pFrame, *pFrameYUV;
    unsigned char *out_buffer;
    // AVPacket *packet;
    int ret, got_picture;
    AVStream *mVideoStream; //视频流

    // audio stream
    int index;
    int audioindex;
    AVCodecContext *aCodecCtx;
    AVCodec *aCodec;
    uint8_t *a_out_buffer;
    AVFrame *aFrame;
    SDL_AudioSpec wanted_spec;
    int64_t in_channel_layout;
    struct SwrContext *au_convert_ctx;
    int out_buffer_size;
    int a_got_picture;
    AVStream *mAudioStream; //音频流

    // SDL
    int screen_w, screen_h;
    SDL_Window *screen;
    SDL_Renderer *sdlRenderer;
    SDL_Texture *sdlTexture;
    SDL_Rect sdlRect_1;
    SDL_Thread *video_tid;
    SDL_Event event;

    struct SwsContext *img_convert_ctx;

    char *filepath;

    // video frame list
    Cond *mConditon_Video;
    std::list<AVPacket> mVideoPacktList;
    bool inputVideoQuene(const AVPacket &pkt);
    void clearVideoQuene();

    ///audio frame list
    Cond *mConditon_Audio;
    std::list<AVPacket> mAudioPacktList;
    bool inputAudioQuene(const AVPacket &pkt);
    void clearAudioQuene();
private:
};

#endif


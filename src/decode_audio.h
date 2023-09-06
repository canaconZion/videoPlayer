#ifndef DECODE_AUDIO_H
#define DECODE_AUDIO_H
#include <QObject>
#include <QString>

extern "C"
{
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libswresample/swresample.h"
#include "SDL.h"
}

class AudioDecod:public QObject
{
    Q_OBJECT
public:
    AudioDecod(QObject *parent = nullptr);
    ~AudioDecod();
    bool isPlay;
    bool isPause;
    bool isOver;

    // ffmpeg
    AVFormatContext	*pFormatCtx;
    int				i, audioStream;
    AVCodecContext	*pCodecCtx;
    AVCodec			*pCodec;
    AVPacket		*packet;
    uint8_t			*out_buffer;
    AVFrame			*pFrame;
    SDL_AudioSpec wanted_spec;
    int ret;
    uint32_t len = 0;
    int got_picture;
    int index = 0;
    int64_t in_channel_layout;
    struct SwrContext *au_convert_ctx;


//public slots:
    void decodeThread(QString file_path);
};

#endif // DECODE_AUDIO_H

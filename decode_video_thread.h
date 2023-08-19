#ifndef DECODE_VIDEO_THREAD_H
#define DECODE_VIDEO_THREAD_H
#include <QObject>
#include <QString>
#include <thread>
extern "C"
{
#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#include <libavutil/time.h>
#include "SDL.h"
#include "SDL_video.h"
#include "SDL_rect.h"
#include "SDL_render.h"
}

#define MAX_AUDIO_FRAME_SIZE 192000 // 1 second of 48khz 32bit audio

//Output PCM
#define OUTPUT_PCM 1
//Use SDL
#define USE_SDL 1

//Buffer:
//|-----------|-------------|
//chunk-------pos---len-----|
static  Uint8  *audio_chunk;
static  Uint32  audio_len;
static  Uint8  *audio_pos;

class DecodeThread : public QObject
{
    Q_OBJECT
public:
    DecodeThread(QObject *parent = nullptr);
    ~DecodeThread();
    bool isPlay;
    bool isPause;
    bool isSeek;
    bool isFileChange;
    long total_time;
    long curr_time;
    long seek_pos;
    double frameRate;
    int bitRate;
    QString decoder;
    QString source_file;
    bool startPlay(QString filePath);

protected:
    void readVideoFile();
    void decodeVideoThread();
    int decodeAudioThread();
    void fill_audio(void *udata,Uint8 *stream,int len);

    int decodeAudioFrame(bool isBlock = false);

private:
    bool mIsMute;
    float mVolume; // 音量 0~1 超过1 表示放大倍数

    /// 跳转相关的变量
    int seek_req = 0;    // 跳转标志
    // int64_t seek_pos;    // 跳转的位置 -- 微秒
    int seek_flag_audio; // 跳转标志 -- 用于音频线程中
    int seek_flag_video; // 跳转标志 -- 用于视频线程中
    double seek_time;    // 跳转的时间(秒)  值和seek_pos是一样的

    /// 播放控制相关
    bool mIsNeedPause;    // 暂停后跳转先标记此变量
    bool mIsPause;        // 暂停标志
    bool mIsQuit;         // 停止
    bool mIsReadFinished; // 文件读取完毕
    bool mIsReadThreadFinished;
    bool mIsVideoThreadFinished; // 视频解码线程
    bool mIsAudioThreadFinished; // 音频播放线程

    /// 音视频同步相关
    uint64_t mVideoStartTime; // 开始播放视频的时间
    uint64_t mPauseStartTime; // 暂停开始的时间
    double audio_clock;       /// 音频时钟
    double video_clock;       ///< pts of last decoded frame / predicted pts of next decoded frame
    AVStream *mVideoStream;   // 视频流
    AVStream *mAudioStream;   // 音频流

    AVPacket *packet;
    /// 视频相关
    int videoStream;
    AVFormatContext *pFormatCtx;
    AVCodecContext *pCodecCtx;
    AVCodec *pCodec;

    /// 音频相关
    int audioStream;
    int a_i;
    SDL_AudioSpec wanted_spec;
    FILE *pFile=NULL;
    uint8_t			*out_buffer;

    AVCodecContext *aCodecCtx;
    AVCodec *aCodec;
    AVFrame *aFrame;
    AVFrame *aFrame_ReSample;
    SwrContext *au_convert_ctx;

signals:
    void sigGetFrame(AVFrame *pFrame);
    void sigGetVideoInfo(int mWidth, int mHeight);
    void sigGetCurrentPts(long, long);

public slots:
    void slotDoWork(QString palyFile);
};

#endif // DECODE_VIDEO_THREAD_H

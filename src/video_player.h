#ifndef VIDEO_PLAYER_H
#define VIDEO_PLAYER_H
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

#include "cond.h"
//Output PCM
#define OUTPUT_PCM 1
//Use SDL
#define USE_SDL 1

#define MAX_AUDIO_FRAME_SIZE 192000 // 1 second of 48khz 32bit audio

#define SFM_REFRESH_EVENT (SDL_USEREVENT + 1)
#define SFM_BREAK_EVENT (SDL_USEREVENT + 2)
#define MAX_AUDIO_SIZE (50 * 20)
#define MAX_VIDEO_SIZE (25 * 20)

#define FLUSH_DATA "FLUSH"

class VideoPlayer : public QObject
{
    Q_OBJECT
public:
    VideoPlayer(QObject *parent = nullptr);
    ~VideoPlayer();

    static bool init_player();


    bool v_pause;
    bool v_play;
    // ------ old element --------
    bool isSeek;
    bool isFileChange;
    long total_time;
    long curr_time;
    long seek_pos;
    double frameRate;
    int bitRate;
    QString decoder;
    // ----------------------------


private:

    // player control
    bool v_read_finished;
    bool v_quit;
    bool v_video_thread_finished;
    bool v_audio_thread_finished;

    double audio_clock;
    double video_clock;

    QString v_file_path;
    AVStream *mVideoStream; //视频流
    AVStream *mAudioStream; //音频流
    // 视频
    AVFormatContext *pFormatCtx;
    AVCodecContext *pCodecCtx;
    AVCodec *pCodec;
    // 音频
    AVCodecContext *aCodecCtx;
    AVCodec *aCodec;
    AVFrame *aFrame;

    AVFrame *aFrame_ReSample;
    SwrContext *swrCtx;

    // 视频队列
    Cond *mConditon_Video;
    std::list<AVPacket> mVideoPacktList;
    bool inputVideoQuene(const AVPacket &pkt);
    void clearVideoQuene();

    // 音频队列
    Cond *mConditon_Audio;
    std::list<AVPacket> mAudioPacktList;
    bool inputAudioQuene(const AVPacket &pkt);
    void clearAudioQuene();

protected:
    void decode_video_thread();
    void decode_audio_thread();

signals:
    void sigGetFrame(AVFrame *pFrame);

public slots:
//    bool start_play(QString file_path);
    void read_video_file(QString file_path);

};

#endif // VIDEO_PLAYER_H

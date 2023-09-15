#ifndef VIDEO_PLAYER_H
#define VIDEO_PLAYER_H
#include <QObject>
#include <QString>
#include <thread>
extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/time.h>
#include <libavutil/pixfmt.h>
#include <libavutil/display.h>
#include <libavutil/avstring.h>
#include <libavutil/opt.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#include <libavutil/imgutils.h>
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>

#include <SDL.h>
#include <SDL_audio.h>
#include <SDL_types.h>
#include <SDL_name.h>
#include <SDL_main.h>
#include <SDL_config.h>
}

#include "cond.h"
//Output PCM
#define OUTPUT_PCM 1
//Use SDL
#define USE_SDL 1

#define MAX_AUDIO_FRAME_SIZE 192000 // 1 second of 48khz 32bit audio
#define AVCODEC_MAX_AUDIO_FRAME_SIZE 192000

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
    bool v_isSeek;
    bool v_isFileChange;
    long v_total_time;
    long v_curr_time;
    long seek_pos;
    double v_frameRate;
    int v_bitRate;
    QString v_decoder;
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

    enum AVSampleFormat in_sample_fmt; //输入的采样格式
    enum AVSampleFormat out_sample_fmt;//输出的采样格式 16bit PCM
    int in_sample_rate;//输入的采样率
    int out_sample_rate;//输出的采样率
    int audio_tgt_channels; ///av_get_channel_layout_nb_channels(out_ch_layout);
    int out_ch_layout;
    unsigned int audio_buf_size;
    unsigned int audio_buf_index;
    DECLARE_ALIGNED(16,uint8_t,audio_buf) [AVCODEC_MAX_AUDIO_FRAME_SIZE * 4];

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

    SDL_AudioDeviceID mAudioID;
    int set_audio_SDL();
    void close_audio_SDL();

    int configure_filtergraph(AVFilterGraph *graph, const char *filtergraph, AVFilterContext *source_ctx, AVFilterContext *sink_ctx);
    int configure_video_filters(AVFilterGraph *graph, const char *vfilters, AVFrame *frame);

protected:
    void read_video_file();
    void decode_video_thread();
    int decode_audio_thread(bool isBlock = false);

    static void sdlAudioCallBackFunc(void *userdata, Uint8 *stream, int len);
    void sdlAudioCallBack(Uint8 *stream, int len);


signals:
    void sigGetFrame(AVFrame *pFrame);
    void sigGetVideoInfo(int mWidth, int mHeight);
    void sigGetCurrentPts(long, long);

public slots:
    bool start_play(QString file_path);


};

#endif // VIDEO_PLAYER_H

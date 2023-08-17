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
}

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

    /// 视频相关
    AVFormatContext *pFormatCtx;
    AVCodecContext *pCodecCtx;
    AVCodec *pCodec;

    /// 音频相关
    AVCodecContext *aCodecCtx;
    AVCodec *aCodec;
    AVFrame *aFrame;

    AVFrame *aFrame_ReSample;
    SwrContext *swrCtx;

signals:
    void sigGetFrame(AVFrame *pFrame);
    void sigGetVideoInfo(int mWidth, int mHeight);
    void sigGetCurrentPts(long, long);

public slots:
    void slotDoWork(QString palyFile);
};

#endif // DECODE_VIDEO_THREAD_H

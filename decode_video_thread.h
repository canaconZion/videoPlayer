#ifndef DECODE_VIDEO_THREAD_H
#define DECODE_VIDEO_THREAD_H
#include <QObject>
#include <QString>
extern "C"
{
#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
}

class DecodeThread:public QObject
{
    Q_OBJECT
public:
    DecodeThread(QObject *parent = nullptr);
    ~DecodeThread();
private:
    bool isPlay;
    bool isPause;
    QString source_file;

signals:
    void sigGetFrame(AVFrame *pFrame);
    void sigGetVideoInfo(int mWidth,int mHeight);
    void sigGetCurrentPts(long,long);

public slots:
    void slotDoWork(QString palyFile);
};

#endif // DECODE_VIDEO_THREAD_H

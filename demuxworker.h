#ifndef AVDEMUXTHREAD_H
#define AVDEMUXTHREAD_H

extern "C"
{
#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
}


#include <QThread>
#include <QImage>
#include <QLabel>
#include <QObject>
#include <QThread>
#include <QString>


class DemuxWorker:public QObject
{
    Q_OBJECT

public:
    DemuxWorker(QObject *parent = nullptr);
    ~DemuxWorker();

public:
    bool isPlay;
    bool isPause;
    bool doSeek;
    long seekPos;
    QString filePath;

signals:
    void sigGetFrame(AVFrame *pFrame);
    void sigGetVideoInfo(int mWidth,int mHeight);
    void sigGetCurrentPts(long,long);

public slots:
    void slotDoWork(QString palyFile);
};

#endif // AVDEMUXTHREAD_H

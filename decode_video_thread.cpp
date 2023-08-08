#include "decode_video_thread.h"
#include <QDebug>

DecodeThread::DecodeThread(QObject *parent):QObject (parent)
{

}

DecodeThread::~DecodeThread()
{

    qDebug() << "thread quit";
}
void DecodeThread::slotDoWork(QString palyFile)
{

}

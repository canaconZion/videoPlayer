#include <QApplication>
#include "paly_window.h"

#undef main
int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    paly_window win;
    win.setWindowTitle("🎞MY VIDEO PLAYER🎞");
    win.show();
    //    MediaProcess b;
    //    b.filepath = "D:\\workdir\\ffmpeg\\streaming-practice\\ffmpeg\\bin\\cat.flv";
    //    // a.filepath = "cat.flv";
    //    b.media_decode();
    return a.exec();
}

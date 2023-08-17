#include <QApplication>
#include "paly_window.h"

#undef main
int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    paly_window win;
    win.setWindowTitle("🎞MY VIDEO PLAYER🎞");
    win.show();
    return a.exec();
}

#include <QApplication>
#include "mediawindow.h"
#include "ui_mediawindow.h"

#undef main
int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    MediaWindow *mediaWindow = new MediaWindow();
//    testWindow tw;
    //    tw.getUi()->choose->setText("Hello world");
    //    tw.getUi()->start->setText("World strat");
    mediaWindow->show();
//    tw.show();
    int ret = a.exec();
    return ret;
}

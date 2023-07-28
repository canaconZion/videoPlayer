#ifndef TESTWINDOW_H
#define TESTWINDOW_H

#include <QWidget>

namespace Ui {
class testWindow;
}

class testWindow : public QWidget
{
    Q_OBJECT

public:
    explicit testWindow(QWidget *parent = 0);
    Ui::testWindow *getUi() const { return ui; }
    ~testWindow();

private:
    Ui::testWindow *ui;
};

#endif // TESTWINDOW_H

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

#include "finder.h"

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

private:
    Ui::MainWindow *ui;
    finder bg_finder;
};

#endif // MAINWINDOW_H

#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    connect(ui->lineEdit_directory, &QLineEdit::textChanged, &bg_finder, &finder::set_directory);
    connect(ui->lineEdit_text_to_search, &QLineEdit::textChanged, &bg_finder, &finder::set_text_to_search);

    connect(&bg_finder, &finder::result_changed, this, [this]
    {
        auto result = bg_finder.get_result();

        if (result.first)
            ui->listWidget_result->clear();

        ui->listWidget_result->addItems(result.items);
        ui->progressBar->setValue(result.progress);
    });
}

MainWindow::~MainWindow()
{
    delete ui;
}

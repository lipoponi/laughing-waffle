#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow),
    timer(this)
{
    ui->setupUi(this);

    connect(ui->lineEdit_directory, &QLineEdit::textChanged, this, [this](QString value){
        QDir current(value);
        QString color = current.exists() ? "green" : "red";

        ui->lineEdit_directory->setStyleSheet("color: " + color);
    });

    connect(ui->lineEdit_directory, &QLineEdit::textChanged, &bg_finder, &finder::set_directory);
    connect(ui->lineEdit_text_to_search, &QLineEdit::textChanged, &bg_finder, &finder::set_text_to_search);

    connect(&timer, &QTimer::timeout, this, [this]
    {
        auto result = bg_finder.get_result();

        if (result.first)
            ui->listWidget_result->clear();

        ui->listWidget_result->addItems(result.items);
        ui->progressBar->setValue(result.progress);
    });
    timer.start(100);

    connect(ui->pushButton_browse, &QPushButton::clicked, this, [this]{
        QFileDialog dialog(this);
        dialog.setFileMode(QFileDialog::DirectoryOnly);
        dialog.setDirectory(QDir::rootPath());

        if (dialog.exec()) {
            ui->lineEdit_directory->setText(dialog.selectedFiles().first());
        }
    });

    connect(ui->pushButton_stop, &QPushButton::clicked, this, [this]{
        bg_finder.stop();
    });
}

MainWindow::~MainWindow()
{
    delete ui;
}

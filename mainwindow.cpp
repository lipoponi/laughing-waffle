#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , pollingTimer(this)
    , statusAnimationTimer(this)
{
    ui->setupUi(this);

    connect(ui->lineEdit_directory, &QLineEdit::textChanged, this, &MainWindow::onDirectoryChange);
    connect(ui->lineEdit_pattern, &QLineEdit::textChanged, this, &MainWindow::onPatternChange);
    connect(ui->pushButton_browse, &QPushButton::clicked, this, &MainWindow::onBrowseClick);
    connect(ui->pushButton_restart, &QPushButton::clicked, this, &MainWindow::onRestartClick);
    connect(ui->pushButton_stop, &QPushButton::clicked, this, &MainWindow::onStopClick);
    connect(&pollingTimer, &QTimer::timeout, this, &MainWindow::onPollingTimeout);
    connect(&statusAnimationTimer, &QTimer::timeout, this, &MainWindow::onStatusAnimationTimeout);

    ui->statusBar->addWidget(ui->label_status);
    ui->statusBar->addPermanentWidget(ui->label_metrics);

    ui->lineEdit_directory->setText(QDir::homePath());

    statusAnimationTimer.setSingleShot(true);
    pollingTimer.start(pollingInterval);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::onDirectoryChange(const QString &value)
{
    bool exists = QDir(value).exists();
    QString color = exists ? "green" : "red";

    ui->lineEdit_directory->setStyleSheet("color: " + color);
    bgFinder.setDirectory(value);
}

void MainWindow::onPatternChange(const QString &value)
{
    bgFinder.setPattern(value);
}

void MainWindow::onBrowseClick()
{
    QString dir = QFileDialog::getExistingDirectory(this, "Select Directory",
                                                    QDir::homePath(),
                                                    QFileDialog::ShowDirsOnly);
    if (dir.size() == 0) {
        return;
    }

    ui->lineEdit_directory->setText(dir);
}

void MainWindow::onRestartClick()
{
    bgFinder.setDirectory(ui->lineEdit_directory->text());
}

void MainWindow::onStopClick()
{
    bgFinder.stop();
}

void MainWindow::onPollingTimeout()
{
    auto result = bgFinder.getResult();

    if (result.first) {
        ui->listWidget->clear();
    }

    QString fileLine = "%1:%2:%3";
    QString chunkLine = "    %1%2%3";

    for (auto &item : result.list) {
        ui->listWidget->addItem(fileLine.arg(item.filePath).arg(item.line).arg(item.position));
        ui->listWidget->addItem(chunkLine.arg(item.before).arg(item.entry).arg(item.after));
    }

    auto status = bgFinder.getStatus();
    switch (status) {
    case 1:
        statusAnimationTimer.start(0);
        break;
    case 2:
        setStatus("Finished");
        break;
    case 3:
        setStatus("Stopped by user");
        break;
    case 4:
        setStatus("Too many matches");
        break;
    default:
        setStatus("");
    }

    auto metrics = bgFinder.getMetrics();
    QString line = "Scanned: %1 | Total: %2 | Scanned size: %3 | Total size: %4";
    line = line.arg(metrics.scannedCount).arg(metrics.totalCount);
    line = line.arg(humanizeSize(metrics.scannedSize)).arg(humanizeSize(metrics.totalSize));

    ui->label_metrics->setText(line);
}

void MainWindow::onStatusAnimationTimeout()
{
    static int tick = 0;
    setStatus("Working" + QString(tick, '.'));
    tick = (tick + 1) % 4;
    statusAnimationTimer.start(statusAnimationInterval);
}

void MainWindow::setStatus(const QString &status)
{
    QString color = "black";
    statusAnimationTimer.stop();

    if (status.startsWith("Working")) {
        color = "orange";
        statusAnimationTimer.start(statusAnimationInterval);
    } else if (status == "Finished") {
        color = "green";
    } else if (status == "Stopped by user") {
        color = "red";
    } else if (status == "Too many matches") {
        color = "purple";
    }

    QString format("<span style=\"font-weight: bold; color: %1\">%2</span>");
    ui->label_status->setText(format.arg(color).arg(status));
}

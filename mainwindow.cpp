#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , pollingTimer(this)
    , statusAnimationTimer(this)
    , listSize(0)
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

void MainWindow::updateList()
{
    auto result = bgFinder.getResult();

    if (result.first) {
        ui->textBrowser->clear();
        listSize = 0;
    }

    QString file = "<font color=purple>%1</font>";
    QString format = "%1<font color=blue><b>%2</b></font>%3";

    for (auto &item : result.list) {
        if (maxListSize <= listSize) break;

        listSize++;
        ui->textBrowser->append(file.arg(item.filePath));
        ui->textBrowser->append(format
                                .arg(item.before.toHtmlEscaped())
                                .arg(item.entry.toHtmlEscaped())
                                .arg(item.after.toHtmlEscaped()));
        ui->textBrowser->append("");
    }
}

void MainWindow::updateStatus()
{
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
    default:
        setStatus("");
    }
}

void MainWindow::updateMetrics()
{
    auto metrics = bgFinder.getMetrics();
    QString format = "Scanned: %1 (%2) | Found: %3 (%4)";

    ui->label_metrics->setText(format
                               .arg(humanizeSize(metrics.scannedSize))
                               .arg(metrics.scannedCount)
                               .arg(humanizeSize(metrics.totalSize))
                               .arg(metrics.totalCount));
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
    updateList();
    updateStatus();
    updateMetrics();
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

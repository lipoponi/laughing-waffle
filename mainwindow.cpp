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
    connect(ui->pushButton_saveResults, &QPushButton::clicked, this, &MainWindow::saveResults);
    connect(&pollingTimer, &QTimer::timeout, this, &MainWindow::onPollingTimeout);
    connect(&statusAnimationTimer, &QTimer::timeout, this, &MainWindow::onStatusAnimationTimeout);

    ui->statusBar->addWidget(ui->label_status);
    ui->statusBar->addPermanentWidget(ui->label_metrics);

    ui->lineEdit_directory->setText(QDir::homePath());
    ui->label_forList->setText(QString("First %1 entries:").arg(maxListSize));

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
        ui->textBrowser_result->clear();
        store.clear();
        listSize = 0;
    }

    QString fileFormat = "<font color=purple>%1</font>:%2";
    QString fragmentFormat = "%1<font color=blue><b>%2</b></font>%3";

    for (auto &item : result.list) {
        store[item.filePath].push_back(item);

        if (maxListSize <= listSize) continue;

        listSize++;
        ui->textBrowser_result->append(fileFormat.arg(item.filePath).arg(item.line));
        ui->textBrowser_result->append(fragmentFormat
                                .arg(item.before.toHtmlEscaped())
                                .arg(item.entry.toHtmlEscaped())
                                .arg(item.after.toHtmlEscaped()));
        ui->textBrowser_result->append("");
    }
}

void MainWindow::updateStatus()
{
    auto status = bgFinder.getStatus();
    switch (status) {
    case 0:
        setStatus("IDLE");
        break;
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
    QString color = "black";
    if (bgFinder.isCrawlingFinished()) {
        color = "green";
    }

    QString format = "Scanned: %1 (%2) | Found: <font color=" + color + ">%3 (%4)</font>";

    ui->label_metrics->setText(format
                               .arg(humanizeSize(metrics.scannedSize))
                               .arg(metrics.scannedCount)
                               .arg(humanizeSize(metrics.totalSize))
                               .arg(metrics.totalCount));
}

void MainWindow::saveResults()
{
    QString filePath = QFileDialog::getSaveFileName(this, "Select Destination", QDir::homePath());

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        QMessageBox::information(this, "Save Results", "Unsuccessfully");
        return;
    }
    QTextStream fstream(&file);

    QString fileFormat = "<font color=purple>%1</font>";
    QString fragmentFormat = "<font color=gray>%1</font>:\t%2<font color=blue><b>%3</b></font>%4";

    fstream << "<pre>";
    for (auto &filePath : store.keys()) {
        fstream << fileFormat.arg(filePath) << endl;
        for (Finder::Entry &item : store[filePath]) {
            fstream << fragmentFormat
                       .arg(item.line)
                       .arg(item.before.toHtmlEscaped())
                       .arg(item.entry.toHtmlEscaped())
                       .arg(item.after.toHtmlEscaped()) << endl;
        }
        fstream << endl;
    }
    fstream << "</pre>";

    QMessageBox::information(this, "Save Results", "Success");
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

    if (status == "IDLE") {
        color = "blue";
    } else if (status.startsWith("Working")) {
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

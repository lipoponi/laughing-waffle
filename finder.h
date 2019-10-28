#ifndef FINDER_H
#define FINDER_H

#include <array>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QObject>
#include <QTextStream>

class finder : public QObject
{
    Q_OBJECT
public:
    struct result_t
    {
        bool first = true;
        int progress = 0;
        QStringList items;
    };

private:
    struct params_t
    {
        bool invalid = true;
        bool done = true;
        QString directory;
        QString text_to_search;
    };

    struct file_size_cmp
    {
        bool operator()(const QString &lhs, const QString &rhs);
    };

public:
    finder();
    ~finder();
    result_t get_result();

private:
    void crawl(QString directory);
    void enque_file_to_scan(QString file_path);
    void scan(QString file_path, QString text, std::atomic<bool> &cancel);
    void queue_callback();

signals:
    void result_changed();

public slots:
    void set_directory(QString directory);
    void set_text_to_search(QString text);

private slots:
    void callback();

private:
    static const int scan_threads_count = 4;

    params_t params;
    result_t result;
    std::priority_queue<QString, std::vector<QString>, file_size_cmp> file_queue;
    bool quit;
    std::atomic<bool> cancel;
    std::atomic<bool> callback_queued;

    mutable std::mutex params_m;
    mutable std::mutex queue_m;
    mutable std::mutex result_m;
    std::condition_variable crawler_has_work_cv;
    std::condition_variable scanner_has_work_cv;

    std::thread crawl_thread;
    std::vector<std::thread> scan_threads;
    std::array<std::atomic<bool>, finder::scan_threads_count> scan_cancel;
};

#endif // FINDER_H

#ifndef FINDER_H
#define FINDER_H

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <QObject>

class finder : public QObject
{
    Q_OBJECT
private:
    struct params_t
    {
        bool invalid = true;
        bool done = true;
        QString directory;
        QString text_to_search;
    };

    friend struct scan_thread_t
    {
        scan_thread_t();
        scan_thread_t(void(*fun)(scan_thread_t *obj));

        std::atomic<bool> cancel;
        std::thread thread;
    };

public:
    finder();

signals:

public slots:

private:
    static int scan_threads_count = 4;

    params_t params;
    bool quit;

    std::mutex params_m;
    std::mutex queue_m;
    std::condition_variable crawler_has_work_cv;

    std::vector<scan_thread_t> scan_threads;
    std::thread crawl_thread;
};

#endif // FINDER_H

#include "finder.h"

bool finder::file_size_cmp::operator()(const QString &lhs, const QString &rhs)
{
    return QFile(rhs).size() < QFile(lhs).size();
}

finder::finder()
    : total_size(0)
    , scanned_size(0)
    , quit(false)
    , cancel(false)
    , callback_queued(false)
    , crawl_thread([this]
{
    for (;;)
    {
        std::unique_lock<std::mutex> params_lg(params_m);
        crawler_has_work_cv.wait(params_lg, [this]
        {
            return !params.done || quit;
        });

        {
            std::lock_guard<std::mutex> queue_lg(queue_m);
            file_queue = decltype(file_queue)();
            for (auto &current : scan_cancel)
                current.store(true);
        }

        {
            std::lock_guard<std::mutex> result_lg(result_m);
            result.first = true;
            result.items.clear();
            queue_callback();
        }

        total_size = 0;
        scanned_size = 0;

        if (quit)
            return;

        params.done = true;
        if (params.invalid)
            continue;

        QString dir = params.directory;
        cancel.store(false);
        params_lg.unlock();

        crawl(dir);
    }
})
{
    for (int i = 0; i != finder::scan_threads_count; i++)
    {
        scan_threads.emplace_back([this, i]
        {
            for (;;)
            {
                std::unique_lock<std::mutex> queue_lg(queue_m);
                scanner_has_work_cv.wait(queue_lg, [this]
                {
                    return !file_queue.empty() || quit;
                });

                if (quit)
                    return;

                QString file_path = file_queue.top();
                QString text;
                file_queue.pop();
                scan_cancel[i].store(false);
                queue_lg.unlock();

                {
                    std::lock_guard<std::mutex> params_lg(params_m);
                    text = params.text_to_search;
                }

                scan(file_path, text, scan_cancel[i]);
            }
        });
    }
}

finder::~finder()
{
    cancel.store(true);
    {
        std::lock_guard<std::mutex> params_lg(params_m);
        quit = true;
        crawler_has_work_cv.notify_all();
        scanner_has_work_cv.notify_all();
    }
    crawl_thread.join();
    for (size_t i = 0; i != scan_threads.size(); i++)
    {
        scan_threads[i].join();
    }
}

void finder::set_directory(QString directory)
{
    std::lock_guard<std::mutex> params_lg(params_m);
    params.done = false;
    params.directory = directory;
    params.invalid = params.directory.isEmpty() || params.text_to_search.isEmpty();

    cancel.store(true);
    crawler_has_work_cv.notify_all();
}

void finder::set_text_to_search(QString text)
{
    std::lock_guard<std::mutex> params_lg(params_m);
    params.done = false;
    params.text_to_search = text;
    params.invalid = params.directory.isEmpty() || params.text_to_search.isEmpty();

    cancel.store(true);
    crawler_has_work_cv.notify_all();
}

void finder::stop() {
    std::lock_guard<std::mutex> lg(queue_m);

    cancel.store(true);
    for (size_t i = 0; i < scan_cancel.size(); i++) {
        scan_cancel[i].store(true);
    }

    file_queue = decltype(file_queue)();
}

void finder::crawl(QString directory)
{
    QDirIterator dir_it(directory,
                        QDir::Dirs | QDir::NoSymLinks | QDir::NoDotAndDotDot | QDir::Hidden,
                        QDirIterator::Subdirectories);

    QDir current(directory);

    do
    {
        if (cancel.load())
            return;

        current.setFilter(QDir::Files | QDir::Hidden | QDir::NoSymLinks);

        auto files = current.entryInfoList();

        for (auto file : files)
        {
            if (cancel.load())
                return;

            if (!file.permission(QFile::ReadUser))
                continue;

            enque_file_to_scan(file.absoluteFilePath());
            total_size += file.size();
        }

        queue_callback();
    }
    while (dir_it.hasNext() && (current = QDir(dir_it.next())).exists());
}

void finder::scan(QString file_path, QString text, std::atomic<bool> &cancel)
{
    QFile file_obj(file_path);
    if (!file_obj.open(QIODevice::ReadOnly | QIODevice::Text))
        return;

    QTextStream in(&file_obj);
    QString block;

    while (!in.atEnd())
    {
        if (cancel.load())
            break;

        QString buffer = std::move(block);
        block = in.read(finder::reading_block_size);
        buffer.append(block);
        if (buffer.contains(text))
        {
            std::lock_guard<std::mutex> result_lg(result_m);
            if (cancel.load())
                break;

            result.items.push_back(file_path);
            queue_callback();
            break;
        }
    }

    scanned_size += QFile(file_path).size();
    queue_callback();
}

finder::result_t finder::get_result()
{
    std::lock_guard<std::mutex> result_lg(result_m);

    unsigned long long all_cnt = total_size.load();
    unsigned long long done_cnt = scanned_size.load();

    result_t tmp = result;
    result.first = false;
    result.items.clear();
    tmp.progress = all_cnt == 0 ? 0 : done_cnt *100 / all_cnt;
    return tmp;
}

void finder::enque_file_to_scan(QString file_path)
{
    std::lock_guard<std::mutex> queue_lg(queue_m);
    file_queue.push(file_path);
    scanner_has_work_cv.notify_one();
}

void finder::queue_callback()
{
    if (callback_queued.load())
        return;

    callback_queued.store(true);
    QMetaObject::invokeMethod(this, "callback", Qt::QueuedConnection);
}

void finder::callback()
{
    callback_queued.store(false);

    emit result_changed();
}

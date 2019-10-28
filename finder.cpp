#include "finder.h"

finder::scan_thread_t::scan_thread_t(finder *parent)
    : cancel(false)
    , thread([this, parent]
{
    for (;;)
    {
        std::unique_lock<std::mutex> queue_lg(parent->queue_m);
        parent->scanner_has_work_cv.wait(queue_lg, [this, parent]
        {
            return !parent->file_queue.empty() || parent->quit;
        });

        if (parent->quit)
            return;

        QString file_path = file_queue.top();
        QString text;
        file_queue.pop();
        cancel.store(false);
        queue_lg.unlock();

        {
            std::lock_guard<std::mutex> params_lg(parent->params_m);
            text = parent->params.text_to_search;
        }

        parent->scan(file_path, text, cancel);
    }
})
{}

finder::finder()
    : scan_threads(finder::scan_threads_count, scan_thread_t(this))
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
            for (scan_thread_t &current : scan_threads)
                current.cancel.store(true);
        }

        {
            std::lock_guard<std::mutex> result_lg(result_m);
            result.first = true;
            result.items.clear();
        }

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
{}

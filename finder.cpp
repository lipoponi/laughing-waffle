#include "finder.h"

finder::finder()
    : crawl_thread([this]
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
{
    for (int i = 0; i != scan_threads.size(); i++)
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

void finder::set_directory(QString directory)
{
    std::lock_guard<std::mutex> params_lg(params_m);
    params.done = false;
    params.directory = directory;
    params.invalid = params.directory.isEmpty() || params.text_to_search.isEmpty();
}

void finder::set_text_to_search(QString text)
{
    std::lock_guard<std::mutex> params_lg(params_m);
    params.done = false;
    params.text_to_search = text;
    params.invalid = params.directory.isEmpty() || params.text_to_search.isEmpty();
}

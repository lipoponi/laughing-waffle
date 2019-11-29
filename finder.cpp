#include "finder.h"

Finder::EntryList::EntryList()
    : first(true)
{}

Finder::Params::Params()
    : invalid(true)
    , done(true)
{}

bool Finder::FileSizeCmp::operator()(const QString &lhs, const QString &rhs)
{
    return QFile(rhs).size() < QFile(lhs).size();
}

Finder::Finder()
    : QObject(nullptr)
    , entryCount(0)
    , statusCode(0)
    , crawlFinished(false)
    , scannedCount(0)
    , scannedSize(0)
    , totalCount(0)
    , totalSize(0)
    , quit(false)
    , cancel(false)
    , crawlThread([this]
{
    for (;;) {
        std::unique_lock<std::mutex> paramsLg(paramsM);
        crawlerHasWorkCv.wait(paramsLg, [this]
        {
            return !params.done || quit;
        });

        {
            std::lock_guard<std::mutex> queueLg(queueM);
            fileQueue = FileQueue();
            queuedPattern = params.pattern;
            for (auto &current : scanCancel) {
                current.store(true);
            }
        }

        {
            std::lock_guard<std::mutex> resultLg(resultM);
            result.first = true;
            result.list.clear();
        }

        if (quit) {
            return;
        }

        params.done = true;
        if (params.invalid) {
            continue;
        }

        cancel.store(false);
        entryCount.store(0);
        statusCode.store(1);
        scannedCount.store(0);
        scannedSize.store(0);
        totalCount.store(0);
        totalSize.store(0);
        crawlFinished.store(false);

        QString dir = params.directory;
        paramsLg.unlock();

        crawl(dir);
        crawlFinished.store(true);

        if (crawlFinished.load() && scannedCount.load() == totalCount.load()) {
            statusCode.store(2);
        }
    }
})
{
    for (int i = 0; i != Finder::scanThreadsCount; i++)
    {
        scanThreads.emplace_back([this, i]
        {
            for (;;)
            {
                std::unique_lock<std::mutex> queueLg(queueM);
                scannerHasWorkCv.wait(queueLg, [this]
                {
                    return !fileQueue.empty() || quit;
                });

                if (quit) {
                    return;
                }

                QString filePath = fileQueue.top();
                QString pattern = queuedPattern;
                fileQueue.pop();
                scanCancel[i].store(false);
                queueLg.unlock();

                scan(filePath, pattern, scanCancel[i]);

                if (crawlFinished.load() && scannedCount.load() == totalCount.load()) {
                    statusCode.store(2);
                }
            }
        });
    }
}

Finder::~Finder()
{
    cancel.store(true);
    {
        std::lock_guard<std::mutex> paramsLg(paramsM);
        quit = true;
        crawlerHasWorkCv.notify_all();
        scannerHasWorkCv.notify_all();
    }
    crawlThread.join();
    for (size_t i = 0; i != scanThreads.size(); i++)
    {
        scanThreads[i].join();
    }
}

void Finder::setDirectory(const QString &directory)
{
    std::lock_guard<std::mutex> paramsLg(paramsM);
    params.done = false;
    params.directory = directory;
    params.invalid = params.directory.isEmpty() || params.pattern.isEmpty();

    cancel.store(true);
    crawlerHasWorkCv.notify_all();
}

void Finder::setPattern(const QString &pattern)
{
    std::lock_guard<std::mutex> paramsLg(paramsM);
    params.done = false;
    params.pattern = pattern;
    params.invalid = params.directory.isEmpty() || params.pattern.isEmpty();

    cancel.store(true);
    crawlerHasWorkCv.notify_all();
}

void Finder::stop() {
    std::lock_guard<std::mutex> queueLg(queueM);

    statusCode.store(3);
    cancel.store(true);
    for (size_t i = 0; i < scanCancel.size(); i++) {
        scanCancel[i].store(true);
    }

    fileQueue = FileQueue();
}

void Finder::crawl(const QString &directory)
{
    QDirIterator dirIt(directory,
                       QDir::Dirs | QDir::NoSymLinks | QDir::NoDotAndDotDot | QDir::Hidden,
                       QDirIterator::Subdirectories);

    QDir current(directory);

    do {
        if (cancel.load()) {
            return;
        }

        current.setFilter(QDir::Files | QDir::Hidden | QDir::NoSymLinks);

        auto files = current.entryInfoList();

        for (auto file : files) {
            if (cancel.load()) {
                return;
            }

            if (!file.permission(QFile::ReadUser)) {
                continue;
            }

            enqueFileToScan(file.absoluteFilePath());
        }
    } while (dirIt.hasNext() && (current = QDir(dirIt.next())).exists());
}

void Finder::enqueFileToScan(const QString &filePath)
{
    std::lock_guard<std::mutex> queueLg(queueM);

    totalCount++;
    totalSize += QFile(filePath).size();

    fileQueue.push(filePath);
    scannerHasWorkCv.notify_one();
}


void Finder::scan(const QString &filePath, const QString &pattern, std::atomic<bool> &cancel)
{
    QFile fileObj(filePath);
    if (!fileObj.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return;
    }
    QTextStream in(&fileObj);

    Automaton a = Automaton::fromString(pattern);
    std::deque<QChar> que;

    while (!in.atEnd()) {
        if (cancel.load() || maxEntryListSize <= entryCount) {
            break;
        }

        QString block(in.read(Finder::readingBlockSize));
        for (QChar &ch : block) {
            if (cancel.load() || maxEntryListSize <= entryCount) {
                break;
            }

            que.push_back(ch);
            a.step(ch, true);

            if (isUnsupportedChar(ch) || isLineSeperator(ch)) {
                que.clear();
                continue;
            }

            if (a.isTerminal()) {
                std::lock_guard<std::mutex> resultLg(resultM);
                entryCount++;

                QString before;
                for (size_t i = 0; i < que.size() - pattern.size(); i++) {
                    before.append(que[i]);
                }

                result.list.push_back({filePath, before, pattern, "after"});
            }

            if (que.size() == Entry::maxBeforeSize + pattern.size()) {
                que.pop_front();
            }
        }
    }

    if (cancel.load()) {
        return;
    }

    scannedCount++;
    scannedSize += QFile(filePath).size();

    if (maxEntryListSize <= entryCount.load()) {
        statusCode.store(4);
    }
}

Finder::EntryList Finder::getResult()
{
    std::unique_lock<std::mutex> resultLg(resultM, std::try_to_lock);
    if (!resultLg.owns_lock()) {
        EntryList tmp;
        tmp.first = false;
        return tmp;
    }

    EntryList tmp(std::move(result));
    result.first = false;
    result.list.clear();
    return tmp;
}

int Finder::getStatus() const
{
    return statusCode.load();
}

Finder::Metrics Finder::getMetrics() const
{
    return {scannedCount.load(), scannedSize.load(), totalCount.load(), totalSize.load()};
}

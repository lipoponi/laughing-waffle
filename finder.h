#ifndef FINDER_H
#define FINDER_H

#include <array>
#include <atomic>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QObject>
#include <QTextStream>

#include "automaton.h"
#include "helpers.h"

class Finder : public QObject
{
    Q_OBJECT
public:
    struct Entry
    {
        static const size_t maxBeforeSize = 16;
        static const size_t maxAfterChars = 16;

        uint64_t line;
        QString filePath;
        QString before;
        QString entry;
        QString after;
    };

    struct EntryList
    {
        EntryList();

        bool first;
        std::vector<Entry> list;
    };

    struct Metrics
    {
        uint64_t scannedCount;
        uint64_t scannedSize;
        uint64_t totalCount;
        uint64_t totalSize;
    };

private:
    struct Params
    {
        Params();

        bool invalid;
        bool done;
        QString directory;
        QString pattern;
    };

    struct FileSizeCmp
    {
        bool operator()(const QString &lhs, const QString &rhs);
    };

    typedef std::priority_queue<QString, std::vector<QString>, FileSizeCmp> FileQueue;

public:
    Finder();
    ~Finder();
    EntryList getResult();
    int getStatus() const;
    Metrics getMetrics() const;
    bool isCrawlingFinished() const;

private:
    void crawl(const QString &directory);
    void enqueFileToScan(const QString &filePath);
    void scan(const QString &filePath, const QString &pattern, std::atomic<bool> &cancel);

public slots:
    void setDirectory(const QString &directory);
    void setPattern(const QString &pattern);
    void stop();

private:
    static const int scanThreadsCount = 4;
    static const int readingBlockSize = 4096;

    std::atomic<int> entryCount;
    std::atomic<int> statusCode;
    std::atomic<bool> crawlFinished;
    std::atomic<uint64_t> scannedCount;
    std::atomic<uint64_t> scannedSize;
    std::atomic<uint64_t> totalCount;
    std::atomic<uint64_t> totalSize;

    Params params;
    EntryList result;
    FileQueue fileQueue;
    QString queuedPattern;
    bool quit;
    std::atomic<bool> cancel;
    std::array<std::atomic<bool>, scanThreadsCount> scanCancel;

    mutable std::mutex paramsM;
    mutable std::mutex queueM;
    mutable std::mutex resultM;
    std::condition_variable crawlerHasWorkCv;
    std::condition_variable scannerHasWorkCv;

    std::thread crawlThread;
    std::vector<std::thread> scanThreads;
};

#endif // FINDER_H

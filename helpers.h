#ifndef HELPERS_H
#define HELPERS_H

#include <QChar>
#include <QString>
#include <QStringListIterator>

QString humanizeSize(const uint64_t size);
bool isUnsupportedChar(const QChar &c);
bool isLineSeperator(const QChar &c);

#endif // HELPERS_H

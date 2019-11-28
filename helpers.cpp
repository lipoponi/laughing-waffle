#include "helpers.h"

QString humanizeSize(uint64_t size)
{
    float num = size;
    QStringList list = { "KB", "MB", "GB", "TB" };

    QStringListIterator i(list);
    QString unit("B");

    while(1024 <= num && i.hasNext()) {
        unit = i.next();
        num /= 1024;
    }

    return QString().setNum(num, 'f', 1) + unit;
}

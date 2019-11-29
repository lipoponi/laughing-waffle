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

bool isUnsupportedChar(const QChar &c)
{
    return !c.isPrint() || c.isNonCharacter();
}

bool isLineSeperator(const QChar &c)
{
    return c.category() == QChar::Separator_Line || c.category() == QChar::Separator_Paragraph;
}

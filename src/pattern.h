#pragma once

#include "multifilteroutput_global.h"

#include <QColor>
#include <QString>
#include <QVariantMap>

namespace MultiFilterOutput {

class MULTIFILTEROUTPUT_EXPORT Pattern
{
public:
    QString id;
    QString name;
    QString patternText;
    bool isRegex = true;
    bool caseSensitive = false;
    bool invert = false;
    QColor color = QColor(Qt::gray);

    static Pattern createNew();
    QVariantMap toMap() const;
    static Pattern fromMap(const QVariantMap &map);
};

} // namespace MultiFilterOutput

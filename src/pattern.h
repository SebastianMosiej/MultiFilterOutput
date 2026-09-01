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
    // When set, a line that doesn't itself match patternText is still swept into this
    // pattern's tab -- without counting as a match -- as long as it matches
    // continuationPattern (always a regex), starting right after a line that did match.
    // Sweeping stops at the first line that doesn't match continuationPattern.
    bool groupContinuationLines = false;
    QString continuationPattern = QStringLiteral("^ {1,3}");

    static Pattern createNew();
    QVariantMap toMap() const;
    static Pattern fromMap(const QVariantMap &map);
};

} // namespace MultiFilterOutput

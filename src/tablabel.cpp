#include "tablabel.h"

namespace MultiFilterOutput {

QString tabLabel(const Pattern &pattern, int matchCount)
{
    const QString name = pattern.invert ? QStringLiteral("Not: %1").arg(pattern.name)
                                        : pattern.name;
    return QStringLiteral("%1 (%2)").arg(name).arg(matchCount);
}

} // namespace MultiFilterOutput

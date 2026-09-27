#include "tablabel.h"

namespace MultiFilterOutput {

QString tabLabel(const Pattern &pattern, int matchCount, bool tooSlow)
{
    const QString name = pattern.invert ? QStringLiteral("Not: %1").arg(pattern.name)
                                        : pattern.name;
    const QString label = QStringLiteral("%1 (%2)").arg(name).arg(matchCount);
    return tooSlow ? QStringLiteral("%1 - disabled: too slow").arg(label) : label;
}

} // namespace MultiFilterOutput

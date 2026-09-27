#include "tablabel.h"

#include "multifilteroutputtr.h"

namespace MultiFilterOutput {

QString tabLabel(const Pattern &pattern, int matchCount)
{
    const QString name = pattern.invert ? QStringLiteral("Not: %1").arg(pattern.name)
                                        : pattern.name;
    return QStringLiteral("%1 (%2)").arg(name).arg(matchCount);
}

QString runLabel(const QString &runName, qint64 pid)
{
    if (pid <= 0)
        return Tr::tr("Filtering: %1").arg(runName);
    return Tr::tr("Filtering: %1 (pid %2)").arg(runName).arg(pid);
}

} // namespace MultiFilterOutput

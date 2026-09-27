#include "tablabel.h"

#include "multifilteroutputtr.h"

#include <QTabBar>

namespace MultiFilterOutput {

QString tabLabel(const Pattern &pattern, int matchCount, bool tooSlow)
{
    const QString name = pattern.invert ? QStringLiteral("Not: %1").arg(pattern.name)
                                        : pattern.name;
    // One multi-arg arg(), never chained: a chained .arg() would also substitute into a "%1"
    // inside the name the first one inserted.
    const QString label = QStringLiteral("%1 (%2)").arg(name, QString::number(matchCount));
    return tooSlow ? QStringLiteral("%1 - disabled: too slow").arg(label) : label;
}

QString runLabel(const QString &runName, qint64 pid)
{
    if (pid <= 0)
        return Tr::tr("Filtering: %1").arg(runName);
    return Tr::tr("Filtering: %1 (pid %2)").arg(runName, QString::number(pid));
}

void applyTabLabel(
    QTabBar *tabBar, int index, const Pattern &pattern, int matchCount, bool tooSlow)
{
    tabBar->setTabText(index, tabLabel(pattern, matchCount, tooSlow));
    // Pattern::color is user-editable and persisted; this is where it becomes visible. Set
    // unconditionally: an invalid color is how OutputPaneTabBar knows to fall back to its
    // default text color, so skipping it would leave any previous tint in place.
    tabBar->setTabTextColor(index, pattern.color);
}

} // namespace MultiFilterOutput

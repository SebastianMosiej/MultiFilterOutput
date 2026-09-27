#include "tablabel.h"

#include <QTabBar>

namespace MultiFilterOutput {

QString tabLabel(const Pattern &pattern, int matchCount)
{
    const QString name = pattern.invert ? QStringLiteral("Not: %1").arg(pattern.name)
                                        : pattern.name;
    return QStringLiteral("%1 (%2)").arg(name).arg(matchCount);
}

void applyTabLabel(QTabBar *tabBar, int index, const Pattern &pattern, int matchCount)
{
    tabBar->setTabText(index, tabLabel(pattern, matchCount));
    // Pattern::color is user-editable and persisted; this is where it becomes visible. Set
    // unconditionally: an invalid color is how OutputPaneTabBar knows to fall back to its
    // default text color, so skipping it would leave any previous tint in place.
    tabBar->setTabTextColor(index, pattern.color);
}

} // namespace MultiFilterOutput

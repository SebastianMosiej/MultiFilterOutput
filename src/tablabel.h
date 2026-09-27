#pragma once

#include "multifilteroutput_global.h"
#include "pattern.h"

#include <QString>

QT_BEGIN_NAMESPACE
class QTabBar;
QT_END_NAMESPACE

namespace MultiFilterOutput {

// tooSlow: the pattern's SlowMatchBreaker has tripped, so it is no longer being evaluated for
// this run -- the label says so, since the count has stopped for a reason the user can't see.
MULTIFILTEROUTPUT_EXPORT QString tabLabel(
    const Pattern &pattern, int matchCount, bool tooSlow = false);

// The pane's toolbar label for the run being filtered. pid <= 0 means the pid isn't known
// (yet, or ever -- e.g. a run on a remote device), and is left out rather than shown as 0.
MULTIFILTEROUTPUT_EXPORT QString runLabel(const QString &runName, qint64 pid);

// Sets tab `index` of `tabBar` to tabLabel(pattern, matchCount, tooSlow), tinted with
// pattern.color -- or, when that is invalid ("no tint"), back to the tab bar's default text
// color, so a tint from an earlier pattern state never lingers.
MULTIFILTEROUTPUT_EXPORT void applyTabLabel(
    QTabBar *tabBar, int index, const Pattern &pattern, int matchCount, bool tooSlow = false);

} // namespace MultiFilterOutput

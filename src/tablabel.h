#pragma once

#include "multifilteroutput_global.h"
#include "pattern.h"

#include <QString>

QT_BEGIN_NAMESPACE
class QTabBar;
QT_END_NAMESPACE

namespace MultiFilterOutput {

MULTIFILTEROUTPUT_EXPORT QString tabLabel(const Pattern &pattern, int matchCount);

// Sets tab `index` of `tabBar` to tabLabel(pattern, matchCount), tinted with pattern.color --
// or, when that is invalid ("no tint"), back to the tab bar's default text color, so a tint
// from an earlier pattern state never lingers.
MULTIFILTEROUTPUT_EXPORT void applyTabLabel(
    QTabBar *tabBar, int index, const Pattern &pattern, int matchCount);

} // namespace MultiFilterOutput

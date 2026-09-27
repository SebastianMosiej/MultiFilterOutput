#pragma once

#include "multifilteroutput_global.h"
#include "pattern.h"

#include <QString>

namespace MultiFilterOutput {

// tooSlow: the pattern's SlowMatchBreaker has tripped, so it is no longer being evaluated for
// this run -- the label says so, since the count has stopped for a reason the user can't see.
MULTIFILTEROUTPUT_EXPORT QString tabLabel(
    const Pattern &pattern, int matchCount, bool tooSlow = false);

} // namespace MultiFilterOutput

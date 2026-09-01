#pragma once

#include "multifilteroutput_global.h"

#include <QString>

namespace MultiFilterOutput {

class Pattern;

MULTIFILTEROUTPUT_EXPORT bool lineMatches(const Pattern &pattern, const QString &line);

// True if `line` should be swept into pattern's tab as a continuation of a previous match,
// per pattern.groupContinuationLines / pattern.continuationPattern. Never true on its own --
// only meaningful for a line encountered while already "inside" a previous match's run of
// continuation lines.
MULTIFILTEROUTPUT_EXPORT bool lineContinuesPattern(const Pattern &pattern, const QString &line);

} // namespace MultiFilterOutput

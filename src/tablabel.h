#pragma once

#include "multifilteroutput_global.h"
#include "pattern.h"

#include <QString>

namespace MultiFilterOutput {

MULTIFILTEROUTPUT_EXPORT QString tabLabel(const Pattern &pattern, int matchCount);

// The pane's toolbar label for the run being filtered. pid <= 0 means the pid isn't known
// (yet, or ever -- e.g. a run on a remote device), and is left out rather than shown as 0.
MULTIFILTEROUTPUT_EXPORT QString runLabel(const QString &runName, qint64 pid);

} // namespace MultiFilterOutput

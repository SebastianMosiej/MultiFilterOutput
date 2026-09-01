#pragma once

#include "multifilteroutput_global.h"
#include "pattern.h"

#include <QString>

namespace MultiFilterOutput {

MULTIFILTEROUTPUT_EXPORT QString tabLabel(const Pattern &pattern, int matchCount);

} // namespace MultiFilterOutput

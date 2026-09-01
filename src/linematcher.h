#pragma once

#include "multifilteroutput_global.h"

#include <QString>

namespace MultiFilterOutput {

class Pattern;

MULTIFILTEROUTPUT_EXPORT bool lineMatches(const Pattern &pattern, const QString &line);

} // namespace MultiFilterOutput

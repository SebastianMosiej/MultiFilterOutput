#pragma once

#include "multifilteroutput_global.h"
#include "pattern.h"

#include <QList>

namespace MultiFilterOutput {

// A ready-made set for a fresh Pattern Library: errors, warnings, QML messages, assertions and
// Qt Test failures. Fixed ids (so adding them again never duplicates one) and the tag
// "starter" (so a run configuration can switch them all at once, see Pattern::tags). None is
// on by default: they are offered, not imposed.
MULTIFILTEROUTPUT_EXPORT QList<Pattern> starterPatterns();

// `library` plus the starter patterns it doesn't have yet (by id), at the end. One the user
// has since edited is left as they have it.
MULTIFILTEROUTPUT_EXPORT QList<Pattern> withStarterPatterns(const QList<Pattern> &library);

} // namespace MultiFilterOutput

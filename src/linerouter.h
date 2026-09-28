#pragma once

#include "linematcher.h"
#include "multifilteroutput_global.h"
#include "pattern.h"

#include <QList>
#include <QString>

#include <chrono>

namespace MultiFilterOutput {

// Decides, line by line, which of a run's pattern tabs each line of output belongs in -- the
// part of FilteredOutputPane that has rules, kept free of widgets so it can be tested on its
// own. Holds the per-pattern state that spans lines: whether a continuation run (see
// Pattern::groupContinuationLines) is in progress, and each pattern's SlowMatchBreaker.
class MULTIFILTEROUTPUT_EXPORT LineRouter
{
public:
    struct Route
    {
        int patternIndex;
        // False for a continuation line swept in after a match: shown, but not counted.
        bool countsAsMatch;
    };

    struct Result
    {
        // In pattern order.
        QList<Route> routes;
        // Patterns whose breaker tripped while routing this line -- each is reported here
        // exactly once, and skipped from then on.
        QList<int> newlyTooSlow;
        // No non-inverted pattern took the line (as a match or a continuation line): it
        // belongs in the Unmatched tab. Inverted patterns don't count -- they take nearly
        // everything by design.
        bool unmatched = true;
    };

    explicit LineRouter(
        const QList<Pattern> &patterns = {},
        std::chrono::milliseconds slowThreshold = std::chrono::milliseconds(10));

    // `format` says which channel the line came from (see Pattern::channels).
    Result route(const QString &line, Utils::OutputFormat format);

    bool isTooSlow(int patternIndex) const;

    // Ends every continuation run in progress, e.g. when the pane's contents are cleared.
    // Tripped breakers stay tripped: the pattern is no faster for it.
    void resetContinuations();

private:
    struct PatternState
    {
        Pattern pattern;
        bool inContinuation = false;
        SlowMatchBreaker breaker;
    };

    // One pattern's verdict for one line (continuation first, then the pattern itself);
    // appends to `routes` if the line belongs in its tab.
    void routeOne(
        int index, const QString &line, Utils::OutputFormat format, QList<Route> &routes);

    QList<PatternState> m_states;
};

} // namespace MultiFilterOutput

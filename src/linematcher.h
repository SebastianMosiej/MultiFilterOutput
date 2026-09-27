#pragma once

#include "multifilteroutput_global.h"

#include <QHash>
#include <QRegularExpression>
#include <QString>

#include <chrono>
#include <functional>

namespace MultiFilterOutput {

class Pattern;

// Compiled regular expressions, keyed by the two things that affect compilation: the pattern
// text and case sensitivity. Bounded: when a new entry would exceed the capacity, the cache
// starts over empty. The working set (at most two regexes per enabled pattern) is far below
// the default capacity, so this only ever discards entries left behind by edited or deleted
// patterns -- which an unbounded cache would keep for the rest of the session.
class MULTIFILTEROUTPUT_EXPORT RegexCache
{
public:
    explicit RegexCache(qsizetype capacity = 64);

    // The returned reference is valid until the next get().
    const QRegularExpression &get(const QString &patternText, bool caseSensitive);
    qsizetype size() const;

private:
    qsizetype m_capacity;
    QHash<QString, QRegularExpression> m_cache;
};

MULTIFILTEROUTPUT_EXPORT bool lineMatches(const Pattern &pattern, const QString &line);

// True if `line` should be swept into pattern's tab as a continuation of a previous match,
// per pattern.groupContinuationLines / pattern.continuationPattern. Never true on its own --
// only meaningful for a line encountered while already "inside" a previous match's run of
// continuation lines.
MULTIFILTEROUTPUT_EXPORT bool lineContinuesPattern(const Pattern &pattern, const QString &line);

// Circuit breaker for one pattern's matching during one run. A regex with catastrophic
// backtracking can't hang the GUI thread outright -- PCRE2's match limit makes it give up
// after some tens of ms -- but paying that on every line of a chatty app still grinds Qt
// Creator down. Once a pattern has proven itself slow a few times, the breaker trips and the
// pattern stops being evaluated at all.
class MULTIFILTEROUTPUT_EXPORT SlowMatchBreaker
{
public:
    explicit SlowMatchBreaker(
        std::chrono::milliseconds slowThreshold = std::chrono::milliseconds(10),
        int maxStrikes = 3);

    // Runs `match` and returns its result -- or, once tripped, returns false without running
    // it. A run slower than the threshold is repeated once to rule out a one-off stall
    // (preemption, a page fault): only when the repeat is slow too does it count as a strike.
    bool run(const std::function<bool()> &match);

    bool isTripped() const;
    void reset();

private:
    std::chrono::milliseconds m_slowThreshold;
    int m_maxStrikes;
    int m_strikes = 0;
};

} // namespace MultiFilterOutput

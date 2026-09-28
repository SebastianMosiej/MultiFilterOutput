#include "linerouter.h"

namespace MultiFilterOutput {

LineRouter::LineRouter(const QList<Pattern> &patterns, std::chrono::milliseconds slowThreshold)
{
    m_states.reserve(patterns.size());
    for (const Pattern &pattern : patterns)
        m_states.append({pattern, false, SlowMatchBreaker(slowThreshold)});
}

LineRouter::Result LineRouter::route(const QString &line, Utils::OutputFormat format, int lineTag)
{
    Result result;
    for (int i = 0; i < m_states.size(); ++i) {
        if (m_states.at(i).breaker.isTripped())
            continue;
        routeOne(i, line, format, lineTag, result.routes);
        if (m_states.at(i).breaker.isTripped())
            result.newlyTooSlow.append(i);
    }
    for (const Route &route : std::as_const(result.routes)) {
        if (!route.isContext && !m_states.at(route.patternIndex).pattern.invert) {
            result.unmatched = false;
            break;
        }
    }
    return result;
}

void LineRouter::routeOne(
    int index, const QString &line, Utils::OutputFormat format, int lineTag, QList<Route> &routes)
{
    PatternState &state = m_states[index];
    // Another channel's line is invisible to this pattern -- it doesn't even end a
    // continuation run, so interleaved stdout can't cut a stderr stack trace short.
    if (!state.pattern.looksAt(format))
        return;
    // Both regexes -- the main one and the continuation one -- go through the breaker; either
    // can be the pathological one.
    if (state.inContinuation) {
        if (state.breaker.run([&] { return lineContinuesPattern(state.pattern, line); })) {
            routes.append({index, /*countsAsMatch=*/false});
            // "N lines after the match": the continuation run counts toward them.
            state.afterContextLeft = qMax(0, state.afterContextLeft - 1);
            state.unshownTags.clear();
            return;
        }
        // This line breaks the continuation run; fall through and re-check it as an
        // ordinary candidate match below, same as any other line.
        state.inContinuation = false;
    }
    if (state.breaker.run([&] { return lineMatches(state.pattern, line); })) {
        routes.append({index, /*countsAsMatch=*/true, state.unshownTags});
        state.unshownTags.clear();
        state.inContinuation = state.pattern.groupContinuationLines
                               && !state.pattern.continuationPattern.isEmpty();
        state.afterContextLeft = state.pattern.contextAfter;
        return;
    }
    if (state.afterContextLeft > 0) {
        --state.afterContextLeft;
        routes.append({index, /*countsAsMatch=*/false, {}, /*isContext=*/true});
        return;
    }
    // Not shown: a candidate for the next match's before-context. (Lines shown in the tab never
    // get here, so context can't repeat them.)
    if (state.pattern.contextBefore > 0 && lineTag >= 0) {
        state.unshownTags.append(lineTag);
        if (state.unshownTags.size() > state.pattern.contextBefore)
            state.unshownTags.removeFirst();
    }
}

bool LineRouter::isTooSlow(int patternIndex) const
{
    return patternIndex >= 0 && patternIndex < m_states.size()
           && m_states.at(patternIndex).breaker.isTripped();
}

void LineRouter::resetContinuations()
{
    for (PatternState &state : m_states) {
        state.inContinuation = false;
        state.afterContextLeft = 0;
        state.unshownTags.clear();
    }
}

} // namespace MultiFilterOutput

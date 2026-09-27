#include "linematcher.h"

#include "pattern.h"

#include <QElapsedTimer>
#include <QHash>
#include <QRegularExpression>

namespace MultiFilterOutput {

RegexCache::RegexCache(qsizetype capacity)
    : m_capacity(capacity)
{}

const QRegularExpression &RegexCache::get(const QString &patternText, bool caseSensitive)
{
    // The key prefix keeps case-sensitive and case-insensitive variants of the same text
    // apart.
    const QString key = (caseSensitive ? QStringLiteral("1|") : QStringLiteral("0|")) + patternText;
    auto it = m_cache.find(key);
    if (it == m_cache.end()) {
        if (m_cache.size() >= m_capacity)
            m_cache.clear();
        QRegularExpression::PatternOptions options = QRegularExpression::NoPatternOption;
        if (!caseSensitive)
            options |= QRegularExpression::CaseInsensitiveOption;
        it = m_cache.insert(key, QRegularExpression(patternText, options));
    }
    return it.value();
}

qsizetype RegexCache::size() const
{
    return m_cache.size();
}

namespace {

// lineMatches() is called once per enabled pattern per output line, so compiling the PCRE
// from scratch every time is a real per-line cost with a chatty application and a handful
// of patterns. Matching only ever happens on the GUI thread, so a thread_local cache needs
// no locking.
const QRegularExpression &compiledRegex(const QString &patternText, bool caseSensitive)
{
    static thread_local RegexCache cache;
    return cache.get(patternText, caseSensitive);
}

} // namespace

bool lineMatches(const Pattern &pattern, const QString &line)
{
    bool matched = false;
    if (pattern.isRegex) {
        const QRegularExpression &re = compiledRegex(pattern.patternText, pattern.caseSensitive);
        // The Pattern Library editor refuses to save an uncompilable regex, but a
        // hand-edited QtCreator.ini can still produce one. QRegularExpression::match()
        // would then warn on every single output line, so fail safe instead: treat an
        // invalid pattern as one that never matches, which is exactly what returning
        // pattern.invert (i.e. "false != invert") means.
        if (!re.isValid())
            return pattern.invert;
        matched = re.match(line).hasMatch();
    } else {
        const Qt::CaseSensitivity cs = pattern.caseSensitive ? Qt::CaseSensitive
                                                             : Qt::CaseInsensitive;
        matched = line.contains(pattern.patternText, cs);
    }
    return matched != pattern.invert;
}

SlowMatchBreaker::SlowMatchBreaker(std::chrono::milliseconds slowThreshold, int maxStrikes)
    : m_slowThreshold(slowThreshold)
    , m_maxStrikes(maxStrikes)
{}

bool SlowMatchBreaker::run(const std::function<bool()> &match)
{
    if (isTripped())
        return false;

    const auto timed = [&match](bool &result) {
        QElapsedTimer timer;
        timer.start();
        result = match();
        return timer.durationElapsed();
    };

    bool result = false;
    if (timed(result) > m_slowThreshold && timed(result) > m_slowThreshold)
        ++m_strikes;
    return result;
}

bool SlowMatchBreaker::isTripped() const
{
    return m_strikes >= m_maxStrikes;
}

void SlowMatchBreaker::reset()
{
    m_strikes = 0;
}

bool lineContinuesPattern(const Pattern &pattern, const QString &line)
{
    if (!pattern.groupContinuationLines || pattern.continuationPattern.isEmpty())
        return false;

    // Always a regex, unlike the main pattern -- a "starts with" structural check has no
    // sensible plain-substring reading. An invalid regex (e.g. hand-edited settings) never
    // continues, same fail-safe as an invalid main pattern.
    const QRegularExpression &re = compiledRegex(pattern.continuationPattern, pattern.caseSensitive);
    if (!re.isValid())
        return false;
    return re.match(line).hasMatch();
}

} // namespace MultiFilterOutput

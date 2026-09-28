#include "../src/linerouter.h"
#include "../src/pattern.h"

#include <QtTest>

using namespace MultiFilterOutput;
using namespace std::chrono_literals;

using Routes = QList<LineRouter::Route>;

namespace {

Pattern regexPattern(const QString &text)
{
    Pattern p;
    p.isRegex = true;
    p.patternText = text;
    return p;
}

Pattern grouping(Pattern p, const QString &continuation = QStringLiteral("^ {1,3}"))
{
    p.groupContinuationLines = true;
    p.continuationPattern = continuation;
    return p;
}

} // namespace

namespace MultiFilterOutput {
// For QCOMPARE on Routes.
bool operator==(const LineRouter::Route &a, const LineRouter::Route &b)
{
    return a.patternIndex == b.patternIndex && a.countsAsMatch == b.countsAsMatch
           && a.contextBefore == b.contextBefore;
}
} // namespace MultiFilterOutput

class tst_LineRouter : public QObject
{
    Q_OBJECT

private slots:
    void routesLinesToEveryMatchingPattern();
    void invertedPatternTakesNonMatchingLines();
    void continuationLinesAreSweptInWithoutCounting();
    void lineEndingContinuationIsRecheckedAsCandidate();
    void continuationOfInvertedPattern();
    void continuationRunsAreIndependentPerPattern();
    void resetContinuationsEndsRun();
    void slowPatternIsReportedOnceThenSkipped();
    void unmatchedMeansNoNonInvertedPatternTookIt();
    void continuationLinesAreNotUnmatched();
    void patternOnlySeesItsChannels();
    void otherChannelsDontTouchContinuationRuns();
    void lineOnNoPatternsChannelIsUnmatched();
    void excludedLineStartsNoContinuationRun();
    void afterContextShowsTheNextLines();
    void afterContextRestartsOnEachMatch();
    void beforeContextHandsBackThePrecedingLines();
    void contextNeverRepeatsLinesAlreadyShown();
    void contextCountsOnlyThePatternsChannels();
    void continuationLinesUseUpAfterContext();
    void contextLinesDontTakeLinesFromUnmatched();
    void resetContinuationsForgetsContext();
};

void tst_LineRouter::routesLinesToEveryMatchingPattern()
{
    LineRouter router({regexPattern("ERROR"), regexPattern("net")});

    QCOMPARE(router.route("12:00 ERROR disk full", Utils::StdOutFormat).routes, (Routes{{0, true}}));
    QCOMPARE(router.route("net ERROR timeout", Utils::StdOutFormat).routes, (Routes{{0, true}, {1, true}}));
    QCOMPARE(router.route("all good", Utils::StdOutFormat).routes, Routes{});
}

void tst_LineRouter::invertedPatternTakesNonMatchingLines()
{
    Pattern notDebug = regexPattern("DEBUG");
    notDebug.invert = true;
    LineRouter router({notDebug});

    QCOMPARE(router.route("DEBUG noise", Utils::StdOutFormat).routes, Routes{});
    QCOMPARE(router.route("INFO signal", Utils::StdOutFormat).routes, (Routes{{0, true}}));
}

// An error followed by its indented detail block: the detail lines land in the tab right
// after the match, shown but not counted as matches of their own.
void tst_LineRouter::continuationLinesAreSweptInWithoutCounting()
{
    LineRouter router({grouping(regexPattern("ERROR"))});

    QCOMPARE(router.route("ERROR boom", Utils::StdOutFormat).routes, (Routes{{0, true}}));
    QCOMPARE(router.route("  at foo()", Utils::StdOutFormat).routes, (Routes{{0, false}}));
    QCOMPARE(router.route("  at bar()", Utils::StdOutFormat).routes, (Routes{{0, false}}));
    QCOMPARE(router.route("next unrelated line", Utils::StdOutFormat).routes, Routes{});
    QCOMPARE(router.route("  indented, but no longer after a match", Utils::StdOutFormat).routes, Routes{});
}

// The line that ends a continuation run is still a candidate in its own right.
void tst_LineRouter::lineEndingContinuationIsRecheckedAsCandidate()
{
    LineRouter router({grouping(regexPattern("ERROR"))});

    router.route("ERROR first", Utils::StdOutFormat);
    router.route("  detail", Utils::StdOutFormat);
    QCOMPARE(router.route("ERROR second", Utils::StdOutFormat).routes, (Routes{{0, true}}));
    QCOMPARE(router.route("  its detail", Utils::StdOutFormat).routes, (Routes{{0, false}}));
}

// Inverted + grouping: a line the pattern admits (one NOT matching its text) starts a run,
// and continuation lines are swept in even though the continuation check ignores the invert.
// A line the inverted pattern excludes ends the run and isn't admitted; an indented line
// after that is judged on its own again -- admitted as a match, since it doesn't match the
// excluded text either.
void tst_LineRouter::continuationOfInvertedPattern()
{
    Pattern notDebug = grouping(regexPattern("DEBUG"));
    notDebug.invert = true;
    LineRouter router({notDebug});

    QCOMPARE(router.route("INFO started", Utils::StdOutFormat).routes, (Routes{{0, true}}));
    QCOMPARE(router.route("  DEBUG-looking detail", Utils::StdOutFormat).routes, (Routes{{0, false}}));
    QCOMPARE(router.route("DEBUG noise", Utils::StdOutFormat).routes, Routes{});
    QCOMPARE(router.route("  detail after the noise", Utils::StdOutFormat).routes, (Routes{{0, true}}));
}

void tst_LineRouter::continuationRunsAreIndependentPerPattern()
{
    LineRouter router({grouping(regexPattern("ERROR")), regexPattern("foo")});

    QCOMPARE(router.route("ERROR x", Utils::StdOutFormat).routes, (Routes{{0, true}}));
    // Continues pattern 0's run and, separately, is a real match of pattern 1.
    QCOMPARE(router.route("  at foo()", Utils::StdOutFormat).routes, (Routes{{0, false}, {1, true}}));
}

// Used when the pane is cleared mid-run: the next indented line mustn't be swept into a
// match the user can no longer see.
void tst_LineRouter::resetContinuationsEndsRun()
{
    LineRouter router({grouping(regexPattern("ERROR"))});

    router.route("ERROR boom", Utils::StdOutFormat);
    router.resetContinuations();
    QCOMPARE(router.route("  at foo()", Utils::StdOutFormat).routes, Routes{});
}

// ^(\d+)*$ on 40 digits and a letter backtracks until PCRE2's match limit, tens of ms per
// line -- far over the 1 ms threshold used here, however fast the machine.
void tst_LineRouter::slowPatternIsReportedOnceThenSkipped()
{
    LineRouter router({regexPattern("^(\\d+)*$"), regexPattern("x$")}, 1ms);
    const QString line = QString(40, '1') + 'x';

    int trippedAfter = -1;
    for (int i = 0; i < 10 && trippedAfter < 0; ++i) {
        const LineRouter::Result result = router.route(line, Utils::StdOutFormat);
        QCOMPARE(result.routes, (Routes{{1, true}}));
        if (!result.newlyTooSlow.isEmpty()) {
            QCOMPARE(result.newlyTooSlow, QList<int>{0});
            trippedAfter = i;
        }
    }
    QVERIFY2(trippedAfter >= 0, "the slow pattern never tripped");
    QVERIFY(router.isTooSlow(0));
    QVERIFY(!router.isTooSlow(1));

    // From now on it is skipped outright and never reported again; the other pattern still
    // works.
    const LineRouter::Result after = router.route(line, Utils::StdOutFormat);
    QCOMPARE(after.routes, (Routes{{1, true}}));
    QVERIFY(after.newlyTooSlow.isEmpty());
}

// The Unmatched tab gets what no pattern accounts for. Inverted patterns ("everything but
// DEBUG") take nearly every line by design, so they don't count as accounting for one.
void tst_LineRouter::unmatchedMeansNoNonInvertedPatternTookIt()
{
    Pattern notDebug = regexPattern("DEBUG");
    notDebug.invert = true;
    LineRouter router({regexPattern("ERROR"), notDebug});

    QVERIFY(!router.route("ERROR boom", Utils::StdOutFormat).unmatched);
    QVERIFY(router.route("INFO started", Utils::StdOutFormat).unmatched); // only the inverted pattern took it
    QVERIFY(router.route("DEBUG noise", Utils::StdOutFormat).unmatched);  // nothing took it
}

// A continuation line belongs to the match it follows, so it isn't unmatched.
void tst_LineRouter::continuationLinesAreNotUnmatched()
{
    LineRouter router({grouping(regexPattern("ERROR"))});

    router.route("ERROR boom", Utils::StdOutFormat);
    QVERIFY(!router.route("  at foo()", Utils::StdOutFormat).unmatched);
    QVERIFY(router.route("next line", Utils::StdOutFormat).unmatched);
}

void tst_LineRouter::patternOnlySeesItsChannels()
{
    Pattern stderrErrors = regexPattern("ERROR");
    stderrErrors.channels = Pattern::StdErrChannel;
    LineRouter router({stderrErrors, regexPattern("ERROR")});

    QCOMPARE(router.route("ERROR on stdout", Utils::StdOutFormat).routes, (Routes{{1, true}}));
    QCOMPARE(router.route("ERROR on stderr", Utils::StdErrFormat).routes,
             (Routes{{0, true}, {1, true}}));
    QCOMPARE(router.route("ERROR from Creator", Utils::ErrorMessageFormat).routes,
             (Routes{{1, true}}));
}

// Interleaved stdout doesn't cut a stderr stack trace short, nor get swept into it.
void tst_LineRouter::otherChannelsDontTouchContinuationRuns()
{
    Pattern traces = grouping(regexPattern("ERROR"));
    traces.channels = Pattern::StdErrChannel;
    LineRouter router({traces});

    QCOMPARE(router.route("ERROR boom", Utils::StdErrFormat).routes, (Routes{{0, true}}));
    QCOMPARE(router.route("  indented stdout", Utils::StdOutFormat).routes, Routes{});
    QCOMPARE(router.route("unindented stdout", Utils::StdOutFormat).routes, Routes{});
    QCOMPARE(router.route("  at foo()", Utils::StdErrFormat).routes, (Routes{{0, false}}));
}

void tst_LineRouter::lineOnNoPatternsChannelIsUnmatched()
{
    Pattern stdoutAll = regexPattern(".");
    stdoutAll.channels = Pattern::StdOutChannel;
    LineRouter router({stdoutAll});

    QVERIFY(!router.route("from the app", Utils::StdOutFormat).unmatched);
    QVERIFY(router.route("from the app, on stderr", Utils::StdErrFormat).unmatched);
}

void tst_LineRouter::excludedLineStartsNoContinuationRun()
{
    Pattern errors = grouping(regexPattern("ERROR"));
    errors.excludeText = "flaky";
    LineRouter router({errors});

    const LineRouter::Result excluded = router.route("ERROR flaky test", Utils::StdOutFormat);
    QCOMPARE(excluded.routes, Routes{});
    QVERIFY(excluded.unmatched);
    QCOMPARE(router.route("  its trace", Utils::StdOutFormat).routes, Routes{});
}

static Pattern withContext(Pattern p, int before, int after)
{
    p.contextBefore = before;
    p.contextAfter = after;
    return p;
}

// Feeds lines tagged 1, 2, 3, ... and returns each one's routes.
static QList<Routes> feed(LineRouter &router, const QStringList &lines, int firstTag = 1)
{
    QList<Routes> result;
    int tag = firstTag;
    for (const QString &line : lines)
        result << router.route(line, Utils::StdOutFormat, tag++).routes;
    return result;
}

void tst_LineRouter::afterContextShowsTheNextLines()
{
    LineRouter router({withContext(regexPattern("ERROR"), 0, 2)});
    QCOMPARE(
        feed(router, {"ERROR x", "a", "b", "c"}),
        (QList<Routes>{{{0, true}}, {{0, false}}, {{0, false}}, {}}));
}

void tst_LineRouter::afterContextRestartsOnEachMatch()
{
    LineRouter router({withContext(regexPattern("ERROR"), 0, 1)});
    QCOMPARE(
        feed(router, {"ERROR 1", "ERROR 2", "a", "b"}),
        (QList<Routes>{{{0, true}}, {{0, true}}, {{0, false}}, {}}));
}

void tst_LineRouter::beforeContextHandsBackThePrecedingLines()
{
    LineRouter router({withContext(regexPattern("ERROR"), 2, 0)});
    const QList<Routes> routes = feed(router, {"a", "b", "c", "ERROR x", "d"});
    QCOMPARE(routes.at(3), (Routes{{0, true, {2, 3}}}));
    QCOMPARE(routes.at(4), Routes{});
}

// grep's joined-up context: what the tab already shows -- a match, after-context -- is never
// handed back as before-context of the next match.
void tst_LineRouter::contextNeverRepeatsLinesAlreadyShown()
{
    LineRouter router({withContext(regexPattern("ERROR"), 3, 1)});
    const QList<Routes> routes = feed(router, {"ERROR 1", "after", "gap", "ERROR 2"});
    QCOMPARE(routes.at(1), (Routes{{0, false}}));
    QCOMPARE(routes.at(3), (Routes{{0, true, {3}}}));
}

void tst_LineRouter::contextCountsOnlyThePatternsChannels()
{
    Pattern errors = withContext(regexPattern("ERROR"), 1, 1);
    errors.channels = Pattern::StdErrChannel;
    LineRouter router({errors});

    router.route("before, on stderr", Utils::StdErrFormat, 1);
    router.route("stdout noise", Utils::StdOutFormat, 2);
    QCOMPARE(router.route("ERROR", Utils::StdErrFormat, 3).routes, (Routes{{0, true, {1}}}));
    QCOMPARE(router.route("stdout noise", Utils::StdOutFormat, 4).routes, Routes{});
    QCOMPARE(router.route("after, on stderr", Utils::StdErrFormat, 5).routes, (Routes{{0, false}}));
}

// "N lines after the match", like grep: a continuation run (which carries on regardless)
// counts toward them.
void tst_LineRouter::continuationLinesUseUpAfterContext()
{
    LineRouter router({withContext(grouping(regexPattern("ERROR")), 0, 1)});
    QCOMPARE(
        feed(router, {"ERROR x", "  at foo()", "  at bar()", "next"}),
        (QList<Routes>{{{0, true}}, {{0, false}}, {{0, false}}, {}}));
}

// Unmatched is for lines no pattern picked out; context around another pattern's match
// wasn't picked out (and before-context can't be taken back from Unmatched anyway).
void tst_LineRouter::contextLinesDontTakeLinesFromUnmatched()
{
    LineRouter router({withContext(regexPattern("ERROR"), 0, 1)});
    router.route("ERROR x", Utils::StdOutFormat, 1);
    const LineRouter::Result context = router.route("a", Utils::StdOutFormat, 2);
    QCOMPARE(context.routes, (Routes{{0, false}}));
    QVERIFY(context.unmatched);
}

void tst_LineRouter::resetContinuationsForgetsContext()
{
    LineRouter router({withContext(regexPattern("ERROR"), 1, 1)});
    feed(router, {"a", "ERROR x"});
    router.resetContinuations();
    QCOMPARE(feed(router, {"b"}, 3), (QList<Routes>{{}}));
    router.route("c", Utils::StdOutFormat, 4);
    router.resetContinuations();
    QCOMPARE(router.route("ERROR y", Utils::StdOutFormat, 5).routes, (Routes{{0, true}}));
}

QTEST_GUILESS_MAIN(tst_LineRouter)

#include "tst_linerouter.moc"

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
    return a.patternIndex == b.patternIndex && a.countsAsMatch == b.countsAsMatch;
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
};

void tst_LineRouter::routesLinesToEveryMatchingPattern()
{
    LineRouter router({regexPattern("ERROR"), regexPattern("net")});

    QCOMPARE(router.route("12:00 ERROR disk full").routes, (Routes{{0, true}}));
    QCOMPARE(router.route("net ERROR timeout").routes, (Routes{{0, true}, {1, true}}));
    QCOMPARE(router.route("all good").routes, Routes{});
}

void tst_LineRouter::invertedPatternTakesNonMatchingLines()
{
    Pattern notDebug = regexPattern("DEBUG");
    notDebug.invert = true;
    LineRouter router({notDebug});

    QCOMPARE(router.route("DEBUG noise").routes, Routes{});
    QCOMPARE(router.route("INFO signal").routes, (Routes{{0, true}}));
}

// An error followed by its indented detail block: the detail lines land in the tab right
// after the match, shown but not counted as matches of their own.
void tst_LineRouter::continuationLinesAreSweptInWithoutCounting()
{
    LineRouter router({grouping(regexPattern("ERROR"))});

    QCOMPARE(router.route("ERROR boom").routes, (Routes{{0, true}}));
    QCOMPARE(router.route("  at foo()").routes, (Routes{{0, false}}));
    QCOMPARE(router.route("  at bar()").routes, (Routes{{0, false}}));
    QCOMPARE(router.route("next unrelated line").routes, Routes{});
    QCOMPARE(router.route("  indented, but no longer after a match").routes, Routes{});
}

// The line that ends a continuation run is still a candidate in its own right.
void tst_LineRouter::lineEndingContinuationIsRecheckedAsCandidate()
{
    LineRouter router({grouping(regexPattern("ERROR"))});

    router.route("ERROR first");
    router.route("  detail");
    QCOMPARE(router.route("ERROR second").routes, (Routes{{0, true}}));
    QCOMPARE(router.route("  its detail").routes, (Routes{{0, false}}));
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

    QCOMPARE(router.route("INFO started").routes, (Routes{{0, true}}));
    QCOMPARE(router.route("  DEBUG-looking detail").routes, (Routes{{0, false}}));
    QCOMPARE(router.route("DEBUG noise").routes, Routes{});
    QCOMPARE(router.route("  detail after the noise").routes, (Routes{{0, true}}));
}

void tst_LineRouter::continuationRunsAreIndependentPerPattern()
{
    LineRouter router({grouping(regexPattern("ERROR")), regexPattern("foo")});

    QCOMPARE(router.route("ERROR x").routes, (Routes{{0, true}}));
    // Continues pattern 0's run and, separately, is a real match of pattern 1.
    QCOMPARE(router.route("  at foo()").routes, (Routes{{0, false}, {1, true}}));
}

// Used when the pane is cleared mid-run: the next indented line mustn't be swept into a
// match the user can no longer see.
void tst_LineRouter::resetContinuationsEndsRun()
{
    LineRouter router({grouping(regexPattern("ERROR"))});

    router.route("ERROR boom");
    router.resetContinuations();
    QCOMPARE(router.route("  at foo()").routes, Routes{});
}

// ^(\d+)*$ on 40 digits and a letter backtracks until PCRE2's match limit, tens of ms per
// line -- far over the 1 ms threshold used here, however fast the machine.
void tst_LineRouter::slowPatternIsReportedOnceThenSkipped()
{
    LineRouter router({regexPattern("^(\\d+)*$"), regexPattern("x$")}, 1ms);
    const QString line = QString(40, '1') + 'x';

    int trippedAfter = -1;
    for (int i = 0; i < 10 && trippedAfter < 0; ++i) {
        const LineRouter::Result result = router.route(line);
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
    const LineRouter::Result after = router.route(line);
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

    QVERIFY(!router.route("ERROR boom").unmatched);
    QVERIFY(router.route("INFO started").unmatched); // only the inverted pattern took it
    QVERIFY(router.route("DEBUG noise").unmatched);  // nothing took it
}

// A continuation line belongs to the match it follows, so it isn't unmatched.
void tst_LineRouter::continuationLinesAreNotUnmatched()
{
    LineRouter router({grouping(regexPattern("ERROR"))});

    router.route("ERROR boom");
    QVERIFY(!router.route("  at foo()").unmatched);
    QVERIFY(router.route("next line").unmatched);
}

QTEST_GUILESS_MAIN(tst_LineRouter)

#include "tst_linerouter.moc"

#include "../src/linematcher.h"
#include "../src/pattern.h"

#include <QtTest>

#include <thread>

using namespace std::chrono_literals;

using namespace MultiFilterOutput;

class tst_LineMatcher : public QObject
{
    Q_OBJECT

private slots:
    void plainTextMatchCaseInsensitiveByDefault();
    void plainTextRespectsCaseSensitivity();
    void regexMatches();
    void invertFlipsResult();
    void invalidRegexFailsSafe();
    void repeatedCallsStayConsistent();
    void continuationOffByDefault();
    void continuationMatchesLeadingWhitespace();
    void continuationInvalidRegexFailsSafe();
    void catastrophicRegexStaysBounded_data();
    void catastrophicRegexStaysBounded();
    void breakerPassesResultsThroughWhileFast();
    void breakerTripsAfterRepeatedSlowMatches();
    void breakerForgivesOneOffStall();
    void breakerResetRearms();
};

void tst_LineMatcher::plainTextMatchCaseInsensitiveByDefault()
{
    Pattern p;
    p.isRegex = false;
    p.caseSensitive = false;
    p.patternText = "error";

    QVERIFY(lineMatches(p, "12:00 ERROR connection refused"));
    QVERIFY(!lineMatches(p, "12:00 INFO connected"));
}

void tst_LineMatcher::plainTextRespectsCaseSensitivity()
{
    Pattern p;
    p.isRegex = false;
    p.caseSensitive = true;
    p.patternText = "ERROR";

    QVERIFY(lineMatches(p, "12:00 ERROR connection refused"));
    QVERIFY(!lineMatches(p, "12:00 error connection refused"));
}

void tst_LineMatcher::regexMatches()
{
    Pattern p;
    p.isRegex = true;
    p.caseSensitive = false;
    p.patternText = "^\\d{2}:\\d{2}:\\d{2} ERROR";

    QVERIFY(lineMatches(p, "12:03:41 ERROR connection refused"));
    QVERIFY(!lineMatches(p, "connection refused at 12:03:41 ERROR"));
}

void tst_LineMatcher::invertFlipsResult()
{
    Pattern p;
    p.isRegex = false;
    p.caseSensitive = false;
    p.patternText = "debug";
    p.invert = true;

    QVERIFY(lineMatches(p, "12:00 INFO started"));
    QVERIFY(!lineMatches(p, "12:00 DEBUG verbose detail"));
}

void tst_LineMatcher::invalidRegexFailsSafe()
{
    // A hand-edited QtCreator.ini can carry a pattern QRegularExpression cannot compile.
    // Matching must not warn or crash, and must behave as "this pattern never matches".
    Pattern p;
    p.isRegex = true;
    p.patternText = "([unterminated";

    QVERIFY(!lineMatches(p, "anything at all"));
    QVERIFY(!lineMatches(p, "([unterminated"));

    p.invert = true;
    QVERIFY(lineMatches(p, "anything at all"));
}

void tst_LineMatcher::repeatedCallsStayConsistent()
{
    // The compiled expression is cached between calls; repeated matching (including with
    // the same pattern text at a different case sensitivity) must still be correct.
    Pattern insensitive;
    insensitive.isRegex = true;
    insensitive.caseSensitive = false;
    insensitive.patternText = "ERROR";

    Pattern sensitive = insensitive;
    sensitive.caseSensitive = true;

    for (int i = 0; i < 3; ++i) {
        QVERIFY(lineMatches(insensitive, "some error here"));
        QVERIFY(lineMatches(insensitive, "some ERROR here"));
        QVERIFY(!lineMatches(sensitive, "some error here"));
        QVERIFY(lineMatches(sensitive, "some ERROR here"));
    }
}

void tst_LineMatcher::continuationOffByDefault()
{
    // groupContinuationLines defaults to false, so lineContinuesPattern() must never sweep a
    // line in just because it happens to match continuationPattern's default text.
    Pattern p;
    p.continuationPattern = "^ {1,3}";

    QVERIFY(!lineContinuesPattern(p, "  indented line"));
}

void tst_LineMatcher::continuationMatchesLeadingWhitespace()
{
    Pattern p;
    p.groupContinuationLines = true;
    p.continuationPattern = "^ {1,3}";

    QVERIFY(lineContinuesPattern(p, "  18 |       { \"wait\": \"...\" },"));
    // Matches even with many more leading spaces than the {1,3} count requires: the regex
    // only needs to find 1-3 at the start to succeed, not exactly that many total.
    QVERIFY(lineContinuesPattern(p, "                              ^"));
    QVERIFY(!lineContinuesPattern(p, "[quickautomation][INFO]: next entry"));
}

void tst_LineMatcher::continuationInvalidRegexFailsSafe()
{
    Pattern p;
    p.groupContinuationLines = true;
    p.continuationPattern = "([unterminated";

    QVERIFY(!lineContinuesPattern(p, "anything at all"));
}

void tst_LineMatcher::catastrophicRegexStaysBounded_data()
{
    QTest::addColumn<QString>("patternText");
    QTest::addColumn<QString>("line");
    // Each of these would take ~2^n steps with naive backtracking (^(\d+)*$ on 40 digits:
    // ~10^12); measured at 20-30 ms per line on Qt 6.10 / MSVC, where PCRE2's match limit
    // makes QRegularExpression give up and report no match.
    const auto repeat = [](const QString &s, int n) { return s.repeated(n); };
    QTest::newRow("(a+)+$") << "(a+)+$" << repeat("a", 5000) + "!";
    QTest::newRow("(a|aa)+$") << "(a|aa)+$" << repeat("a", 5000) + "!";
    QTest::newRow("^(\\d+)*$") << "^(\\d+)*$" << repeat("1", 40) + "x";
    QTest::newRow("^(\\w+\\s?)*$") << "^(\\w+\\s?)*$" << repeat("word ", 30) + "!";
    QTest::newRow("^(a?){30}a{30}$") << "^(a?){30}a{30}$" << repeat("a", 30);
}

// Pins what S13's circuit breaker (SlowMatchBreaker) relies on: a catastrophic pattern costs
// a bounded amount per line rather than hanging the GUI thread outright. The limit is
// deliberately generous -- this guards against a Qt/PCRE2 build without a match limit, not
// against a slow machine.
void tst_LineMatcher::catastrophicRegexStaysBounded()
{
    QFETCH(QString, patternText);
    QFETCH(QString, line);

    Pattern p;
    p.isRegex = true;
    p.patternText = patternText;
    lineMatches(p, "warm-up: compiles and caches the expression");

    QElapsedTimer timer;
    timer.start();
    lineMatches(p, line);
    const qint64 ms = timer.elapsed();
    QVERIFY2(ms < 500, qPrintable(QString("took %1 ms").arg(ms)));
}

void tst_LineMatcher::breakerPassesResultsThroughWhileFast()
{
    SlowMatchBreaker breaker;
    for (int i = 0; i < 100; ++i) {
        QCOMPARE(breaker.run([] { return true; }), true);
        QCOMPARE(breaker.run([] { return false; }), false);
    }
    QVERIFY(!breaker.isTripped());
}

// A match that is slow every time is a genuinely pathological pattern: after maxStrikes of
// them the breaker trips, and from then on the match isn't even attempted.
void tst_LineMatcher::breakerTripsAfterRepeatedSlowMatches()
{
    SlowMatchBreaker breaker(10ms, 3);
    int calls = 0;
    const auto slowMatch = [&] {
        ++calls;
        std::this_thread::sleep_for(15ms);
        return true;
    };

    QCOMPARE(breaker.run(slowMatch), true);
    QCOMPARE(breaker.run(slowMatch), true);
    QVERIFY(!breaker.isTripped());
    breaker.run(slowMatch);
    QVERIFY(breaker.isTripped());

    const int callsWhenTripped = calls;
    QCOMPARE(breaker.run(slowMatch), false);
    QCOMPARE(calls, callsWhenTripped);
}

// One slow match followed by a fast repeat is the OS stalling us, not the pattern.
void tst_LineMatcher::breakerForgivesOneOffStall()
{
    SlowMatchBreaker breaker(10ms, 1);
    bool stalled = false;
    const auto stallOnce = [&] {
        if (!stalled) {
            stalled = true;
            std::this_thread::sleep_for(15ms);
        }
        return true;
    };

    QCOMPARE(breaker.run(stallOnce), true);
    QVERIFY(!breaker.isTripped());
}

// A new run gets a fresh chance: the output (and so what the pattern chokes on) may differ.
void tst_LineMatcher::breakerResetRearms()
{
    SlowMatchBreaker breaker(10ms, 1);
    breaker.run([] {
        std::this_thread::sleep_for(15ms);
        return true;
    });
    QVERIFY(breaker.isTripped());

    breaker.reset();
    QVERIFY(!breaker.isTripped());
    QCOMPARE(breaker.run([] { return true; }), true);
}

QTEST_GUILESS_MAIN(tst_LineMatcher)

#include "tst_linematcher.moc"

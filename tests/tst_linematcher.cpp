#include "../src/linematcher.h"
#include "../src/pattern.h"

#include <QtTest>

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

QTEST_GUILESS_MAIN(tst_LineMatcher)

#include "tst_linematcher.moc"

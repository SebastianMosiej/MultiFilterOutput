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

QTEST_GUILESS_MAIN(tst_LineMatcher)

#include "tst_linematcher.moc"

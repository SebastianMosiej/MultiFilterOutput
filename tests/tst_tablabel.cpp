#include "../src/tablabel.h"

#include <QtTest>

using namespace MultiFilterOutput;

class tst_TabLabel : public QObject
{
    Q_OBJECT

private slots:
    void normalPatternShowsNameAndCount();
    void invertedPatternIsPrefixed();
    void zeroMatchesStillShowsCount();
    void tooSlowPatternIsMarked();
};

void tst_TabLabel::normalPatternShowsNameAndCount()
{
    Pattern p;
    p.name = "Errors";
    p.invert = false;
    QCOMPARE(tabLabel(p, 12), QStringLiteral("Errors (12)"));
}

void tst_TabLabel::invertedPatternIsPrefixed()
{
    Pattern p;
    p.name = "Debug";
    p.invert = true;
    QCOMPARE(tabLabel(p, 85), QStringLiteral("Not: Debug (85)"));
}

void tst_TabLabel::zeroMatchesStillShowsCount()
{
    Pattern p;
    p.name = "Network";
    QCOMPARE(tabLabel(p, 0), QStringLiteral("Network (0)"));
}

void tst_TabLabel::tooSlowPatternIsMarked()
{
    Pattern p;
    p.name = "Errors";
    QCOMPARE(tabLabel(p, 12, /*tooSlow=*/true), QStringLiteral("Errors (12) - disabled: too slow"));
}

QTEST_GUILESS_MAIN(tst_TabLabel)

#include "tst_tablabel.moc"

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
    void runLabelShowsKnownPid();
    void runLabelOmitsUnknownPid();
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

void tst_TabLabel::runLabelShowsKnownPid()
{
    QCOMPARE(runLabel("MyApp", 4242), QStringLiteral("Filtering: MyApp (pid 4242)"));
}

// Debugger, remote-device and Android/iOS runs report started before (or without ever)
// knowing their pid; "pid 0" would be misleading.
void tst_TabLabel::runLabelOmitsUnknownPid()
{
    QCOMPARE(runLabel("MyApp", 0), QStringLiteral("Filtering: MyApp"));
}

QTEST_GUILESS_MAIN(tst_TabLabel)

#include "tst_tablabel.moc"

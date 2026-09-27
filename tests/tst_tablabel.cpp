#include "../src/tablabel.h"

#include <QTabBar>
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
    void runLabelShowsKnownPid();
    void runLabelOmitsUnknownPid();
    void applyTabLabelSetsTextAndColor();
    void applyTabLabelClearsColorWhenPatternHasNone();
    void applyTabLabelMarksTooSlow();
    void placeholdersInNamesAreShownLiterally();
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

void tst_TabLabel::applyTabLabelSetsTextAndColor()
{
    QTabBar bar;
    bar.addTab(QString());
    Pattern p;
    p.name = "Errors";
    p.color = Qt::red;

    applyTabLabel(&bar, 0, p, 3);

    QCOMPARE(bar.tabText(0), QStringLiteral("Errors (3)"));
    QCOMPARE(bar.tabTextColor(0), QColor(Qt::red));
}

// A pattern whose color was removed must not keep showing the old tint.
void tst_TabLabel::applyTabLabelClearsColorWhenPatternHasNone()
{
    QTabBar bar;
    bar.addTab(QString());
    Pattern p;
    p.name = "Errors";
    p.color = Qt::red;
    applyTabLabel(&bar, 0, p, 3);

    p.color = QColor();
    applyTabLabel(&bar, 0, p, 3);

    QVERIFY(!bar.tabTextColor(0).isValid());
}

// The pane relabels through applyTabLabel(), so a tripped SlowMatchBreaker has to reach the
// tab text through it too.
void tst_TabLabel::applyTabLabelMarksTooSlow()
{
    QTabBar bar;
    bar.addTab(QString());
    Pattern p;
    p.name = "Errors";

    applyTabLabel(&bar, 0, p, 12, /*tooSlow=*/true);

    QCOMPARE(bar.tabText(0), QStringLiteral("Errors (12) - disabled: too slow"));
}

// Chained QString::arg() substitutes into text an earlier arg() inserted, so a "%1" in a
// pattern or run name would be replaced by the count or pid.
void tst_TabLabel::placeholdersInNamesAreShownLiterally()
{
    Pattern p;
    p.name = "Foo %1 %2";
    QCOMPARE(tabLabel(p, 5), QStringLiteral("Foo %1 %2 (5)"));
    QCOMPARE(runLabel("App %1", 42), QStringLiteral("Filtering: App %1 (pid 42)"));
}

QTEST_MAIN(tst_TabLabel)

#include "tst_tablabel.moc"

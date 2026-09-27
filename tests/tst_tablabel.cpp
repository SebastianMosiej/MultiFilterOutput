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
    void applyTabLabelSetsTextAndColor();
    void applyTabLabelClearsColorWhenPatternHasNone();
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

QTEST_MAIN(tst_TabLabel)

#include "tst_tablabel.moc"

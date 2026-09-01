#include "../src/pattern.h"

#include <QtTest>

using namespace MultiFilterOutput;

class tst_Pattern : public QObject
{
    Q_OBJECT

private slots:
    void roundTripsThroughMap();
    void createNewGeneratesUniqueIds();
};

void tst_Pattern::roundTripsThroughMap()
{
    Pattern p;
    p.id = "abc-123";
    p.name = "Errors";
    p.patternText = "ERROR";
    p.isRegex = false;
    p.caseSensitive = true;
    p.invert = false;
    p.color = QColor(Qt::red);

    const Pattern restored = Pattern::fromMap(p.toMap());

    QCOMPARE(restored.id, p.id);
    QCOMPARE(restored.name, p.name);
    QCOMPARE(restored.patternText, p.patternText);
    QCOMPARE(restored.isRegex, p.isRegex);
    QCOMPARE(restored.caseSensitive, p.caseSensitive);
    QCOMPARE(restored.invert, p.invert);
    QCOMPARE(restored.color, p.color);
}

void tst_Pattern::createNewGeneratesUniqueIds()
{
    const Pattern a = Pattern::createNew();
    const Pattern b = Pattern::createNew();
    QVERIFY(!a.id.isEmpty());
    QVERIFY(a.id != b.id);
}

QTEST_GUILESS_MAIN(tst_Pattern)

#include "tst_pattern.moc"

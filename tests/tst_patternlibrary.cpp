#include "../src/patternlibrary.h"

#include <QSignalSpy>
#include <QtTest>

using namespace MultiFilterOutput;

class tst_PatternLibrary : public QObject
{
    Q_OBJECT

private slots:
    void addOrUpdateInsertsNewPattern();
    void addOrUpdateUpsertsExisting();
    void removeDeletesById();
    void patternByIdReturnsNulloptWhenMissing();
    void mutationsEmitPatternsChanged();
    void setPatternsReplacesWholeListAndEmitsOnce();
};

void tst_PatternLibrary::addOrUpdateInsertsNewPattern()
{
    PatternLibrary lib;
    Pattern p = Pattern::createNew();
    p.name = "Errors";
    lib.addOrUpdate(p);

    QCOMPARE(lib.patterns().size(), 1);
    QCOMPARE(lib.patterns().first().name, QStringLiteral("Errors"));
}

void tst_PatternLibrary::addOrUpdateUpsertsExisting()
{
    PatternLibrary lib;
    Pattern p = Pattern::createNew();
    p.name = "Errors";
    lib.addOrUpdate(p);

    p.name = "Errors (renamed)";
    lib.addOrUpdate(p);

    QCOMPARE(lib.patterns().size(), 1);
    QCOMPARE(lib.patterns().first().name, QStringLiteral("Errors (renamed)"));
}

void tst_PatternLibrary::removeDeletesById()
{
    PatternLibrary lib;
    Pattern p = Pattern::createNew();
    lib.addOrUpdate(p);
    lib.remove(p.id);

    QVERIFY(lib.patterns().isEmpty());
}

void tst_PatternLibrary::patternByIdReturnsNulloptWhenMissing()
{
    PatternLibrary lib;
    QVERIFY(!lib.patternById("does-not-exist").has_value());
}

void tst_PatternLibrary::mutationsEmitPatternsChanged()
{
    PatternLibrary lib;
    QSignalSpy spy(&lib, &PatternLibrary::patternsChanged);

    Pattern p = Pattern::createNew();
    lib.addOrUpdate(p);
    QCOMPARE(spy.count(), 1);

    lib.remove(p.id);
    QCOMPARE(spy.count(), 2);
}

void tst_PatternLibrary::setPatternsReplacesWholeListAndEmitsOnce()
{
    PatternLibrary lib;
    Pattern stale = Pattern::createNew();
    stale.name = "Stale";
    lib.addOrUpdate(stale);

    Pattern first = Pattern::createNew();
    first.name = "First";
    Pattern second = Pattern::createNew();
    second.name = "Second";

    QSignalSpy spy(&lib, &PatternLibrary::patternsChanged);
    lib.setPatterns({first, second});

    QCOMPARE(spy.count(), 1);
    QCOMPARE(lib.patterns().size(), 2);
    QCOMPARE(lib.patterns().at(0).name, QStringLiteral("First"));
    QCOMPARE(lib.patterns().at(1).name, QStringLiteral("Second"));
    QVERIFY(!lib.patternById(stale.id).has_value());
}

QTEST_GUILESS_MAIN(tst_PatternLibrary)

#include "tst_patternlibrary.moc"

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
    void patternsWithIdsFollowsLibraryOrder();
    void validLibraryPassesUntouched();
    void missingIdIsRegenerated();
    void duplicateIdKeepsFirstAndRenamesOthers();
    void emptyPatternTextIsReportedButKept();
    void invalidRegexesAreReported();
};

namespace {

Pattern makePattern(const QString &id, const QString &name, const QString &text)
{
    Pattern p;
    p.id = id;
    p.name = name;
    p.patternText = text;
    return p;
}

} // namespace

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

// A run configuration's enabled ids are stored in the order their boxes were ticked; the
// tabs must still follow the Pattern Library's own order, which is what the user reorders.
// Ids no longer in the library (pattern deleted since) are skipped.
void tst_PatternLibrary::patternsWithIdsFollowsLibraryOrder()
{
    PatternLibrary lib;
    Pattern a = Pattern::createNew();
    a.name = "A";
    Pattern b = Pattern::createNew();
    b.name = "B";
    Pattern c = Pattern::createNew();
    c.name = "C";
    lib.setPatterns({a, b, c});

    const QList<Pattern> result = lib.patternsWithIds({c.id, "deleted-id", a.id});

    QCOMPARE(result.size(), 2);
    QCOMPARE(result.at(0).name, QStringLiteral("A"));
    QCOMPARE(result.at(1).name, QStringLiteral("C"));
}

void tst_PatternLibrary::validLibraryPassesUntouched()
{
    QList<Pattern> patterns{makePattern("id-1", "Errors", "ERROR"), makePattern("id-2", "Warn", "W.*")};
    const QList<Pattern> before = patterns;

    QVERIFY(validatePatterns(patterns).isEmpty());
    QCOMPARE(patterns.size(), 2);
    QCOMPARE(patterns.at(0).toMap(), before.at(0).toMap());
    QCOMPARE(patterns.at(1).toMap(), before.at(1).toMap());
}

// A pattern with no id can't be selected by any run configuration (selections are stored by
// id), so it gets one.
void tst_PatternLibrary::missingIdIsRegenerated()
{
    QList<Pattern> patterns{makePattern("", "Errors", "ERROR")};

    const QStringList problems = validatePatterns(patterns);

    QCOMPARE(problems.size(), 1);
    QVERIFY(problems.first().contains("Errors"));
    QVERIFY(!patterns.first().id.isEmpty());
}

// patternById() returns the first pattern with an id, so that is the one existing run
// configuration selections already mean; the others need ids of their own to be selectable.
void tst_PatternLibrary::duplicateIdKeepsFirstAndRenamesOthers()
{
    QList<Pattern> patterns{
        makePattern("dup", "First", "A"),
        makePattern("dup", "Second", "B"),
        makePattern("dup", "Third", "C")};

    const QStringList problems = validatePatterns(patterns);

    QCOMPARE(problems.size(), 2);
    QCOMPARE(patterns.size(), 3);
    QCOMPARE(patterns.at(0).id, QStringLiteral("dup"));
    QCOMPARE(patterns.at(0).name, QStringLiteral("First"));
    QVERIFY(!patterns.at(1).id.isEmpty() && patterns.at(1).id != "dup");
    QVERIFY(!patterns.at(2).id.isEmpty() && patterns.at(2).id != "dup");
    QVERIFY(patterns.at(1).id != patterns.at(2).id);
}

// Empty text would match every line; the editor refuses it, so it came from outside. It's
// reported, not deleted -- the user decides what it should have been.
void tst_PatternLibrary::emptyPatternTextIsReportedButKept()
{
    QList<Pattern> patterns{makePattern("id-1", "Blank", "")};

    const QStringList problems = validatePatterns(patterns);

    QCOMPARE(problems.size(), 1);
    QVERIFY(problems.first().contains("Blank"));
    QCOMPARE(patterns.size(), 1);
    QCOMPARE(patterns.first().patternText, QString());
}

void tst_PatternLibrary::invalidRegexesAreReported()
{
    Pattern badMain = makePattern("id-1", "BadMain", "([unterminated");
    Pattern badContinuation = makePattern("id-2", "BadCont", "ok");
    badContinuation.groupContinuationLines = true;
    badContinuation.continuationPattern = "([unterminated";
    // Not grouping, so its continuation pattern is never used and not worth reporting.
    Pattern unusedContinuation = makePattern("id-3", "Unused", "ok");
    unusedContinuation.continuationPattern = "([unterminated";
    Pattern plainText = makePattern("id-4", "Plain", "([not a regex");
    plainText.isRegex = false;
    QList<Pattern> patterns{badMain, badContinuation, unusedContinuation, plainText};

    const QStringList problems = validatePatterns(patterns);

    QCOMPARE(problems.size(), 2);
    QVERIFY(problems.at(0).contains("BadMain"));
    QVERIFY(problems.at(1).contains("BadCont"));
}

QTEST_GUILESS_MAIN(tst_PatternLibrary)

#include "tst_patternlibrary.moc"

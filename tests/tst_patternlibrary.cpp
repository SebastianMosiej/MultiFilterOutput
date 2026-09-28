#include "../src/patternlibrary.h"

#include <utils/qtcsettings.h>

#include <QSignalSpy>
#include <QTemporaryDir>
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
    void loadedLibraryPersistsEveryMutation();
    void roundTripKeepsEveryField();
    void removeReportsRemovedId();
    void setPatternsReportsDroppedIds();
    void noRemovalReportsNothing();
    void saveStampsSchemaVersion();
    void unversionedSettingsMigrateLegacyGray();
    void versionedSettingsKeepGray();
    void newerSchemaVersionIsReported();
    void loadPersistsLegacyMigration();
    void loadPersistsRepairedIds();
    void loadNeverRewritesNewerSettings();
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
    plainText.excludeText = "([not a regex either";
    Pattern badExclude = makePattern("id-5", "BadExclude", "ok");
    badExclude.excludeText = "([unterminated";
    QList<Pattern> patterns{badMain, badContinuation, unusedContinuation, plainText, badExclude};

    const QStringList problems = validatePatterns(patterns);

    QCOMPARE(problems.size(), 3);
    QVERIFY(problems.at(0).contains("BadMain"));
    QVERIFY(problems.at(1).contains("BadCont"));
    QVERIFY(problems.at(2).contains("BadExclude"));
}

// Reads back what `settings` holds, the way the next Qt Creator session would.
static QList<Pattern> reloaded(Utils::QtcSettings &settings)
{
    PatternLibrary fresh;
    fresh.load(&settings);
    return fresh.patterns();
}

// Nothing but the settings page's apply() used to call save(); any other caller of a
// mutator would silently lose its change on a crash. Once loaded, the library persists
// every change itself.
void tst_PatternLibrary::loadedLibraryPersistsEveryMutation()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    Utils::QtcSettings settings(dir.filePath("settings.ini"), QSettings::IniFormat);
    PatternLibrary lib;
    lib.load(&settings);

    const Pattern a = makePattern("id-a", "A", "a");
    const Pattern b = makePattern("id-b", "B", "b");
    lib.addOrUpdate(a);
    QCOMPARE(reloaded(settings).size(), 1);

    lib.setPatterns({a, b});
    QCOMPARE(reloaded(settings).size(), 2);

    Pattern renamed = b;
    renamed.name = "B renamed";
    lib.addOrUpdate(renamed);
    QCOMPARE(reloaded(settings).at(1).name, QStringLiteral("B renamed"));

    lib.remove("id-a");
    const QList<Pattern> afterRemove = reloaded(settings);
    QCOMPARE(afterRemove.size(), 1);
    QCOMPARE(afterRemove.first().id, QStringLiteral("id-b"));
}

void tst_PatternLibrary::roundTripKeepsEveryField()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    Utils::QtcSettings settings(dir.filePath("settings.ini"), QSettings::IniFormat);
    PatternLibrary lib;
    lib.load(&settings);

    Pattern p = makePattern("id-1", "Errors", "ERR(OR)?");
    p.isRegex = false;
    p.caseSensitive = true;
    p.invert = true;
    p.color = QColor(Qt::red);
    p.groupContinuationLines = true;
    p.continuationPattern = "^\\s+";
    p.isDefault = true;
    p.channels = Pattern::StdErrChannel;
    p.excludeText = "flaky";
    p.wholeWord = true;
    p.contextBefore = 1;
    p.contextAfter = 2;
    p.tags = {"net"}; // one item: QSettings' INI format reads it back as a plain string
    // Every field off its default: a field Pattern::toMap() has but save()/load() miss shows
    // up as a difference below.
    const QVariantMap fields = p.toMap();
    const QVariantMap defaults = Pattern().toMap();
    QCOMPARE(fields.keys(), defaults.keys());
    for (auto it = fields.cbegin(); it != fields.cend(); ++it)
        QVERIFY2(it.value() != defaults.value(it.key()), qPrintable(it.key()));
    lib.addOrUpdate(p);

    const QList<Pattern> back = reloaded(settings);
    QCOMPARE(back.size(), 1);
    QCOMPARE(back.first().toMap(), p.toMap());
}

void tst_PatternLibrary::removeReportsRemovedId()
{
    PatternLibrary lib;
    lib.setPatterns({makePattern("id-a", "A", "a"), makePattern("id-b", "B", "b")});
    QSignalSpy spy(&lib, &PatternLibrary::patternsRemoved);

    lib.remove("id-a");

    QCOMPARE(spy.size(), 1);
    QCOMPARE(spy.first().first().toStringList(), QStringList{"id-a"});
}

// The settings page applies its edits via setPatterns(), so a deletion there arrives as
// "the new list no longer has these ids".
void tst_PatternLibrary::setPatternsReportsDroppedIds()
{
    PatternLibrary lib;
    lib.setPatterns(
        {makePattern("id-a", "A", "a"), makePattern("id-b", "B", "b"), makePattern("id-c", "C", "c")});
    QSignalSpy spy(&lib, &PatternLibrary::patternsRemoved);

    lib.setPatterns({makePattern("id-b", "B", "b"), makePattern("id-d", "D", "d")});

    QCOMPARE(spy.size(), 1);
    QCOMPARE(spy.first().first().toStringList(), (QStringList{"id-a", "id-c"}));
}

void tst_PatternLibrary::noRemovalReportsNothing()
{
    PatternLibrary lib;
    lib.setPatterns({makePattern("id-a", "A", "a")});
    QSignalSpy spy(&lib, &PatternLibrary::patternsRemoved);

    lib.addOrUpdate(makePattern("id-b", "B", "b"));
    lib.setPatterns({makePattern("id-b", "B", "b"), makePattern("id-a", "A2", "a")});
    lib.remove("no-such-id");

    QCOMPARE(spy.size(), 0);
}

// Writes one pattern the way a given schema version stored it: `schemaVersion` < 0 means no
// SchemaVersion key at all, like every release before the key existed.
static void writeRawPattern(
    Utils::QtcSettings &settings, int schemaVersion, const QString &id, const QColor &color)
{
    settings.beginGroup("MultiFilterOutput");
    if (schemaVersion >= 0)
        settings.setValue("SchemaVersion", schemaVersion);
    settings.beginWriteArray("patterns", 1);
    settings.setArrayIndex(0);
    settings.setValue("id", id);
    settings.setValue("name", "Legacy");
    settings.setValue("patternText", "x");
    settings.setValue("color", color);
    settings.endArray();
    settings.endGroup();
}

static QVariant rawSchemaVersion(Utils::QtcSettings &settings)
{
    settings.beginGroup("MultiFilterOutput");
    const QVariant version = settings.value("SchemaVersion");
    settings.endGroup();
    return version;
}

void tst_PatternLibrary::saveStampsSchemaVersion()
{
    QTemporaryDir dir;
    Utils::QtcSettings settings(dir.filePath("settings.ini"), QSettings::IniFormat);
    PatternLibrary lib;
    lib.load(&settings);

    lib.addOrUpdate(makePattern("id-1", "A", "a"));

    QCOMPARE(rawSchemaVersion(settings).toInt(), 1);
}

// Releases before the SchemaVersion key stored QColor(Qt::gray) for every pattern nobody had
// colored; that still has to come back as "no tint".
void tst_PatternLibrary::unversionedSettingsMigrateLegacyGray()
{
    QTemporaryDir dir;
    Utils::QtcSettings settings(dir.filePath("settings.ini"), QSettings::IniFormat);
    writeRawPattern(settings, -1, "id-1", QColor(Qt::gray));

    PatternLibrary lib;
    lib.load(&settings);

    QCOMPARE(lib.patterns().size(), 1);
    QVERIFY(!lib.patterns().first().color.isValid());
}

// Once the settings say which format they are in, gray is just a color the user chose.
void tst_PatternLibrary::versionedSettingsKeepGray()
{
    QTemporaryDir dir;
    Utils::QtcSettings settings(dir.filePath("settings.ini"), QSettings::IniFormat);
    writeRawPattern(settings, 1, "id-1", QColor(Qt::gray));

    PatternLibrary lib;
    lib.load(&settings);

    QCOMPARE(lib.patterns().first().color, QColor(Qt::gray));
}

// Saving from an older build can't preserve what a newer one added; the user should know
// before that happens.
void tst_PatternLibrary::newerSchemaVersionIsReported()
{
    QTemporaryDir dir;
    Utils::QtcSettings settings(dir.filePath("settings.ini"), QSettings::IniFormat);
    writeRawPattern(settings, 99, "id-1", QColor(Qt::red));

    PatternLibrary lib;
    const QStringList problems = lib.load(&settings);

    QCOMPARE(problems.size(), 1);
    QVERIFY(problems.first().contains("99"));
    QCOMPARE(lib.patterns().size(), 1);
}

// Migrated legacy data is written back right away, so the migration runs exactly once --
// not again after a crash, and without relying on a save at shutdown.
void tst_PatternLibrary::loadPersistsLegacyMigration()
{
    QTemporaryDir dir;
    Utils::QtcSettings settings(dir.filePath("settings.ini"), QSettings::IniFormat);
    writeRawPattern(settings, -1, "id-1", QColor(Qt::gray));

    PatternLibrary lib;
    lib.load(&settings);

    QCOMPARE(rawSchemaVersion(settings).toInt(), 1);
    QVERIFY(!reloaded(settings).first().color.isValid());
}

// A repaired id that only lived in memory until shutdown would come back different after a
// crash -- orphaning any run configuration that selected the pattern in the meantime.
void tst_PatternLibrary::loadPersistsRepairedIds()
{
    QTemporaryDir dir;
    Utils::QtcSettings settings(dir.filePath("settings.ini"), QSettings::IniFormat);
    writeRawPattern(settings, 1, "", QColor());

    PatternLibrary lib;
    lib.load(&settings);
    const QString repairedId = lib.patterns().first().id;

    QVERIFY(!repairedId.isEmpty());
    QCOMPARE(reloaded(settings).first().id, repairedId);
}

// Loading alone must never downgrade what a newer plugin version wrote; only an actual edit
// (which the load warning announces) may.
void tst_PatternLibrary::loadNeverRewritesNewerSettings()
{
    QTemporaryDir dir;
    Utils::QtcSettings settings(dir.filePath("settings.ini"), QSettings::IniFormat);
    writeRawPattern(settings, 99, "", QColor(Qt::gray));

    PatternLibrary lib;
    lib.load(&settings);

    QCOMPARE(rawSchemaVersion(settings).toInt(), 99);
}

QTEST_GUILESS_MAIN(tst_PatternLibrary)

#include "tst_patternlibrary.moc"

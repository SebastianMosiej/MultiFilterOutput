#include "../src/patternexchange.h"
#include "../src/projectpatterns.h"

#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

using namespace MultiFilterOutput;
using Utils::FilePath;

class tst_ProjectPatterns : public QObject
{
    Q_OBJECT

private slots:
    void patternFileLivesInDotQtcreator();
    void addingAProjectReadsItsFile();
    void projectWithoutAFileHasNoPatterns();
    void reloadSignalsOnlyRealChanges();
    void unreadableFileKeepsWhatWasReadAndReports();
    void changesOnDiskAreNoticed();
    void aFileCreatedLaterIsNoticed();
    void projectsAreCounted();
    void shareCreatesTheFile();
    void shareReplacesByIdAndKeepsTheRest();
    void shareRefusesToOverwriteAnUnreadableFile();
    void combinedPatternsLetTheProjectWin();
    void entriesWithoutIdsKeepTheirIds();
    void problemsAreReportedOncePerChange();
    void shutdownStopsWatching();
};

static Pattern pattern(const QString &id, const QString &name, const QString &text = "x")
{
    Pattern p;
    p.id = id;
    p.name = name;
    p.patternText = text;
    return p;
}

static QStringList names(const QList<Pattern> &patterns)
{
    QStringList result;
    for (const Pattern &p : patterns)
        result << p.name;
    return result;
}

static void writeFile(const FilePath &projectDir, const QByteArray &contents)
{
    const FilePath file = ProjectPatterns::patternFile(projectDir);
    QVERIFY(file.parentDir().ensureWritableDir());
    QVERIFY(file.writeFileContents(contents));
}

void tst_ProjectPatterns::patternFileLivesInDotQtcreator()
{
    QCOMPARE(
        ProjectPatterns::patternFile(FilePath::fromString("/work/app")),
        FilePath::fromString("/work/app/.qtcreator/multifilteroutput-patterns.json"));
}

void tst_ProjectPatterns::addingAProjectReadsItsFile()
{
    QTemporaryDir dir;
    const FilePath project = FilePath::fromString(dir.path());
    writeFile(project, exportPatterns({pattern("t1", "Team errors")}));

    ProjectPatterns store;
    QSignalSpy changed(&store, &ProjectPatterns::patternsChanged);
    store.addProject(project);

    QVERIFY(store.hasProject(project));
    QCOMPARE(names(store.patterns(project)), QStringList{"Team errors"});
    QCOMPARE(changed.size(), 1);
}

void tst_ProjectPatterns::projectWithoutAFileHasNoPatterns()
{
    QTemporaryDir dir;
    const FilePath project = FilePath::fromString(dir.path());
    ProjectPatterns store;
    QSignalSpy problems(&store, &ProjectPatterns::problemsFound);
    store.addProject(project);
    QVERIFY(store.hasProject(project));
    QVERIFY(store.patterns(project).isEmpty());
    QCOMPARE(problems.size(), 0);
}

void tst_ProjectPatterns::reloadSignalsOnlyRealChanges()
{
    QTemporaryDir dir;
    const FilePath project = FilePath::fromString(dir.path());
    writeFile(project, exportPatterns({pattern("t1", "One")}));
    ProjectPatterns store;
    store.addProject(project);
    QSignalSpy changed(&store, &ProjectPatterns::patternsChanged);

    store.reload(project);
    QCOMPARE(changed.size(), 0);

    writeFile(project, exportPatterns({pattern("t1", "One, renamed")}));
    store.reload(project);
    QCOMPARE(changed.size(), 1);
    QCOMPARE(changed.first().first().value<FilePath>(), project);
    QCOMPARE(names(store.patterns(project)), QStringList{"One, renamed"});
}

void tst_ProjectPatterns::unreadableFileKeepsWhatWasReadAndReports()
{
    QTemporaryDir dir;
    const FilePath project = FilePath::fromString(dir.path());
    writeFile(project, exportPatterns({pattern("t1", "One")}));
    ProjectPatterns store;
    store.addProject(project);
    QSignalSpy problems(&store, &ProjectPatterns::problemsFound);

    writeFile(project, "<<<<<<< HEAD\n{ not json");
    store.reload(project);

    QCOMPARE(names(store.patterns(project)), QStringList{"One"});
    QCOMPARE(problems.size(), 1);
    QCOMPARE(problems.first().first().value<FilePath>(), ProjectPatterns::patternFile(project));
}

void tst_ProjectPatterns::changesOnDiskAreNoticed()
{
    QTemporaryDir dir;
    const FilePath project = FilePath::fromString(dir.path());
    writeFile(project, exportPatterns({pattern("t1", "One")}));
    ProjectPatterns store;
    store.addProject(project);

    writeFile(project, exportPatterns({pattern("t1", "One"), pattern("t2", "Two")}));
    QTRY_COMPARE_WITH_TIMEOUT(names(store.patterns(project)), (QStringList{"One", "Two"}), 5000);
}

// E.g. a teammate's commit adds the file and you pull.
void tst_ProjectPatterns::aFileCreatedLaterIsNoticed()
{
    QTemporaryDir dir;
    const FilePath project = FilePath::fromString(dir.path());
    ProjectPatterns store;
    store.addProject(project);

    writeFile(project, exportPatterns({pattern("t1", "Pulled")}));
    QTRY_COMPARE_WITH_TIMEOUT(names(store.patterns(project)), QStringList{"Pulled"}, 5000);
}

void tst_ProjectPatterns::projectsAreCounted()
{
    QTemporaryDir dir;
    const FilePath project = FilePath::fromString(dir.path());
    writeFile(project, exportPatterns({pattern("t1", "One")}));
    ProjectPatterns store;
    store.addProject(project);
    store.addProject(project);

    store.removeProject(project);
    QCOMPARE(names(store.patterns(project)), QStringList{"One"});
    store.removeProject(project);
    QVERIFY(!store.hasProject(project));
    QVERIFY(store.patterns(project).isEmpty());
}

void tst_ProjectPatterns::shareCreatesTheFile()
{
    QTemporaryDir dir;
    const FilePath project = FilePath::fromString(dir.path());
    ProjectPatterns store;
    store.addProject(project);

    const Utils::Result<> shared = store.share(project, {pattern("p1", "Shared")});

    // QVERIFY2 builds its message either way, and error() on a success throws.
    QVERIFY2(shared, shared ? "" : qPrintable(shared.error()));
    QCOMPARE(names(store.patterns(project)), QStringList{"Shared"});
    const ParsedPatterns onDisk = parsePatterns(
        ProjectPatterns::patternFile(project).fileContents().value_or(QByteArray()));
    QCOMPARE(names(onDisk.patterns), QStringList{"Shared"});
}

void tst_ProjectPatterns::shareReplacesByIdAndKeepsTheRest()
{
    QTemporaryDir dir;
    const FilePath project = FilePath::fromString(dir.path());
    writeFile(project, exportPatterns({pattern("t1", "One"), pattern("t2", "Two")}));
    ProjectPatterns store;
    store.addProject(project);

    QVERIFY(store.share(project, {pattern("t1", "One, improved"), pattern("t3", "Three")}));

    QCOMPARE(names(store.patterns(project)), (QStringList{"One, improved", "Two", "Three"}));
}

void tst_ProjectPatterns::shareRefusesToOverwriteAnUnreadableFile()
{
    QTemporaryDir dir;
    const FilePath project = FilePath::fromString(dir.path());
    writeFile(project, "{ half a merge");
    ProjectPatterns store;
    store.addProject(project);

    QVERIFY(!store.share(project, {pattern("p1", "Shared")}));
    QCOMPARE(
        ProjectPatterns::patternFile(project).fileContents().value_or(QByteArray()),
        QByteArray("{ half a merge"));
}

void tst_ProjectPatterns::combinedPatternsLetTheProjectWin()
{
    const QList<Pattern> library{pattern("a", "A"), pattern("b", "B mine"), pattern("c", "C")};
    const QList<Pattern> project{pattern("t", "Team"), pattern("b", "B shared")};

    QCOMPARE(
        names(combinedPatterns(library, project)),
        (QStringList{"A", "B shared", "C", "Team"}));
    QCOMPARE(names(combinedPatterns(library, {})), (QStringList{"A", "B mine", "C"}));
}

static const QByteArray idless = R"({"format": "MultiFilterOutput.PatternLibrary", "version": 1,
    "patterns": [{"name": "Hand-written", "patternText": "x"}]})";

// Qt Creator writes the project's .user file into .qtcreator too, so the file is re-read on
// every project save: an id made up afresh each time would lose every run configuration's
// selection of the pattern.
void tst_ProjectPatterns::entriesWithoutIdsKeepTheirIds()
{
    QTemporaryDir dir;
    const FilePath project = FilePath::fromString(dir.path());
    writeFile(project, idless);
    ProjectPatterns store;
    store.addProject(project);
    const QString id = store.patterns(project).first().id;
    QSignalSpy changed(&store, &ProjectPatterns::patternsChanged);

    store.reload(project);
    QCOMPARE(changed.size(), 0);
    ProjectPatterns nextSession;
    nextSession.addProject(project);
    QCOMPARE(nextSession.patterns(project).first().id, id);
}

void tst_ProjectPatterns::problemsAreReportedOncePerChange()
{
    QTemporaryDir dir;
    const FilePath project = FilePath::fromString(dir.path());
    writeFile(project, idless);
    ProjectPatterns store;
    QSignalSpy problems(&store, &ProjectPatterns::problemsFound);
    store.addProject(project);
    QCOMPARE(problems.size(), 1);

    store.reload(project);
    store.reload(project);
    QCOMPARE(problems.size(), 1);

    writeFile(project, "{ broken");
    store.reload(project);
    store.reload(project);
    QCOMPARE(problems.size(), 2);
}

void tst_ProjectPatterns::shutdownStopsWatching()
{
    QTemporaryDir dir;
    const FilePath project = FilePath::fromString(dir.path());
    writeFile(project, exportPatterns({pattern("t1", "One")}));
    ProjectPatterns store;
    store.addProject(project);

    store.shutdown();
    QVERIFY(!store.hasProject(project));
    // Late calls (projects closing during shutdown) are harmless.
    store.removeProject(project);
    store.addProject(project);
    QCOMPARE(names(store.patterns(project)), QStringList{"One"});
    store.reload(project);
}

QTEST_GUILESS_MAIN(tst_ProjectPatterns)

#include "tst_projectpatterns.moc"

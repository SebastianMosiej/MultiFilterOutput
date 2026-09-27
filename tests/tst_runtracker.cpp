#include "../src/runtracker.h"

#include <projectexplorer/projectexplorerconstants.h>
#include <projectexplorer/runcontrol.h>

#include <QSignalSpy>
#include <QtTest>

#include <memory>

using namespace MultiFilterOutput;
using ProjectExplorer::RunControl;

// Drives RunTracker the way ProjectExplorerPlugin's runControlStarted/runControlStoped
// signals do (see RunTracker::attachToProjectExplorer()), with real RunControls that are
// never actually started -- their output is posted directly.
class tst_RunTracker : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void startedRunBecomesActive();
    void activeRunLinesAreEmittedWithFormat();
    void newerRunReplacesActiveAndOlderOutputIsIgnored();
    void stoppedRunStaysActiveAndFlushesItsLastLine();
    void outputAfterStopIsIgnored();
    void destroyingActiveRunClearsIt();
    void destroyingInactiveRunKeepsActive();
    void rerunOfSameRunControlFlowsAgain();
    void rerunOfSameRunControlResetsPane();

private:
    static std::unique_ptr<RunControl> makeRun()
    {
        return std::make_unique<RunControl>(ProjectExplorer::Constants::NORMAL_RUN_MODE);
    }
};

// runtracker.h only forward-declares RunControl, so its pointer type isn't registered
// there; without this QSignalSpy records activeRunChanged() arguments as empty QVariants --
// which would make "the new active run is nullptr" checks pass vacuously.
void tst_RunTracker::initTestCase()
{
    qRegisterMetaType<RunControl *>();
}

void tst_RunTracker::startedRunBecomesActive()
{
    RunTracker tracker;
    QSignalSpy changed(&tracker, &RunTracker::activeRunChanged);
    auto run = makeRun();

    tracker.trackRunControl(run.get());

    QCOMPARE(tracker.activeRunControl(), run.get());
    QCOMPARE(changed.size(), 1);
    QCOMPARE(changed.first().first().value<RunControl *>(), run.get());
}

void tst_RunTracker::activeRunLinesAreEmittedWithFormat()
{
    RunTracker tracker;
    auto run = makeRun();
    tracker.trackRunControl(run.get());
    QSignalSpy lines(&tracker, &RunTracker::activeRunLineReceived);

    run->postMessage("one\ntw", Utils::StdOutFormat, false);
    run->postMessage("o\n", Utils::StdOutFormat, false);
    run->postMessage("oops\n", Utils::StdErrFormat, false);

    QCOMPARE(lines.size(), 3);
    QCOMPARE(lines.at(0).at(0).toString(), QStringLiteral("one"));
    QCOMPARE(lines.at(1).at(0).toString(), QStringLiteral("two"));
    QCOMPARE(lines.at(2).at(0).toString(), QStringLiteral("oops"));
    QCOMPARE(lines.at(2).at(1).value<Utils::OutputFormat>(), Utils::StdErrFormat);
}

// Like Application Output, a freshly started run takes over; the older one, still running,
// no longer reaches the pane.
void tst_RunTracker::newerRunReplacesActiveAndOlderOutputIsIgnored()
{
    RunTracker tracker;
    auto older = makeRun();
    auto newer = makeRun();
    tracker.trackRunControl(older.get());
    tracker.trackRunControl(newer.get());
    QSignalSpy lines(&tracker, &RunTracker::activeRunLineReceived);

    older->postMessage("from older", Utils::StdOutFormat);
    newer->postMessage("from newer", Utils::StdOutFormat);

    QCOMPARE(tracker.activeRunControl(), newer.get());
    QCOMPARE(lines.size(), 1);
    QCOMPARE(lines.first().first().toString(), QStringLiteral("from newer"));
}

// A finished run keeps its tabs (as Application Output keeps its output), and its last line
// arrives even without a trailing newline (S04).
void tst_RunTracker::stoppedRunStaysActiveAndFlushesItsLastLine()
{
    RunTracker tracker;
    auto run = makeRun();
    tracker.trackRunControl(run.get());
    QSignalSpy changed(&tracker, &RunTracker::activeRunChanged);
    QSignalSpy lines(&tracker, &RunTracker::activeRunLineReceived);

    run->postMessage("Done.", Utils::StdOutFormat, false);
    QCOMPARE(lines.size(), 0);
    tracker.untrackRunControl(run.get());

    QCOMPARE(lines.size(), 1);
    QCOMPARE(lines.first().first().toString(), QStringLiteral("Done."));
    QCOMPARE(tracker.activeRunControl(), run.get());
    QCOMPARE(changed.size(), 0);
}

// Verified for S05: Qt Creator posts nothing to a run after it stops; the tracker has
// deliberately stopped listening by then.
void tst_RunTracker::outputAfterStopIsIgnored()
{
    RunTracker tracker;
    auto run = makeRun();
    tracker.trackRunControl(run.get());
    tracker.untrackRunControl(run.get());
    QSignalSpy lines(&tracker, &RunTracker::activeRunLineReceived);

    run->postMessage("late", Utils::StdOutFormat);

    QCOMPARE(lines.size(), 0);
}

void tst_RunTracker::destroyingActiveRunClearsIt()
{
    RunTracker tracker;
    auto run = makeRun();
    tracker.trackRunControl(run.get());
    tracker.untrackRunControl(run.get());
    QSignalSpy changed(&tracker, &RunTracker::activeRunChanged);

    run.reset();

    QCOMPARE(tracker.activeRunControl(), nullptr);
    QCOMPARE(changed.size(), 1);
    QCOMPARE(changed.first().first().value<RunControl *>(), nullptr);
}

void tst_RunTracker::destroyingInactiveRunKeepsActive()
{
    RunTracker tracker;
    auto older = makeRun();
    auto newer = makeRun();
    tracker.trackRunControl(older.get());
    tracker.trackRunControl(newer.get());
    QSignalSpy changed(&tracker, &RunTracker::activeRunChanged);

    older.reset();

    QCOMPARE(tracker.activeRunControl(), newer.get());
    QCOMPARE(changed.size(), 0);
}

// Application Output's Re-run restarts the *same* RunControl, which reports started again:
// its new output must reach the pane.
void tst_RunTracker::rerunOfSameRunControlFlowsAgain()
{
    RunTracker tracker;
    auto run = makeRun();
    tracker.trackRunControl(run.get());
    tracker.untrackRunControl(run.get());
    QSignalSpy lines(&tracker, &RunTracker::activeRunLineReceived);

    tracker.trackRunControl(run.get());
    run->postMessage("second run", Utils::StdOutFormat);

    QCOMPARE(lines.size(), 1);
    QCOMPARE(lines.first().first().toString(), QStringLiteral("second run"));
}

// ...and resets the pane like any new run does -- as Application Output also treats a Re-run
// like a new run (AppOutputPane::handleOldOutput() for both). The re-run is still the
// latched active run, so without special handling activeRunChanged() would never fire (S24).
void tst_RunTracker::rerunOfSameRunControlResetsPane()
{
    RunTracker tracker;
    auto run = makeRun();
    tracker.trackRunControl(run.get());
    tracker.untrackRunControl(run.get());
    QSignalSpy changed(&tracker, &RunTracker::activeRunChanged);

    tracker.trackRunControl(run.get());

    QCOMPARE(changed.size(), 1);
    QCOMPARE(changed.first().first().value<RunControl *>(), run.get());
    QCOMPARE(tracker.activeRunControl(), run.get());
}

QTEST_MAIN(tst_RunTracker)

#include "tst_runtracker.moc"

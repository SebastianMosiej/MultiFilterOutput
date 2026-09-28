#include "runtracker.h"

#include <projectexplorer/projectexplorer.h>
#include <projectexplorer/runcontrol.h>

#include <QDateTime>

using namespace ProjectExplorer;

namespace MultiFilterOutput {

RunTracker::RunTracker(QObject *parent)
    : QObject(parent)
{}

void RunTracker::attachToProjectExplorer()
{
    for (RunControl *rc : ProjectExplorerPlugin::allRunControls())
        trackRunControl(rc);

    connect(
        ProjectExplorerPlugin::instance(),
        &ProjectExplorerPlugin::runControlStarted,
        this,
        &RunTracker::trackRunControl);
    connect(
        ProjectExplorerPlugin::instance(),
        &ProjectExplorerPlugin::runControlStoped,
        this,
        &RunTracker::untrackRunControl);
}

ProjectExplorer::RunControl *RunTracker::activeRunControl() const
{
    return m_active;
}

RunTracker::FollowMode RunTracker::followMode() const
{
    return m_followMode;
}

void RunTracker::setFollowMode(FollowMode mode)
{
    if (mode == FollowMode::ApplicationOutput && !canFollowApplicationOutput())
        mode = FollowMode::NewestRun;
    if (mode == FollowMode::Pinned || mode == m_followMode)
        return;
    m_followMode = mode;

    // Catch up with what the new mode would have chosen: the run whose tab Application Output
    // shows, or the newest live run. Nothing live: keep what is on screen.
    RunControl *wanted = nullptr;
#if MULTIFILTEROUTPUT_QTC21
    if (mode == FollowMode::ApplicationOutput) {
        for (RunControl *runControl : std::as_const(m_liveOrder)) {
            if (runControl->isOutputVisible())
                wanted = runControl;
        }
    }
#endif
    if (!wanted && !m_liveOrder.isEmpty())
        wanted = m_liveOrder.last();
    if (wanted && wanted != m_active)
        activateRunControl(wanted, /*isSwitch=*/true);
    else
        emit runsChanged();
}

void RunTracker::pinRunControl(RunControl *runControl)
{
    if (!m_runs.contains(runControl))
        return;
    m_followMode = FollowMode::Pinned;
    if (runControl != m_active)
        activateRunControl(runControl, /*isSwitch=*/true);
    else
        emit runsChanged();
}

QList<RunControl *> RunTracker::liveRunControls() const
{
    return m_liveOrder;
}

QList<RunTracker::HistoryLine> RunTracker::history(RunControl *runControl) const
{
    const auto it = m_runs.constFind(runControl);
    return it == m_runs.constEnd() ? QList<HistoryLine>() : it->history;
}

void RunTracker::setHistoryLimit(qsizetype characters)
{
    m_historyLimit = qMax<qsizetype>(0, characters);
}

QList<RunControl *> RunTracker::finishedRunControls() const
{
    return m_finishedOrder;
}

void RunTracker::setFinishedRunsKept(int count)
{
    count = qMax(0, count);
    if (count == m_finishedRunsKept)
        return;
    m_finishedRunsKept = count;
    trimFinishedRuns();
    emit runsChanged();
}

void RunTracker::trimFinishedRuns()
{
    while (m_finishedOrder.size() > m_finishedRunsKept) {
        RunControl *runControl = m_finishedOrder.takeFirst();
        m_runs.remove(runControl);
        // The run on screen still needs its destroyed connection (see untrackRunControl()).
        if (runControl != m_active)
            disconnect(runControl, nullptr, this, nullptr);
    }
}

bool RunTracker::lastActivationWasSwitch() const
{
    return m_lastActivationWasSwitch;
}

void RunTracker::addToHistory(LiveRun &run, const OutputLineSplitter::Line &line)
{
    run.history.append({line.text, line.format, QDateTime::currentMSecsSinceEpoch()});
    run.historyCharacters += line.text.size();
    while (run.historyCharacters > m_historyLimit && !run.history.isEmpty())
        run.historyCharacters -= run.history.takeFirst().text.size();
}

void RunTracker::trackRunControl(RunControl *runControl)
{
    if (!runControl)
        return;
    if (const auto it = m_runs.constFind(runControl); it != m_runs.constEnd()) {
        if (it->running)
            return;
        // A Re-run of a finished run whose output was kept: a new run, with fresh history.
        forgetRunControl(runControl);
    }

    // A Re-run tracks the same RunControl again; untrackRunControl() left its destroyed
    // connection in place if it was the active run, so drop that rather than stack another.
    disconnect(runControl, nullptr, this, nullptr);
    m_runs.insert(runControl, LiveRun());
    m_liveOrder.append(runControl);

    connect(
        runControl,
        &RunControl::appendMessage,
        this,
        [this, runControl](const QString &msg, Utils::OutputFormat format) {
            LiveRun &run = m_runs[runControl];
            const QList<OutputLineSplitter::Line> lines = run.splitter.feed(msg, format);
            // Every run's lines are kept, so switching to it later can show them.
            for (const OutputLineSplitter::Line &line : lines)
                addToHistory(run, line);
            if (runControl != m_active)
                return;
            for (const OutputLineSplitter::Line &line : lines)
                emit activeRunLineReceived(line.text, line.format);
        });

#if MULTIFILTEROUTPUT_QTC21
    // Application Output shows this run's tab (and is itself visible).
    connect(runControl, &RunControl::outputVisibilityChanged, this, [this, runControl](bool visible) {
        if (visible && m_followMode == FollowMode::ApplicationOutput && runControl != m_active)
            activateRunControl(runControl, /*isSwitch=*/true);
    });
#endif

    connect(runControl, &QObject::destroyed, this, [this, runControl] {
        handleRunControlDestroyed(runControl);
    });

    // A pinned run keeps the pane -- unless this is that run being Re-run.
    if (m_followMode != FollowMode::Pinned || runControl == m_active)
        activateRunControl(runControl);
    else
        emit runsChanged();
}

void RunTracker::untrackRunControl(RunControl *runControl)
{
    if (!runControl)
        return;

    const auto it = m_runs.find(runControl);
    if (it != m_runs.end() && !it->running)
        return; // already stopped

    // The run's last line need not end in a newline (a crash message, a final printf with
    // no "\n"), in which case it is still sitting in the buffer; emit (and keep) it now
    // rather than lose it with the buffer.
    if (it != m_runs.end()) {
        const QList<OutputLineSplitter::Line> pending = it->splitter.flush();
        for (const OutputLineSplitter::Line &line : pending)
            addToHistory(*it, line);
        if (runControl == m_active) {
            for (const OutputLineSplitter::Line &line : pending)
                emit activeRunLineReceived(line.text, line.format);
        }
    }

    // Drop every connection this tracker installed on the now-stopped run. Without this a
    // stray late appendMessage() would re-create the run's hash entry via operator[] on a
    // run we have deliberately stopped buffering for.
    disconnect(runControl, nullptr, this, nullptr);
    const bool keep = it != m_runs.end() && m_finishedRunsKept > 0;
    if (keep) {
        it->running = false;
        m_liveOrder.removeOne(runControl);
        m_finishedOrder.append(runControl);
        trimFinishedRuns();
    } else {
        forgetRunControl(runControl);
    }

    // Deliberately do NOT touch m_active here: a run that has merely stopped keeps its
    // tabs on screen (like Application Output does) until a different run starts or the
    // RunControl itself is destroyed. The destroyed connection just dropped above is the
    // one thing we still need, so re-establish it for the run we are still displaying -- and
    // for a finished run whose output is kept, to forget it when it goes.
    if (m_active == runControl || m_runs.contains(runControl)) {
        connect(runControl, &QObject::destroyed, this, [this, runControl] {
            handleRunControlDestroyed(runControl);
        });
    }
    emit runsChanged();
}

void RunTracker::forgetRunControl(RunControl *runControl)
{
    m_runs.remove(runControl);
    m_liveOrder.removeOne(runControl);
    m_finishedOrder.removeOne(runControl);
}

void RunTracker::handleRunControlDestroyed(RunControl *runControl)
{
    forgetRunControl(runControl);
    if (m_active == runControl) {
        // Nothing left to stay pinned to: let the next run take over.
        if (m_followMode == FollowMode::Pinned)
            m_followMode = FollowMode::NewestRun;
        m_active = nullptr;
        m_lastActivationWasSwitch = false;
        emit activeRunChanged(nullptr);
    }
    emit runsChanged();
}

void RunTracker::activateRunControl(RunControl *runControl, bool isSwitch)
{
    // Deliberately no "already active" early-out: Application Output's Re-run restarts the
    // same RunControl, which is still the active one (a stopped run stays latched), and it
    // must reset the pane just like any newly started run -- as Application Output itself
    // treats a Re-run like a new run (AppOutputPane::handleOldOutput() for both).
    m_active = runControl;
    m_lastActivationWasSwitch = isSwitch;
    emit activeRunChanged(m_active);
    emit runsChanged();
}

} // namespace MultiFilterOutput

#include "runtracker.h"

#include <projectexplorer/projectexplorer.h>
#include <projectexplorer/runcontrol.h>

using namespace ProjectExplorer;

namespace MultiFilterOutput {

RunTracker::RunTracker(QObject *parent)
    : QObject(parent)
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

void RunTracker::trackRunControl(RunControl *runControl)
{
    if (!runControl || m_buffers.contains(runControl))
        return;

    m_buffers.insert(runControl, LineBuffer());

    connect(
        runControl,
        &RunControl::appendMessage,
        this,
        [this, runControl](const QString &msg, Utils::OutputFormat format) {
            const QStringList lines = m_buffers[runControl].feed(msg);
            if (runControl != m_active)
                return;
            for (const QString &line : lines)
                emit activeRunLineReceived(line, format);
        });

    connect(runControl, &RunControl::outputVisibilityChanged, this, [this](bool) {
        updateActiveRunControl();
    });

    connect(runControl, &QObject::destroyed, this, [this, runControl] {
        handleRunControlDestroyed(runControl);
    });

    updateActiveRunControl();
}

void RunTracker::untrackRunControl(RunControl *runControl)
{
    if (!runControl)
        return;

    // Drop every connection this tracker installed on the now-stopped run. Without this a
    // stray late appendMessage() would re-create the run's hash entry via operator[] on a
    // run we have deliberately stopped buffering for.
    disconnect(runControl, nullptr, this, nullptr);
    m_buffers.remove(runControl);

    // Deliberately do NOT touch m_active or re-run updateActiveRunControl() here: a run
    // that has merely stopped keeps its tabs on screen (like Application Output does)
    // until a genuinely different run takes over or the RunControl itself is destroyed.
    // The destroyed connection just dropped above is the one thing we still need, so
    // re-establish it for the run we are still displaying.
    if (m_active == runControl) {
        connect(runControl, &QObject::destroyed, this, [this, runControl] {
            handleRunControlDestroyed(runControl);
        });
    }
}

void RunTracker::handleRunControlDestroyed(RunControl *runControl)
{
    m_buffers.remove(runControl);
    if (m_active == runControl) {
        m_active = nullptr;
        emit activeRunChanged(nullptr);
    }
}

void RunTracker::updateActiveRunControl()
{
    RunControl *newActive = nullptr;
    for (auto it = m_buffers.constBegin(); it != m_buffers.constEnd(); ++it) {
        if (it.key()->isOutputVisible()) {
            newActive = it.key();
            break;
        }
    }

    // No tracked run currently reports "visible" -- e.g. the user switched the bottom
    // panel to our own pane, which hides AppOutputPane and clears every run's visibility
    // flag, or the run we are following has stopped and been untracked. Keep following the
    // run we were already tracking rather than blanking the pane; only
    // handleRunControlDestroyed() ever clears m_active.
    if (!newActive && m_active)
        return;

    if (newActive != m_active) {
        m_active = newActive;
        emit activeRunChanged(m_active);
    }
}

} // namespace MultiFilterOutput

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

    m_buffers.insert(runControl, RunOutput());

    connect(
        runControl,
        &RunControl::appendMessage,
        this,
        [this, runControl](const QString &msg, Utils::OutputFormat format) {
            RunOutput &output = m_buffers[runControl];
            output.lastFormat = format;
            const QStringList lines = output.buffer.feed(msg);
            if (runControl != m_active)
                return;
            for (const QString &line : lines)
                emit activeRunLineReceived(line, format);
        });

    connect(runControl, &QObject::destroyed, this, [this, runControl] {
        handleRunControlDestroyed(runControl);
    });

    activateRunControl(runControl);
}

void RunTracker::untrackRunControl(RunControl *runControl)
{
    if (!runControl)
        return;

    // The run's last line need not end in a newline (a crash message, a final printf with
    // no "\n"), in which case it is still sitting in the buffer; emit it now rather than
    // drop it along with the buffer below.
    if (const auto it = m_buffers.find(runControl); it != m_buffers.end()) {
        const QString partial = it->buffer.flush();
        if (!partial.isEmpty() && runControl == m_active)
            emit activeRunLineReceived(partial, it->lastFormat);
    }

    // Drop every connection this tracker installed on the now-stopped run. Without this a
    // stray late appendMessage() would re-create the run's hash entry via operator[] on a
    // run we have deliberately stopped buffering for.
    disconnect(runControl, nullptr, this, nullptr);
    m_buffers.remove(runControl);

    // Deliberately do NOT touch m_active here: a run that has merely stopped keeps its
    // tabs on screen (like Application Output does) until a different run starts or the
    // RunControl itself is destroyed. The destroyed connection just dropped above is the
    // one thing we still need, so re-establish it for the run we are still displaying.
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

void RunTracker::activateRunControl(RunControl *runControl)
{
    if (runControl == m_active)
        return;

    m_active = runControl;
    emit activeRunChanged(m_active);
}

} // namespace MultiFilterOutput

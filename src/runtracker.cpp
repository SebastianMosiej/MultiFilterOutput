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
        untrackRunControl(runControl);
    });

    updateActiveRunControl();
}

void RunTracker::untrackRunControl(RunControl *runControl)
{
    m_buffers.remove(runControl);
    if (m_active == runControl)
        updateActiveRunControl();
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
    if (newActive != m_active) {
        m_active = newActive;
        emit activeRunChanged(m_active);
    }
}

} // namespace MultiFilterOutput

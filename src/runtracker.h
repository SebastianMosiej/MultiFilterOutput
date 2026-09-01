#pragma once

#include "linebuffer.h"
#include "multifilteroutput_global.h"

#include <utils/outputformat.h>

#include <QHash>
#include <QObject>

namespace ProjectExplorer {
class RunControl;
}

namespace MultiFilterOutput {

class MULTIFILTEROUTPUT_EXPORT RunTracker : public QObject
{
    Q_OBJECT

public:
    explicit RunTracker(QObject *parent = nullptr);

    // The run whose output this tracker is currently following, or nullptr.
    //
    // Note that this is NOT simply "the run reporting isOutputVisible() == true":
    // isOutputVisible() means "this run's tab is current AND Application Output is the
    // visible bottom pane", so it goes false for *every* run the moment the user switches
    // the bottom panel to our own pane. The tracker therefore latches: a run stays active
    // until a genuinely different run reports itself visible, or until the RunControl
    // object is destroyed. A run that has merely stopped keeps its tabs up, matching
    // Application Output's own behavior.
    ProjectExplorer::RunControl *activeRunControl() const;

signals:
    // Emitted whenever activeRunControl() changes (including to/from nullptr).
    void activeRunChanged(ProjectExplorer::RunControl *runControl);
    // Emitted for each complete line from the currently-active run only.
    void activeRunLineReceived(const QString &line, Utils::OutputFormat format);

private:
    void trackRunControl(ProjectExplorer::RunControl *runControl);
    void untrackRunControl(ProjectExplorer::RunControl *runControl);
    void handleRunControlDestroyed(ProjectExplorer::RunControl *runControl);
    void updateActiveRunControl();

    QHash<ProjectExplorer::RunControl *, LineBuffer> m_buffers;
    ProjectExplorer::RunControl *m_active = nullptr;
};

} // namespace MultiFilterOutput

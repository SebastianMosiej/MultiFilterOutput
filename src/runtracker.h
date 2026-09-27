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
    // Qt Creator 20.0.1 exposes no public "this run's Application Output tab is the one
    // currently visible" signal (that landed only in later dev snapshots as
    // RunControl::isOutputVisible()/outputVisibilityChanged, and AppOutputPane's own
    // notion of the current tab is a private implementation detail). We approximate it
    // instead: a run becomes active the moment it starts, mirroring Application Output's
    // default behavior of switching focus to a freshly started run. The tracker then
    // latches onto that run until either a different run starts or the RunControl object
    // is destroyed -- a run that has merely stopped keeps its tabs up. This will NOT
    // notice the user manually switching Application Output back to view an older,
    // already-finished run's tab.
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
    void activateRunControl(ProjectExplorer::RunControl *runControl);

    struct RunOutput
    {
        LineBuffer buffer;
        // The format of the most recent chunk -- what a trailing partial line still sitting
        // in `buffer` when the run stops gets emitted with (see untrackRunControl()).
        Utils::OutputFormat lastFormat = Utils::StdOutFormat;
    };

    QHash<ProjectExplorer::RunControl *, RunOutput> m_buffers;
    ProjectExplorer::RunControl *m_active = nullptr;
};

} // namespace MultiFilterOutput

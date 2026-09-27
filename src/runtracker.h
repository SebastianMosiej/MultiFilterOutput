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

    // Starts following Qt Creator's runs: tracks every run already in progress, then each
    // one ProjectExplorerPlugin reports as started or stopped. Separate from the constructor
    // so that a tracker can exist -- and be tested -- without a running ProjectExplorer.
    void attachToProjectExplorer();

    // What ProjectExplorerPlugin::runControlStarted / runControlStoped feed (see
    // attachToProjectExplorer()); public so tests can drive the tracker directly.
    // A started run becomes the active one. A stopped run stops being listened to but stays
    // active (its tabs stay up) until another run starts or it is destroyed.
    void trackRunControl(ProjectExplorer::RunControl *runControl);
    void untrackRunControl(ProjectExplorer::RunControl *runControl);

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
    void handleRunControlDestroyed(ProjectExplorer::RunControl *runControl);
    void activateRunControl(ProjectExplorer::RunControl *runControl);

    QHash<ProjectExplorer::RunControl *, OutputLineSplitter> m_buffers;
    ProjectExplorer::RunControl *m_active = nullptr;
};

} // namespace MultiFilterOutput

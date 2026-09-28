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
    // A started run becomes the active one (unless another is pinned, see FollowMode). A
    // stopped run stops being listened to but stays active (its tabs stay up) until another
    // run takes over or it is destroyed.
    void trackRunControl(ProjectExplorer::RunControl *runControl);
    void untrackRunControl(ProjectExplorer::RunControl *runControl);

    // The run whose output this tracker is currently following, or nullptr.
    //
    // By default a run becomes active the moment it starts, mirroring Application Output
    // switching to a freshly started run, and stays active -- even once stopped -- until
    // another run takes over or the RunControl is destroyed. FollowMode changes which run
    // takes over. Only live runs can be switched to: a stopped run's output is dropped
    // (Application Output keeps finished runs' tabs; this pane shows one run at a time).
    ProjectExplorer::RunControl *activeRunControl() const;

    // Which run is the active one:
    //  - NewestRun: each run that starts takes over (the default, as above);
    //  - ApplicationOutput: the same, and also whichever run's tab the user selects in
    //    Application Output (RunControl::outputVisibilityChanged);
    //  - Pinned: the run chosen with pinRunControl() stays, even as newer runs start. If it
    //    goes away, the tracker falls back to NewestRun.
    enum class FollowMode { NewestRun, ApplicationOutput, Pinned };
    FollowMode followMode() const;
    // NewestRun or ApplicationOutput; for Pinned use pinRunControl().
    void setFollowMode(FollowMode mode);
    // Makes a live run the active one and keeps it so (FollowMode::Pinned).
    void pinRunControl(ProjectExplorer::RunControl *runControl);

    // The runs being listened to -- started and not yet stopped -- oldest first.
    QList<ProjectExplorer::RunControl *> liveRunControls() const;

    // Every live run's recent output, active or not, so switching to a run can show what it
    // printed before the switch. Bounded per run by setHistoryLimit() (oldest lines go
    // first); dropped when the run stops.
    struct HistoryLine
    {
        QString text;
        Utils::OutputFormat format;
        qint64 receivedAt;
    };
    QList<HistoryLine> history(ProjectExplorer::RunControl *runControl) const;
    void setHistoryLimit(qsizetype characters);

    // Whether the latest activeRunChanged() was a switch to an already-running run (a pin, or
    // following Application Output) rather than a run starting.
    bool lastActivationWasSwitch() const;

signals:
    // Emitted whenever a run becomes the active one -- including the already-active run
    // starting again (Application Output's Re-run) -- and with nullptr when the active run
    // goes away. Either way, what the pane shows belongs to a new run from here on.
    void activeRunChanged(ProjectExplorer::RunControl *runControl);
    // Emitted for each complete line from the currently-active run only.
    void activeRunLineReceived(const QString &line, Utils::OutputFormat format);
    // The live runs, the active one or the follow mode changed (see liveRunControls()).
    void runsChanged();

private:
    void handleRunControlDestroyed(ProjectExplorer::RunControl *runControl);
    // `isSwitch`: see lastActivationWasSwitch().
    void activateRunControl(ProjectExplorer::RunControl *runControl, bool isSwitch = false);
    void forgetRunControl(ProjectExplorer::RunControl *runControl);

    struct LiveRun
    {
        OutputLineSplitter splitter;
        QList<HistoryLine> history;
        qsizetype historyCharacters = 0;
    };
    void addToHistory(LiveRun &run, const OutputLineSplitter::Line &line);

    QHash<ProjectExplorer::RunControl *, LiveRun> m_runs;
    QList<ProjectExplorer::RunControl *> m_liveOrder; // oldest first
    ProjectExplorer::RunControl *m_active = nullptr;
    FollowMode m_followMode = FollowMode::NewestRun;
    bool m_lastActivationWasSwitch = false;
    qsizetype m_historyLimit = 10'000'000;
};

} // namespace MultiFilterOutput

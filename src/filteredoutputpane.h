#pragma once

#include "linematcher.h"
#include "pattern.h"
#include "runtracker.h"

#include <coreplugin/ioutputpane.h>

#include <QList>

QT_BEGIN_NAMESPACE
class QLabel;
QT_END_NAMESPACE

namespace ProjectExplorer {
class RunControl;
}

namespace MultiFilterOutput {
class LineQueue;
class OutputPaneTabWidget;
class OutputTabWidget;
}

namespace MultiFilterOutput::Internal {

class FilteredOutputPane final : public Core::IOutputPane
{
    Q_OBJECT

public:
    explicit FilteredOutputPane(RunTracker *runTracker, QObject *parent = nullptr);

    QWidget *outputWidget(QWidget *parent) override;
    QList<QWidget *> toolBarWidgets() const override;
    void clearContents() override;
    void setFocus() override;
    bool hasFocus() const override;
    bool canFocus() const override;
    bool canNavigate() const override;
    bool canNext() const override;
    bool canPrevious() const override;
    void goToNext() override;
    void goToPrev() override;

private:
    void rebuildTabs(ProjectExplorer::RunControl *activeRunControl);
    void onLineReceived(const QString &line, Utils::OutputFormat format);
    // Appends one line to "All" and to every pattern tab it belongs in; driven by
    // m_lineQueue, never directly from onLineReceived().
    void processLine(const QString &line, Utils::OutputFormat format);
    void onJumpRequested(int sourceLineId);
    void updateTabLabel(const Pattern &pattern, OutputTabWidget *tab, int count);
    // Matches the tab widget's pane background to OutputPaneTabBar's own painted background
    // (see outputpanetabbar.h) so nothing native shows through around the tab bar.
    void styleTabBar();
    // Keeps every tab in this pane at the same zoom level: applied whenever a tab is
    // (re)created, and re-broadcast to every other tab (and persisted) whenever any one tab's
    // own wheelEvent() changes it.
    void onTabZoomChanged(qreal zoom);

    struct PatternTab
    {
        Pattern pattern;
        OutputTabWidget *tab;
        // True while lines are being swept in as continuations of this pattern's last match
        // (see Pattern::groupContinuationLines); cleared by the first line that doesn't
        // match pattern.continuationPattern.
        bool inContinuation = false;
        // Stops evaluating this pattern for the rest of the run once its matching has proven
        // pathologically slow (see SlowMatchBreaker); fresh for every run, since the tabs --
        // and so these entries -- are rebuilt per run.
        SlowMatchBreaker breaker = SlowMatchBreaker();
    };

    // Runs one pattern's matching for one line (continuation first, then the main pattern)
    // and appends the line to its tab if it belongs there.
    void matchIntoTab(
        PatternTab &entry, const QString &line, Utils::OutputFormat format, int allLine);
    // Called once, when entry's breaker trips: relabels the tab and says why in General
    // Messages.
    void reportTooSlow(const PatternTab &entry);

    RunTracker *m_runTracker;
    LineQueue *m_lineQueue = nullptr;
    QWidget *m_container = nullptr;
    OutputPaneTabWidget *m_tabWidget = nullptr;
    OutputTabWidget *m_allTab = nullptr;
    QList<PatternTab> m_patternTabs;
    QLabel *m_runLabel = nullptr;
    qreal m_fontZoom = 0;
    // Set whenever a new run becomes active; cleared the moment this pane pops itself for
    // that run's first line of output (see onLineReceived()) -- so at most one pop per run.
    bool m_popOnNextLine = false;
};

} // namespace MultiFilterOutput::Internal

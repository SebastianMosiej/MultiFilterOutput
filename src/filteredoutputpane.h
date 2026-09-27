#pragma once

#include "linerouter.h"
#include "outputtabwidget.h"
#include "pattern.h"
#include "runtracker.h"

#include <coreplugin/ioutputpane.h>

#include <QList>

QT_BEGIN_NAMESPACE
class QLabel;
class QToolButton;
QT_END_NAMESPACE

namespace ProjectExplorer {
class RunControl;
}

namespace MultiFilterOutput {
class LineQueue;
class OutputPaneTabWidget;
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
    // The "Filter output..." box: filters the current tab, like Application Output's.
    bool hasFilterContext() const override;
    void updateFilter() override;
    bool canNext() const override;
    bool canPrevious() const override;
    void goToNext() override;
    void goToPrev() override;

private:
    void rebuildTabs(ProjectExplorer::RunControl *activeRunControl);
    OutputTabWidget *currentTab() const;
    // Toolbar: pane-wide zoom by `points` (zoom buttons and shortcuts), copy and save the
    // current tab, and the word-wrap setting applied to a tab.
    void zoomBy(qreal points);
    void copyCurrentTab();
    void saveCurrentTab();
    void applyWordWrap(OutputTabWidget *tab);
    // Turns the quick filter into a Pattern Library entry.
    void saveFilterAsPattern();
    // Steps the current tab to its next/previous match and keeps All at the same line.
    void goToMatch(OutputTabWidget::Direction direction);
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
    // Re-points m_pidConnection at the newly active run, then refreshes the toolbar label.
    void watchRunLabel(ProjectExplorer::RunControl *activeRunControl);
    void updateRunLabel();

    struct PatternTab
    {
        Pattern pattern;
        OutputTabWidget *tab;
    };

    // Called once, when entry's pattern trips its slow-match breaker in m_router: relabels
    // the tab and says why in General Messages.
    void reportTooSlow(const PatternTab &entry);
    // Called by m_lineQueue, in order, where it dropped backlog to bound memory: says so in
    // every tab.
    void reportDiscarded(int lines);

    RunTracker *m_runTracker;
    LineQueue *m_lineQueue = nullptr;
    QWidget *m_container = nullptr;
    OutputPaneTabWidget *m_tabWidget = nullptr;
    OutputTabWidget *m_allTab = nullptr;
    QList<PatternTab> m_patternTabs;
    // Which of m_patternTabs each line goes to (same indexes); rebuilt with them per run.
    LineRouter m_router;
    QLabel *m_runLabel = nullptr;
    QToolButton *m_wrapButton = nullptr;
    QToolButton *m_copyButton = nullptr;
    QToolButton *m_saveButton = nullptr;
    QToolButton *m_saveFilterButton = nullptr;
    // The active run's applicationProcessHandleChanged -> updateRunLabel() connection.
    QMetaObject::Connection m_pidConnection;
    qreal m_fontZoom = 0;
    // Set whenever a new run becomes active; cleared the moment this pane pops itself for
    // that run's first line of output (see onLineReceived()) -- so at most one pop per run.
    bool m_popOnNextLine = false;
};

} // namespace MultiFilterOutput::Internal

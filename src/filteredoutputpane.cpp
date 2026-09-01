#include "filteredoutputpane.h"

#include "filtersetaspect.h"
#include "linematcher.h"
#include "multifilteroutputconstants.h"
#include "multifilteroutputtr.h"
#include "outputpanesettings.h"
#include "outputtabwidget.h"
#include "patternlibrary.h"
#include "tablabel.h"

#include <projectexplorer/project.h>
#include <projectexplorer/runcontrol.h>

#include <QLabel>
#include <QTabBar>
#include <QTabWidget>
#include <QVBoxLayout>

using namespace ProjectExplorer;

namespace MultiFilterOutput::Internal {

FilteredOutputPane::FilteredOutputPane(RunTracker *runTracker, QObject *parent)
    : Core::IOutputPane(parent)
    , m_runTracker(runTracker)
{
    setId(Constants::OUTPUT_PANE_ID);
    setDisplayName(Tr::tr("MultiFilterOutput"));
    // A negative priority hides the pane's status-bar button by default; the real panes use
    // Issues 100, Search Results 80, Application Output 60, Compile Output 40, Terminal 20,
    // Todo 10 (the lowest built-in priority that is still shown by default), so 50 puts our
    // button next to Application Output, and 0 puts it after every built-in default-visible
    // pane -- see OutputPaneSettings.
    setPriorityInStatusBar(
        outputPaneSettings().placement() == OutputPanePlacement::AtEnd ? 0 : 50);

    RunTracker *tracker = m_runTracker;
    connect(tracker, &RunTracker::activeRunChanged, this, &FilteredOutputPane::rebuildTabs);
    connect(tracker, &RunTracker::activeRunLineReceived, this, &FilteredOutputPane::onLineReceived);
}

QWidget *FilteredOutputPane::outputWidget(QWidget *parent)
{
    if (!m_container) {
        m_container = new QWidget(parent);
        m_tabWidget = new QTabWidget(m_container);

        m_allTab = new OutputTabWidget(m_tabWidget);
        m_tabWidget->addTab(m_allTab, Tr::tr("All"));

        auto *layout = new QVBoxLayout(m_container);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->addWidget(m_tabWidget);

        rebuildTabs(m_runTracker->activeRunControl());
    }
    return m_container;
}

QList<QWidget *> FilteredOutputPane::toolBarWidgets() const
{
    if (!m_runLabel)
        const_cast<FilteredOutputPane *>(this)->m_runLabel = new QLabel(Tr::tr("No active run"));
    return {m_runLabel};
}

void FilteredOutputPane::rebuildTabs(RunControl *activeRunControl)
{
    if (!m_tabWidget)
        return;

    for (const auto &pair : std::as_const(m_patternTabs)) {
        const int index = m_tabWidget->indexOf(pair.second);
        if (index != -1)
            m_tabWidget->removeTab(index);
        pair.second->deleteLater();
    }
    m_patternTabs.clear();
    // reset(), not clear(): the inherited QPlainTextEdit::clear() leaves the match count,
    // the source-block mapping and the formatter's carry-over buffer stale.
    m_allTab->reset();

    if (m_runLabel) {
        m_runLabel->setText(
            activeRunControl ? Tr::tr("Filtering: %1 (pid %2)")
                                   .arg(activeRunControl->displayName())
                                   .arg(activeRunControl->applicationProcessHandle().pid())
                             : Tr::tr("No active run"));
    }

    if (!activeRunControl)
        return;

    Project *project = activeRunControl->project();
    m_allTab->setLinkParser(project);

    // RunControl (v20.0.82) does not retain a live RunConfiguration* of its own -- it only
    // copies data out of one at start time (copyDataFromRunConfiguration), snapshotting each
    // aspect's Data into a stable, launch-time-frozen copy. Reading through the current
    // BuildConfiguration's activeRunConfiguration() instead would pick up whichever run
    // configuration happens to be active *now*, not the one this run was actually launched
    // from -- so we must read the RunControl's own snapshot here.
    const auto *filterData = activeRunControl->aspectData<FilterSetAspect>();
    const QStringList enabledIds = filterData ? filterData->value.toStringList() : QStringList();

    for (const QString &id : enabledIds) {
        const auto stored = patternLibrary().patternById(id);
        if (!stored)
            continue; // pattern deleted since selection was saved -- skip silently
        const Pattern pattern = *stored;

        auto *tab = new OutputTabWidget(m_tabWidget);
        tab->setLinkParser(project);
        connect(tab, &OutputTabWidget::matchCountChanged, this, [this, pattern, tab](int count) {
            updateTabLabel(pattern, tab, count);
        });
        connect(tab, &OutputTabWidget::jumpRequested, this, &FilteredOutputPane::onJumpRequested);

        m_patternTabs.append({pattern, tab});
        m_tabWidget->addTab(tab, tabLabel(pattern, 0));
        updateTabLabel(pattern, tab, 0);
    }
}

void FilteredOutputPane::updateTabLabel(const Pattern &pattern, OutputTabWidget *tab, int count)
{
    if (!m_tabWidget)
        return;
    const int index = m_tabWidget->indexOf(tab);
    if (index == -1)
        return;
    m_tabWidget->setTabText(index, tabLabel(pattern, count));
    // Pattern::color is user-editable and persisted; this is where it becomes visible.
    if (pattern.color.isValid())
        m_tabWidget->tabBar()->setTabTextColor(index, pattern.color);
}

void FilteredOutputPane::onJumpRequested(int sourceBlockNumber)
{
    if (!m_tabWidget || !m_allTab || sourceBlockNumber < 0)
        return;
    // Switching to the "All" tab is half the point: scrolling a tab the user cannot see
    // looks like nothing happened at all.
    m_tabWidget->setCurrentWidget(m_allTab);
    m_allTab->scrollToBlock(sourceBlockNumber);
}

void FilteredOutputPane::onLineReceived(const QString &line, Utils::OutputFormat format)
{
    if (!m_allTab)
        return;

    // The "All" tab is the jump target, so its block number for this line is what every
    // matching pattern tab must record -- not each tab's own (identity) block index.
    const int allBlock = m_allTab->appendLine(line, format);

    for (const auto &pair : std::as_const(m_patternTabs)) {
        if (lineMatches(pair.first, line))
            pair.second->appendLine(line, format, allBlock);
    }
}

void FilteredOutputPane::clearContents()
{
    if (m_allTab)
        m_allTab->reset();
    for (const auto &pair : std::as_const(m_patternTabs))
        pair.second->reset();
}

void FilteredOutputPane::setFocus()
{
    if (m_tabWidget && m_tabWidget->currentWidget())
        m_tabWidget->currentWidget()->setFocus();
}

bool FilteredOutputPane::hasFocus() const
{
    return m_tabWidget && m_tabWidget->currentWidget() && m_tabWidget->currentWidget()->hasFocus();
}

bool FilteredOutputPane::canFocus() const
{
    return m_tabWidget && m_tabWidget->currentWidget();
}

bool FilteredOutputPane::canNavigate() const
{
    return false;
}

bool FilteredOutputPane::canNext() const
{
    return false;
}

bool FilteredOutputPane::canPrevious() const
{
    return false;
}

void FilteredOutputPane::goToNext() {}

void FilteredOutputPane::goToPrev() {}

} // namespace MultiFilterOutput::Internal

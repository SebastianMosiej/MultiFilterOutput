#include "filteredoutputpane.h"

#include "filtersetaspect.h"
#include "linematcher.h"
#include "linequeue.h"
#include "multifilteroutputconstants.h"
#include "multifilteroutputtr.h"
#include "outputpanesettings.h"
#include "outputpanetabbar.h"
#include "outputtabwidget.h"
#include "patternlibrary.h"
#include "tablabel.h"

#include <coreplugin/icore.h>
#include <coreplugin/messagemanager.h>

#include <projectexplorer/project.h>
#include <projectexplorer/runcontrol.h>

#include <utils/qtcsettings.h>
#include <utils/theme/theme.h>

#include <QLabel>
#include <QTabBar>
#include <QVBoxLayout>

#include <algorithm>

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

    Utils::QtcSettings *settings = Core::ICore::settings();
    settings->beginGroup(Constants::SETTINGS_GROUP);
    m_fontZoom = settings->value(Constants::OUTPUT_FONT_ZOOM_KEY, 0.0).toDouble();
    settings->endGroup();

    m_lineQueue = new LineQueue(
        [this](const QString &line, Utils::OutputFormat format) { processLine(line, format); },
        std::chrono::milliseconds(10),
        this);

    RunTracker *tracker = m_runTracker;
    connect(tracker, &RunTracker::activeRunChanged, this, &FilteredOutputPane::rebuildTabs);
    connect(tracker, &RunTracker::activeRunLineReceived, this, &FilteredOutputPane::onLineReceived);

    // One limit for every tab, changed live from the Pattern Library settings page.
    connect(
        &outputPaneSettings(),
        &OutputPaneSettings::maxDisplayedLineLengthChanged,
        this,
        [this](int length) {
            if (m_allTab)
                m_allTab->setMaxLineLength(length);
            for (const PatternTab &entry : std::as_const(m_patternTabs))
                entry.tab->setMaxLineLength(length);
        });
}

QWidget *FilteredOutputPane::outputWidget(QWidget *parent)
{
    if (!m_container) {
        m_container = new QWidget(parent);
        m_tabWidget = new OutputPaneTabWidget(m_container);
        styleTabBar();

        m_allTab = new OutputTabWidget(m_tabWidget);
        m_allTab->setFontZoom(m_fontZoom);
        m_allTab->setMaxLineLength(outputPaneSettings().maxDisplayedLineLength());
        connect(m_allTab, &OutputTabWidget::fontZoomChanged, this, &FilteredOutputPane::onTabZoomChanged);
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

    for (const PatternTab &entry : std::as_const(m_patternTabs)) {
        const int index = m_tabWidget->indexOf(entry.tab);
        if (index != -1)
            m_tabWidget->removeTab(index);
        entry.tab->deleteLater();
    }
    m_patternTabs.clear();
    // Lines still queued belong to the run being replaced (or to the tab set being torn
    // down); they must not land in the fresh tabs.
    m_lineQueue->clear();
    // reset(), not clear(): the inherited QPlainTextEdit::clear() leaves the match count,
    // the source-block mapping and the formatter's carry-over buffer stale.
    m_allTab->reset();

    // Arm the run-start pop for this run's first line (see onLineReceived()); a run going
    // away (activeRunControl == nullptr) has no "first line" left to pop on.
    m_popOnNextLine = activeRunControl != nullptr;

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

    // Library order, not enabledIds order: the latter is just the order the boxes were
    // ticked in, while the library's is the one the user deliberately arranges. Patterns
    // deleted since the selection was saved are skipped silently.
    for (const Pattern &pattern : patternLibrary().patternsWithIds(enabledIds)) {
        auto *tab = new OutputTabWidget(m_tabWidget);
        tab->setLinkParser(project);
        tab->setFontZoom(m_fontZoom);
        tab->setMaxLineLength(outputPaneSettings().maxDisplayedLineLength());
        connect(tab, &OutputTabWidget::matchCountChanged, this, [this, pattern, tab](int count) {
            updateTabLabel(pattern, tab, count);
        });
        connect(tab, &OutputTabWidget::jumpRequested, this, &FilteredOutputPane::onJumpRequested);
        connect(tab, &OutputTabWidget::fontZoomChanged, this, &FilteredOutputPane::onTabZoomChanged);

        m_patternTabs.append({pattern, tab});
        m_tabWidget->addTab(tab, tabLabel(pattern, 0));
        updateTabLabel(pattern, tab, 0);
    }
}

void FilteredOutputPane::styleTabBar()
{
    if (!m_tabWidget)
        return;

    // The tab bar itself (an OutputPaneTabBar) paints its own background and text; this just
    // matches the pane's own background -- visible in the thin strip around the tab bar -- to
    // the same color so nothing native shows through.
    const QColor background = Utils::creatorTheme()->color(Utils::Theme::PaletteBase);
    m_tabWidget->setStyleSheet(
        QStringLiteral("QTabWidget::pane { background: %1; border: none; }")
            .arg(background.name()));
}

void FilteredOutputPane::onTabZoomChanged(qreal zoom)
{
    if (zoom == m_fontZoom)
        return;
    m_fontZoom = zoom;

    if (m_allTab)
        m_allTab->setFontZoom(zoom);
    for (const PatternTab &entry : std::as_const(m_patternTabs))
        entry.tab->setFontZoom(zoom);

    Utils::QtcSettings *settings = Core::ICore::settings();
    settings->beginGroup(Constants::SETTINGS_GROUP);
    settings->setValue(Constants::OUTPUT_FONT_ZOOM_KEY, zoom);
    settings->endGroup();
    settings->sync();
}

void FilteredOutputPane::updateTabLabel(const Pattern &pattern, OutputTabWidget *tab, int count)
{
    if (!m_tabWidget)
        return;
    const int index = m_tabWidget->indexOf(tab);
    if (index == -1)
        return;
    const auto entry = std::find_if(
        m_patternTabs.cbegin(), m_patternTabs.cend(), [tab](const PatternTab &e) {
            return e.tab == tab;
        });
    const bool tooSlow = entry != m_patternTabs.cend() && entry->breaker.isTripped();
    m_tabWidget->setTabText(index, tabLabel(pattern, count, tooSlow));
    // Pattern::color is user-editable and persisted; this is where it becomes visible.
    if (pattern.color.isValid())
        m_tabWidget->tabBar()->setTabTextColor(index, pattern.color);
}

void FilteredOutputPane::onJumpRequested(int sourceLineId)
{
    if (!m_tabWidget || !m_allTab || sourceLineId < 0)
        return;
    if (!outputPaneSettings().jumpToAllOnClick())
        return;
    // Switching to the "All" tab is half the point: scrolling a tab the user cannot see
    // looks like nothing happened at all. But a line "All" has already trimmed off its front
    // has nothing to show, so stay put rather than switch to an unrelated position.
    if (m_allTab->scrollToLine(sourceLineId))
        m_tabWidget->setCurrentWidget(m_allTab);
}

void FilteredOutputPane::onLineReceived(const QString &line, Utils::OutputFormat format)
{
    if (!m_allTab)
        return;

    if (m_popOnNextLine) {
        m_popOnNextLine = false;
        // Mirrors Application Output's own default "pop on first output" trigger, so this
        // call -- reaching OutputPaneManager after Application Output's, since RunTracker
        // connects to RunControl::appendMessage later than AppOutputPane does -- is the one
        // that ends up deciding which pane is actually shown for this run.
        if (outputPaneSettings().focusPaneOnRunStart())
            popup(Core::IOutputPane::NoModeSwitch | Core::IOutputPane::WithFocus);
    }

    // The pop above has to happen right here, in step with Application Output's own
    // handling of this same signal; the per-line tab work (processLine()) is deferred to
    // m_lineQueue, which runs it in time-limited slices so a burst of output can't freeze
    // the GUI.
    m_lineQueue->enqueue(line, format);
}

void FilteredOutputPane::processLine(const QString &line, Utils::OutputFormat format)
{
    if (!m_allTab)
        return;

    // The "All" tab is the jump target, so its line id for this line is what every matching
    // pattern tab must record -- not an id of each tab's own.
    const int allLine = m_allTab->appendLine(line, format);

    for (PatternTab &entry : m_patternTabs) {
        if (entry.breaker.isTripped())
            continue;
        matchIntoTab(entry, line, format, allLine);
        if (entry.breaker.isTripped())
            reportTooSlow(entry);
    }
}

void FilteredOutputPane::matchIntoTab(
    PatternTab &entry, const QString &line, Utils::OutputFormat format, int allLine)
{
    // Both regexes -- the main one and the continuation one -- go through the breaker; either
    // can be the pathological one.
    if (entry.inContinuation) {
        if (entry.breaker.run([&] { return lineContinuesPattern(entry.pattern, line); })) {
            entry.tab->appendLine(line, format, allLine, /*countsAsMatch=*/false);
            return;
        }
        // This line breaks the continuation run; fall through and re-check it as an
        // ordinary candidate match below, same as any other line.
        entry.inContinuation = false;
    }
    if (entry.breaker.run([&] { return lineMatches(entry.pattern, line); })) {
        entry.tab->appendLine(line, format, allLine);
        entry.inContinuation = entry.pattern.groupContinuationLines
                               && !entry.pattern.continuationPattern.isEmpty();
    }
}

void FilteredOutputPane::reportTooSlow(const PatternTab &entry)
{
    updateTabLabel(entry.pattern, entry.tab, entry.tab->matchCount());
    Core::MessageManager::writeSilently(
        Tr::tr("MultiFilterOutput: stopped filtering \"%1\" for this run because matching it "
               "repeatedly took too long. Simplify its regular expression -- nested "
               "quantifiers such as (a+)+ are the usual cause.")
            .arg(entry.pattern.name));
}

void FilteredOutputPane::clearContents()
{
    m_lineQueue->clear();
    if (m_allTab)
        m_allTab->reset();
    for (PatternTab &entry : m_patternTabs) {
        entry.tab->reset();
        entry.inContinuation = false;
    }
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

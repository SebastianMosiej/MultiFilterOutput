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
#include "projectpatterns.h"
#include "tablabel.h"

#include <coreplugin/coreconstants.h>
#include <coreplugin/icore.h>
#include <coreplugin/messagemanager.h>

#include <projectexplorer/buildconfiguration.h>
#include <projectexplorer/project.h>
#include <projectexplorer/runconfiguration.h>
#include <projectexplorer/runcontrol.h>

#include <utils/fileutils.h>
#include <utils/qtcsettings.h>
#include <utils/utilsicons.h>
#include <utils/theme/theme.h>

#include <QClipboard>
#include <QComboBox>
#include <QDateTime>
#include <QGuiApplication>
#include <QInputDialog>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QRegularExpression>
#include <QToolButton>
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
    // The same filter box as Application Output's; enabled once the tabs exist.
    setupFilterUi("MultiFilterOutput.Filter", "MultiFilterOutput::Internal::FilteredOutputPane");
    setFilteringEnabled(false);

    Utils::QtcSettings *settings = Core::ICore::settings();
    settings->beginGroup(Constants::SETTINGS_GROUP);
    m_fontZoom = settings->value(Constants::OUTPUT_FONT_ZOOM_KEY, 0.0).toDouble();
    settings->endGroup();

    m_lineQueue = new LineQueue(
        [this](const QString &line, Utils::OutputFormat format, qint64 receivedAt) {
            processLine(line, format, receivedAt);
        },
        std::chrono::milliseconds(10),
        this);
    m_lineQueue->setOverflowHandling(
        LineQueue::DefaultMaxQueuedCharacters, [this](int discarded) { reportDiscarded(discarded); });

    RunTracker *tracker = m_runTracker;
    connect(tracker, &RunTracker::activeRunChanged, this, &FilteredOutputPane::rebuildTabs);
    connect(tracker, &RunTracker::activeRunChanged, this, &FilteredOutputPane::watchRunLabel);
    connect(tracker, &RunTracker::activeRunLineReceived, this, &FilteredOutputPane::onLineReceived);
    connect(tracker, &RunTracker::runsChanged, this, &FilteredOutputPane::rebuildRunSelector);
    applyHistoryLimit();
    tracker->setFinishedRunsKept(outputPaneSettings().finishedRunsKept());
    connect(
        &outputPaneSettings(),
        &OutputPaneSettings::finishedRunsKeptChanged,
        tracker,
        &RunTracker::setFinishedRunsKept);
    // Pattern edits apply to the running run straight away, not just from the next one.
    connect(
        &patternLibrary(),
        &PatternLibrary::patternsChanged,
        this,
        [this] { refreshPatternTabs(); });
    // ...and edits to the run's project's shared patterns (a pull, a Share with Project).
    connect(
        &projectPatterns(),
        &ProjectPatterns::patternsChanged,
        this,
        [this](const Utils::FilePath &projectDirectory) {
            if (!projectDirectory.isEmpty() && projectDirectory == activeProjectDirectory())
                refreshPatternTabs();
        });
    connect(&outputPaneSettings(), &OutputPaneSettings::showUnmatchedTabChanged, this, [this] {
        refreshPatternTabs(/*force=*/true);
    });

    // Word wrap and zoom from the toolbar (and the zoom shortcuts), for every tab.
    connect(&outputPaneSettings(), &OutputPaneSettings::showTimestampsChanged, this, [this](bool enabled) {
        if (m_timestampsButton)
            m_timestampsButton->setChecked(enabled);
        if (m_allTab)
            m_allTab->setShowTimestamps(enabled);
        for (const PatternTab &entry : std::as_const(m_patternTabs))
            entry.tab->setShowTimestamps(enabled);
    });
    connect(&outputPaneSettings(), &OutputPaneSettings::wordWrapChanged, this, [this](bool enabled) {
        if (m_wrapButton)
            m_wrapButton->setChecked(enabled);
        if (m_allTab)
            applyWordWrap(m_allTab);
        for (const PatternTab &entry : std::as_const(m_patternTabs))
            applyWordWrap(entry.tab);
    });
    connect(this, &IOutputPane::zoomInRequested, this, [this](int range) { zoomBy(range); });
    connect(this, &IOutputPane::zoomOutRequested, this, [this](int range) { zoomBy(-range); });
    connect(this, &IOutputPane::resetZoomRequested, this, [this] { onTabZoomChanged(0); });

    connect(&outputPaneSettings(), &OutputPaneSettings::maxCharactersPerTabChanged, this, [this] {
        applyHistoryLimit();
        const qsizetype limit = maxCharactersPerTab();
        if (m_allTab)
            m_allTab->setMaxCharacterCount(limit);
        for (const PatternTab &entry : std::as_const(m_patternTabs))
            entry.tab->setMaxCharacterCount(limit);
    });

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
        // So the zoom shortcuts (Ctrl++ / Ctrl+- / Ctrl+0) reach this pane when it has focus.
        setupContext(Utils::Id(Constants::PANE_CONTEXT), m_container);
        // Next/previous-match navigation works on the current tab.
        connect(m_tabWidget, &QTabWidget::currentChanged, this, &IOutputPane::navigateStateChanged);
        // The quick filter applies to the current tab, so re-apply it to whichever becomes
        // current (a no-op for a tab that already has it).
        connect(m_tabWidget, &QTabWidget::currentChanged, this, &FilteredOutputPane::updateFilter);
        // Right-click the tabs to choose which patterns filter this run.
        m_tabWidget->tabBar()->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(
            m_tabWidget->tabBar(),
            &QWidget::customContextMenuRequested,
            this,
            [this](const QPoint &pos) {
                showPatternsMenu(m_tabWidget->tabBar()->mapToGlobal(pos));
            });
        setFilteringEnabled(true);

        m_allTab = new OutputTabWidget(m_tabWidget);
        m_allTab->setFontZoom(m_fontZoom);
        m_allTab->setMaxLineLength(outputPaneSettings().maxDisplayedLineLength());
        applyWordWrap(m_allTab);
        m_allTab->setMaxCharacterCount(maxCharactersPerTab());
        m_allTab->setShowTimestamps(outputPaneSettings().showTimestamps());
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
    auto *self = const_cast<FilteredOutputPane *>(this);
    if (!m_runLabel) {
        self->m_runLabel = new QLabel;
        self->updateRunLabel();

        self->m_runSelector = new QComboBox;
        m_runSelector->setSizeAdjustPolicy(QComboBox::AdjustToContents);
        m_runSelector->setToolTip(
            Tr::tr("Which run to filter: the newest one, the one selected in Application "
                   "Output, or a specific one -- running (kept while other runs start) or "
                   "recently finished"));
        connect(m_runSelector, &QComboBox::activated, self, [self](int index) {
            if (index == 0) {
                self->m_runTracker->setFollowMode(RunTracker::FollowMode::NewestRun);
            } else if (index == 1) {
                self->m_runTracker->setFollowMode(RunTracker::FollowMode::ApplicationOutput);
            } else if (index < self->m_runSelectorRuns.size()) {
                if (RunControl *run = self->m_runSelectorRuns.at(index))
                    self->m_runTracker->pinRunControl(run);
            }
        });
        self->rebuildRunSelector();
    }
    if (!m_wrapButton) {
        self->m_timestampsButton = new QToolButton;
        m_timestampsButton->setText(Tr::tr("Time"));
        m_timestampsButton->setToolTip(Tr::tr("Show when each line arrived, in all tabs"));
        m_timestampsButton->setCheckable(true);
        m_timestampsButton->setChecked(outputPaneSettings().showTimestamps());
        connect(m_timestampsButton, &QToolButton::toggled, self, [](bool checked) {
            outputPaneSettings().setShowTimestamps(checked);
        });

        self->m_wrapButton = new QToolButton;
        m_wrapButton->setText(Tr::tr("Wrap"));
        m_wrapButton->setToolTip(Tr::tr("Word-wrap long lines in all tabs"));
        m_wrapButton->setCheckable(true);
        m_wrapButton->setChecked(outputPaneSettings().wordWrap());
        connect(m_wrapButton, &QToolButton::toggled, self, [](bool checked) {
            outputPaneSettings().setWordWrap(checked);
        });

        self->m_copyButton = new QToolButton;
        m_copyButton->setIcon(Utils::Icons::COPY_TOOLBAR.icon());
        m_copyButton->setToolTip(Tr::tr("Copy the current tab's contents"));
        connect(m_copyButton, &QToolButton::clicked, self, &FilteredOutputPane::copyCurrentTab);

        self->m_patternsButton = new QToolButton;
        m_patternsButton->setText(Tr::tr("Patterns"));
        m_patternsButton->setToolTip(
            Tr::tr("Choose which patterns filter this run (also right-click the tabs)"));
        m_patternsButton->setPopupMode(QToolButton::InstantPopup);
        auto *patternsMenu = new QMenu(m_patternsButton);
        connect(patternsMenu, &QMenu::aboutToShow, self, [self, patternsMenu] {
            self->fillPatternsMenu(patternsMenu);
        });
        m_patternsButton->setMenu(patternsMenu);

        self->m_saveFilterButton = new QToolButton;
        m_saveFilterButton->setText(Tr::tr("Save as Pattern..."));
        m_saveFilterButton->setToolTip(
            Tr::tr("Add the current filter to the Pattern Library, to enable it for run "
                   "configurations in their Output Filters settings."));
        m_saveFilterButton->setEnabled(!filterText().isEmpty());
        connect(m_saveFilterButton, &QToolButton::clicked, self, &FilteredOutputPane::saveFilterAsPattern);

        self->m_saveButton = new QToolButton;
        m_saveButton->setIcon(Utils::Icons::SAVEFILE_TOOLBAR.icon());
        m_saveButton->setToolTip(Tr::tr("Save the current tab to a file..."));
        connect(m_saveButton, &QToolButton::clicked, self, &FilteredOutputPane::saveCurrentTab);
    }
    // IOutputPane's own widgets: the filter box (see updateFilter()) and the zoom buttons.
    return IOutputPane::toolBarWidgets() << m_patternsButton << m_saveFilterButton
                                         << m_timestampsButton << m_wrapButton << m_copyButton
                                         << m_saveButton << m_runSelector << m_runLabel;
}

qsizetype FilteredOutputPane::maxCharactersPerTab() const
{
    if (const qsizetype own = outputPaneSettings().maxCharactersPerTab(); own > 0)
        return own;
    // Application Output's "Limit output to ... characters" (its AppOutputSettings aren't
    // exported, so the persisted key -- which says "Lines" but holds characters -- and its
    // default, Core::Constants::DEFAULT_MAX_CHAR_COUNT).
    return Core::ICore::settings()
        ->value("ProjectExplorer/Settings/MaxAppOutputLines", Core::Constants::DEFAULT_MAX_CHAR_COUNT)
        .toLongLong();
}

QColor FilteredOutputPane::highlightColor(const Pattern &pattern)
{
    if (!pattern.color.isValid())
        return Utils::creatorTheme()->color(Utils::Theme::TextColorHighlightBackground);
    QColor faint = pattern.color;
    faint.setAlpha(70);
    return faint;
}

void FilteredOutputPane::applyWordWrap(OutputTabWidget *tab)
{
    tab->setLineWrapMode(
        outputPaneSettings().wordWrap() ? QPlainTextEdit::WidgetWidth : QPlainTextEdit::NoWrap);
}

bool FilteredOutputPane::hasFilterContext() const
{
    return true;
}

void FilteredOutputPane::updateFilter()
{
    if (m_saveFilterButton)
        m_saveFilterButton->setEnabled(!filterText().isEmpty());
    if (OutputTabWidget *tab = currentTab()) {
        LineFilter filter;
        filter.text = filterText();
        filter.isRegex = filterUsesRegexp();
        filter.caseSensitivity = filterCaseSensitivity();
        filter.inverted = filterIsInverted();
        filter.beforeContext = beforeContext();
        filter.afterContext = afterContext();
        tab->setFilter(filter);
    }
}

void FilteredOutputPane::saveFilterAsPattern()
{
    if (filterText().isEmpty())
        return;
    bool ok = false;
    const QString name = QInputDialog::getText(
        Core::ICore::dialogParent(),
        Tr::tr("Save Filter as Pattern"),
        Tr::tr("Pattern name:"),
        QLineEdit::Normal,
        filterText(),
        &ok);
    if (!ok || name.trimmed().isEmpty())
        return;
    // Only the filter's text, regex, case and invert carry over; a pattern has no notion of
    // context lines.
    Pattern pattern = Pattern::createNew();
    pattern.name = name.trimmed();
    pattern.patternText = filterText();
    pattern.isRegex = filterUsesRegexp();
    pattern.caseSensitive = filterCaseSensitivity() == Qt::CaseSensitive;
    pattern.invert = filterIsInverted();
    patternLibrary().addOrUpdate(pattern); // persisted right away (see PatternLibrary::load())
    Core::MessageManager::writeSilently(
        Tr::tr("MultiFilterOutput: added pattern \"%1\" to the Pattern Library. Enable it for a "
               "run configuration under Run Settings > Output Filters.")
            .arg(pattern.name));
}

void FilteredOutputPane::zoomBy(qreal points)
{
    // The same pane-wide, persisted zoom as Ctrl+wheel in a tab, never below a 4 pt font
    // (the floor OutputTabWidget itself applies).
    if (m_allTab && points < 0 && m_allTab->font().pointSizeF() + points < 4)
        return;
    onTabZoomChanged(m_fontZoom + points);
}

void FilteredOutputPane::copyCurrentTab()
{
    if (const OutputTabWidget *tab = currentTab())
        QGuiApplication::clipboard()->setText(tab->toPlainText());
}

void FilteredOutputPane::saveCurrentTab()
{
    const OutputTabWidget *tab = currentTab();
    if (!tab)
        return;
    // Tab labels carry a match count ("Errors (12)"); the file name shouldn't.
    QString name = m_tabWidget->tabText(m_tabWidget->indexOf(const_cast<OutputTabWidget *>(tab)));
    name = name.section(" (", 0, 0).trimmed();
    name.replace(QRegularExpression("[^\\w.-]+"), "_");
    const Utils::FilePath path = Utils::FileUtils::getSaveFilePath(
        Tr::tr("Save Tab Contents"),
        Utils::FileUtils::homePath().pathAppended(name + ".txt"),
        Tr::tr("Text files (*.txt);;All files (*)"));
    if (path.isEmpty())
        return;
    if (const Utils::Result<> saved = tab->saveContents(path); !saved) {
        QMessageBox::warning(
            Core::ICore::dialogParent(),
            Tr::tr("Save Tab Contents"),
            Tr::tr("Could not save to \"%1\":\n%2").arg(path.toUserOutput(), saved.error()));
    }
}

void FilteredOutputPane::watchRunLabel(RunControl *activeRunControl)
{
    // A run can report itself started before its pid is known -- the debugger, remote-device
    // and Android/iOS runners all do -- so follow the pid as it arrives (and, on stop, as it
    // is cleared) instead of reading it once when the run becomes active.
    disconnect(m_pidConnection);
    if (activeRunControl) {
        m_pidConnection = connect(
            activeRunControl,
            &RunControl::applicationProcessHandleChanged,
            this,
            &FilteredOutputPane::updateRunLabel);
    }
    updateRunLabel();
}

void FilteredOutputPane::updateRunLabel()
{
    if (!m_runLabel)
        return;
    RunControl *activeRunControl = m_runTracker->activeRunControl();
    m_runLabel->setText(
        activeRunControl ? runLabel(
                               activeRunControl->displayName(),
                               activeRunControl->applicationProcessHandle().pid())
                         : Tr::tr("No active run"));
}

void FilteredOutputPane::rebuildTabs(RunControl *activeRunControl)
{
    if (!m_tabWidget)
        return;

    // A new run (or Re-run) may keep the previous run's output, greyed out above it, like
    // Application Output (see keepsOldRunOutput()). Not on a switch: the tabs then show the
    // run switched to, from its own history.
    const bool keepingOld = activeRunControl && !m_runTracker->lastActivationWasSwitch()
                            && keepsOldRunOutput();
    // The old run's last lines may still be queued -- among them the unterminated last line
    // flushed when it stopped: into All with them, to be kept with the rest.
    if (keepingOld)
        m_lineQueue->flushAll();
    const bool keepOld = keepingOld && !m_allTab->document()->isEmpty();

    removePatternTabs();
    // Lines still queued belong to the run being replaced (or to the tab set being torn
    // down); they must not land in the fresh tabs.
    m_lineQueue->clear();
    if (keepOld) {
        m_allTab->grayOutOldContent();
    } else {
        // reset(), not clear(): the inherited QPlainTextEdit::clear() leaves the match count,
        // the source-block mapping and the formatter's carry-over buffer stale.
        m_allTab->reset();
    }
    // No tab has any match stops now (the new pattern tabs start empty too).
    navigateStateChanged();

    // Arm the run-start pop for this run's first line (see onLineReceived()); a run going
    // away (activeRunControl == nullptr) has no "first line" left to pop on, and switching to
    // an already-running run was the user's doing, from this pane or Application Output.
    m_popOnNextLine = activeRunControl != nullptr && !m_runTracker->lastActivationWasSwitch();

    // Follow Application Output's "Discard excessive output" (Preferences > Build & Run >
    // Application Output), picked up per run as Application Output itself applies it when a
    // run's output window is set up. Its AppOutputSettings aren't exported, so this reads the
    // persisted key.
    m_lineQueue->setDiscardExcessiveOutput(
        Core::ICore::settings()->value("ProjectExplorer/Settings/DiscardAppOutput", false).toBool());
    // Likewise its output limit, when following it (pattern tabs get it as they're created).
    m_allTab->setMaxCharacterCount(maxCharactersPerTab());

    if (!activeRunControl) {
        m_runPatternIds.clear();
        m_runPatternIdsRun = nullptr;
        return;
    }

    Project *project = activeRunControl->project();
    m_allTab->setLinkParser(project);

    // A new run starts from what its configuration had enabled at launch. A Re-run is the
    // same RunControl with the same launch snapshot, so it keeps whatever was toggled from
    // the pane meanwhile instead of silently reverting to that snapshot.
    if (activeRunControl != m_runPatternIdsRun) {
        m_runPatternIds = launchPatternIds(activeRunControl);
        m_runPatternIdsRun = activeRunControl;
    }
    createPatternTabs(project, runPatterns());
    if (keepOld) {
        // The old output into the new tabs, greyed out, then a line where this run begins.
        replayAllIntoPatternTabs();
        const QString separator = Tr::tr("---------- %1 ----------").arg(activeRunControl->displayName());
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        const int allLine
            = m_allTab->appendLine(separator, Utils::NormalMessageFormat, -1, false, now);
        m_allTab->markLastLineAsNotice();
        for (const PatternTab &entry : std::as_const(m_patternTabs)) {
            entry.tab->appendLine(separator, Utils::NormalMessageFormat, allLine, false, now);
            entry.tab->markLastLineAsNotice();
        }
        m_router.resetContinuations();
        navigateStateChanged();
    }

    // What the run printed before becoming the active one (switched to, or before this pane
    // was first shown), filtered like live output and stamped with when it arrived.
    for (const RunTracker::HistoryLine &line : m_runTracker->history(activeRunControl))
        m_lineQueue->enqueue(line.text, line.format, line.receivedAt);
}

void FilteredOutputPane::applyHistoryLimit()
{
    // As much as a tab would keep anyway -- but at most half of what the line queue holds, so
    // replaying a run's history on a switch never overflows it (see LineQueue::enqueue()).
    const qsizetype tabLimit = maxCharactersPerTab();
    const qsizetype queueLimit = LineQueue::DefaultMaxQueuedCharacters / 2;
    m_runTracker->setHistoryLimit(tabLimit > 0 ? qMin(tabLimit, queueLimit) : queueLimit);
}

void FilteredOutputPane::rebuildRunSelector()
{
    if (!m_runSelector)
        return;

    const QList<RunControl *> finished = m_runTracker->finishedRunControls();
    const auto itemText = [&finished](RunControl *run) {
        const qint64 pid = run->applicationProcessHandle().pid();
        const QString name = pid > 0
                                 ? Tr::tr("%1 (pid %2)").arg(run->displayName(), QString::number(pid))
                                 : run->displayName();
        return finished.contains(run) ? Tr::tr("%1 (finished)").arg(name) : name;
    };

    QSignalBlocker blocker(m_runSelector);
    m_runSelector->clear();
    m_runSelectorRuns.clear();
    m_runSelector->addItem(Tr::tr("Newest run"));
    m_runSelector->addItem(Tr::tr("Application Output's run"));
    m_runSelectorRuns << nullptr << nullptr;

    // Oldest first: the finished runs, then the running ones.
    QList<RunControl *> runs = finished + m_runTracker->liveRunControls();
    RunControl *active = m_runTracker->activeRunControl();
    const bool pinned = m_runTracker->followMode() == RunTracker::FollowMode::Pinned;
    // A pinned run that has stopped is still what the pane shows: list it, so it can be seen
    // as the current choice.
    if (pinned && active && !runs.contains(active))
        runs.prepend(active);
    for (RunControl *run : std::as_const(runs)) {
        m_runSelector->addItem(itemText(run));
        m_runSelectorRuns << run;
        // The pid arrives after the run starts.
        connect(
            run,
            &RunControl::applicationProcessHandleChanged,
            this,
            &FilteredOutputPane::rebuildRunSelector,
            Qt::UniqueConnection);
    }

    switch (m_runTracker->followMode()) {
    case RunTracker::FollowMode::NewestRun:
        m_runSelector->setCurrentIndex(0);
        break;
    case RunTracker::FollowMode::ApplicationOutput:
        m_runSelector->setCurrentIndex(1);
        break;
    case RunTracker::FollowMode::Pinned:
        m_runSelector->setCurrentIndex(qMax(0, int(m_runSelectorRuns.indexOf(active))));
        break;
    }
}

QStringList FilteredOutputPane::launchPatternIds(RunControl *runControl) const
{
    // RunControl (v20.0.82) does not retain a live RunConfiguration* of its own -- it only
    // copies data out of one at start time (copyDataFromRunConfiguration), snapshotting each
    // aspect's Data into a stable, launch-time-frozen copy. Reading through the current
    // BuildConfiguration's activeRunConfiguration() instead would pick up whichever run
    // configuration happens to be active *now*, not the one this run was actually launched
    // from -- so we must read the RunControl's own snapshot here.
    return launchSettings(runControl).patternIds;
}

RunFilterSettings FilteredOutputPane::launchSettings(RunControl *runControl) const
{
    const auto *filterData = runControl ? runControl->aspectData<FilterSetAspect>() : nullptr;
    return filterData ? runFilterSettings(filterData->value) : RunFilterSettings();
}

bool FilteredOutputPane::focusOnRunStart() const
{
    return resolvedSetting(
        launchSettings(m_runTracker->activeRunControl()).focusOnRunStart,
        outputPaneSettings().focusPaneOnRunStart());
}

bool FilteredOutputPane::showUnmatchedTab() const
{
    return resolvedSetting(
        launchSettings(m_runTracker->activeRunControl()).showUnmatchedTab,
        outputPaneSettings().showUnmatchedTab());
}

QList<Pattern> FilteredOutputPane::runPatterns() const
{
    // Library order (then the project's), not m_runPatternIds order: the latter is just the
    // order the boxes were ticked in, while the library's is the one the user deliberately
    // arranges. Patterns deleted since the selection was saved are skipped silently.
    QList<Pattern> result;
    for (const Pattern &pattern : availablePatterns()) {
        if (m_runPatternIds.contains(pattern.id))
            result.append(pattern);
    }
    return result;
}

Utils::FilePath FilteredOutputPane::activeProjectDirectory() const
{
    RunControl *run = m_runTracker->activeRunControl();
    Project *project = run ? run->project() : nullptr;
    return project ? project->projectDirectory() : Utils::FilePath();
}

QList<Pattern> FilteredOutputPane::availablePatterns() const
{
    return combinedPatterns(
        patternLibrary().patterns(), projectPatterns().patterns(activeProjectDirectory()));
}

RunConfiguration *FilteredOutputPane::launchedRunConfiguration(RunControl *runControl) const
{
    // A RunControl keeps no pointer to the RunConfiguration it was started from, only data
    // copied out of it -- among which the build key and display name, which together pick
    // it out among its build configuration's run configurations. If they don't do so
    // unambiguously, don't guess.
    BuildConfiguration *bc = runControl->buildConfiguration();
    if (!bc)
        return nullptr;
    QList<RunConfiguration *> candidates;
    for (RunConfiguration *rc : bc->runConfigurations()) {
        if (rc->buildKey() == runControl->buildKey())
            candidates << rc;
    }
    if (candidates.size() > 1) {
        candidates.removeIf([runControl](const RunConfiguration *rc) {
            return rc->expandedDisplayName() != runControl->displayName();
        });
    }
    return candidates.size() == 1 ? candidates.first() : nullptr;
}

void FilteredOutputPane::setPatternEnabledForRun(const QString &id, bool enabled)
{
    const QList<Pattern> patterns = availablePatterns();
    const auto pattern = std::find_if(patterns.cbegin(), patterns.cend(), [&](const Pattern &p) {
        return p.id == id;
    });
    setPatternsEnabledForRun(
        {id}, enabled, Tr::tr("\"%1\"").arg(pattern != patterns.cend() ? pattern->name : id));
}

void FilteredOutputPane::setPatternsEnabledForRun(
    const QStringList &ids, bool enabled, const QString &what)
{
    RunControl *run = m_runTracker->activeRunControl();
    if (!run)
        return;
    for (const QString &id : ids) {
        if (enabled && !m_runPatternIds.contains(id))
            m_runPatternIds.append(id);
        else if (!enabled)
            m_runPatternIds.removeAll(id);
    }
    refreshPatternTabs();

    // Like ticking the box in Run Settings > Output Filters: the next run of the same run
    // configuration keeps the choice.
    const QString name = what;
    if (RunConfiguration *rc = launchedRunConfiguration(run)) {
        if (auto *aspect = rc->aspect<FilterSetAspect>())
            aspect->setPatternsEnabled(ids, enabled);
    } else {
        Core::MessageManager::writeSilently(
            (enabled ? Tr::tr("MultiFilterOutput: enabled %1 for this run only -- the run "
                              "configuration it was started from couldn't be identified, so its "
                              "Output Filters are unchanged.")
                     : Tr::tr("MultiFilterOutput: disabled %1 for this run only -- the run "
                              "configuration it was started from couldn't be identified, so its "
                              "Output Filters are unchanged."))
                .arg(name));
    }
}

void FilteredOutputPane::showPatternsMenu(const QPoint &globalPos)
{
    QMenu menu;
    fillPatternsMenu(&menu);
    menu.exec(globalPos);
}

void FilteredOutputPane::fillPatternsMenu(QMenu *menu)
{
    menu->clear();
    const bool haveRun = m_runTracker->activeRunControl() != nullptr;
    menu->addSection(Tr::tr("Filter This Run By"));
    const QList<Pattern> patterns = availablePatterns();
    const QList<Pattern> shared = projectPatterns().patterns(activeProjectDirectory());
    if (patterns.isEmpty()) {
        menu->addAction(Tr::tr("No patterns defined"))->setEnabled(false);
    }
    for (const Pattern &pattern : patterns) {
        QString text = pattern.invert ? Tr::tr("Not: %1").arg(pattern.name) : pattern.name;
        if (std::any_of(shared.cbegin(), shared.cend(), [&](const Pattern &p) {
                return p.id == pattern.id;
            })) {
            text = Tr::tr("%1 (project)").arg(text);
        }
        QAction *action = menu->addAction(text);
        action->setCheckable(true);
        action->setChecked(m_runPatternIds.contains(pattern.id));
        action->setEnabled(haveRun);
        connect(action, &QAction::toggled, this, [this, id = pattern.id](bool checked) {
            setPatternEnabledForRun(id, checked);
        });
    }
    // Whole groups (see Pattern::tags): checked when all of a group's patterns filter the run.
    if (const QStringList tags = patternTags(patterns); !tags.isEmpty()) {
        menu->addSection(Tr::tr("Groups"));
        for (const QString &tag : tags) {
            const QStringList ids = patternIdsWithTag(patterns, tag);
            const bool allOn = std::all_of(ids.cbegin(), ids.cend(), [this](const QString &id) {
                return m_runPatternIds.contains(id);
            });
            QAction *action = menu->addAction(Tr::tr("%1 (%2)").arg(tag).arg(ids.size()));
            action->setCheckable(true);
            action->setChecked(allOn);
            action->setEnabled(haveRun);
            connect(action, &QAction::toggled, this, [this, tag, ids](bool checked) {
                setPatternsEnabledForRun(ids, checked, Tr::tr("the group \"%1\"").arg(tag));
            });
        }
    }
    menu->addSeparator();
    connect(menu->addAction(Tr::tr("Edit Pattern Library...")), &QAction::triggered, this, [] {
        Core::ICore::showSettings(Constants::OPTIONS_PAGE_ID);
    });
}

void FilteredOutputPane::removePatternTabs()
{
    for (const PatternTab &entry : std::as_const(m_patternTabs)) {
        const int index = m_tabWidget->indexOf(entry.tab);
        if (index != -1)
            m_tabWidget->removeTab(index);
        entry.tab->deleteLater();
    }
    m_patternTabs.clear();
    m_router = LineRouter();
}

void FilteredOutputPane::createPatternTabs(Project *project, const QList<Pattern> &patterns)
{
    // Fresh with the tabs: continuation state and slow-pattern breakers start over. Its
    // pattern indexes are m_patternTabs' indexes.
    m_router = LineRouter(patterns);
    for (const Pattern &pattern : patterns)
        addPatternTab(project, pattern, /*isUnmatched=*/false);

    // The complement of the pattern tabs, for triage -- pointless without a pattern that
    // actually picks lines out (inverted ones don't: see LineRouter::Result::unmatched).
    const bool anyPicksLinesOut = std::any_of(patterns.cbegin(), patterns.cend(), [](const Pattern &p) {
        return !p.invert;
    });
    if (showUnmatchedTab() && anyPicksLinesOut) {
        Pattern unmatched;
        unmatched.id = QStringLiteral("MultiFilterOutput.Unmatched");
        unmatched.name = Tr::tr("Unmatched");
        addPatternTab(project, unmatched, /*isUnmatched=*/true);
    }
}

OutputTabWidget *FilteredOutputPane::unmatchedTab() const
{
    return !m_patternTabs.isEmpty() && m_patternTabs.last().isUnmatched ? m_patternTabs.last().tab
                                                                          : nullptr;
}

void FilteredOutputPane::addPatternTab(Project *project, const Pattern &pattern, bool isUnmatched)
{
    auto *tab = new OutputTabWidget(m_tabWidget);
    tab->setLinkParser(project);
    tab->setFontZoom(m_fontZoom);
    tab->setMaxLineLength(outputPaneSettings().maxDisplayedLineLength());
    applyWordWrap(tab);
    tab->setMaxCharacterCount(maxCharactersPerTab());
    tab->setShowTimestamps(outputPaneSettings().showTimestamps());
    connect(tab, &OutputTabWidget::matchCountChanged, this, [this, pattern, tab](int count) {
        updateTabLabel(pattern, tab, count);
    });
    connect(tab, &OutputTabWidget::jumpRequested, this, &FilteredOutputPane::onJumpRequested);
    connect(tab, &OutputTabWidget::fontZoomChanged, this, &FilteredOutputPane::onTabZoomChanged);

    m_patternTabs.append({pattern, tab, isUnmatched});
    m_tabWidget->addTab(tab, tabLabel(pattern, 0));
    updateTabLabel(pattern, tab, 0);
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
    const bool tooSlow = m_router.isTooSlow(int(entry - m_patternTabs.cbegin()));
    applyTabLabel(m_tabWidget->tabBar(), index, pattern, count, tooSlow);
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
        if (focusOnRunStart())
            popup(Core::IOutputPane::NoModeSwitch | Core::IOutputPane::WithFocus);
    }

    // The pop above has to happen right here, in step with Application Output's own
    // handling of this same signal; the per-line tab work (processLine()) is deferred to
    // m_lineQueue, which runs it in time-limited slices so a burst of output can't freeze
    // the GUI.
    m_lineQueue->enqueue(line, format);
}

void FilteredOutputPane::processLine(
    const QString &line, Utils::OutputFormat format, qint64 receivedAt)
{
    if (!m_allTab)
        return;

    // The "All" tab is the jump target, so its line id for this line is what every matching
    // pattern tab must record -- not an id of each tab's own.
    const int allLine = m_allTab->appendLine(line, format, -1, true, receivedAt);

    const bool couldNavigate = canNext();
    if (routeToPatternTabs(line, format, allLine, /*allLineIsLast=*/true, receivedAt))
        m_allTab->markLastLineAsMatch();
    if (canNext() != couldNavigate)
        navigateStateChanged();
}

bool FilteredOutputPane::routeToPatternTabs(
    const QString &line,
    Utils::OutputFormat format,
    int allLine,
    bool allLineIsLast,
    qint64 timestamp,
    bool fromOldRun)
{
    const LineRouter::Result result = m_router.route(line, format, allLine);
    const bool highlightInAll = allLineIsLast && outputPaneSettings().highlightMatchesInAll();
    bool matchedAnyPattern = false;
    for (const LineRouter::Route &route : result.routes) {
        const PatternTab &entry = m_patternTabs.at(route.patternIndex);
        OutputTabWidget *tab = entry.tab;
        // Before-context: earlier lines, fetched back from All (unless it has dropped them
        // for room meanwhile).
        for (const int contextLine : route.contextBefore) {
            if (const auto shown = m_allTab->line(contextLine)) {
                tab->appendLine(
                    shown->text, shown->format, contextLine, /*countsAsMatch=*/false,
                    shown->timestamp);
                if (shown->isOld)
                    tab->markLastLineAsOld();
            }
        }
        // The previous run's output, kept greyed out: shown where it belongs, but it is not
        // what the tab counts, stops at or highlights.
        tab->appendLine(line, format, allLine, route.countsAsMatch && !fromOldRun, timestamp);
        if (fromOldRun) {
            tab->markLastLineAsOld();
            continue;
        }
        if (route.countsAsMatch) {
            tab->markLastLineAsMatch();
            matchedAnyPattern = true;
            // Computed on each tab's line as shown -- the spans must point into that text.
            const QColor color = highlightColor(entry.pattern);
            tab->highlightLastLine(matchSpans(entry.pattern, tab->lastLineText()), color);
            if (highlightInAll)
                m_allTab->highlightLastLine(matchSpans(entry.pattern, m_allTab->lastLineText()), color);
        }
    }
    if (result.unmatched) {
        if (OutputTabWidget *tab = unmatchedTab()) {
            tab->appendLine(line, format, allLine, !fromOldRun, timestamp);
            if (fromOldRun)
                tab->markLastLineAsOld();
            else
                tab->markLastLineAsMatch();
        }
    }
    for (const int index : result.newlyTooSlow)
        reportTooSlow(m_patternTabs.at(index));
    return matchedAnyPattern;
}

void FilteredOutputPane::replayAllIntoPatternTabs()
{
    // Replay what All still holds through the new patterns -- as shown in All, which is all
    // that is kept: a line All cut short (see setMaxLineLength()) is matched only as far as
    // shown. That text is already cut, so the pattern tabs mustn't cut it (and its note)
    // again while replaying.
    for (const PatternTab &entry : std::as_const(m_patternTabs))
        entry.tab->setMaxLineLength(0);
    m_allTab->clearMatchStops();
    for (const OutputTabWidget::Line &line : m_allTab->lines()) {
        if (line.isNotice) {
            for (const PatternTab &entry : std::as_const(m_patternTabs)) {
                entry.tab->appendLine(
                    line.text, line.format, line.lineId, /*countsAsMatch=*/false, line.timestamp);
                entry.tab->markLastLineAsNotice();
                if (line.isOld)
                    entry.tab->markLastLineAsOld();
            }
            m_router.resetContinuations();
            continue;
        }
        if (routeToPatternTabs(
                line.text, line.format, line.lineId, /*allLineIsLast=*/false, line.timestamp,
                line.isOld))
            m_allTab->markLineAsMatch(line.lineId);
    }
    for (const PatternTab &entry : std::as_const(m_patternTabs))
        entry.tab->setMaxLineLength(outputPaneSettings().maxDisplayedLineLength());
}

bool FilteredOutputPane::keepsOldRunOutput() const
{
    // Application Output's AppOutputSettings aren't exported: its persisted key.
    const bool applicationOutputClears
        = Core::ICore::settings()->value("ProjectExplorer/Settings/CleanOldAppOutput", false).toBool();
    return OutputPaneSettings::keepsOldRunOutput(
        outputPaneSettings().oldRunOutput(), applicationOutputClears);
}

void FilteredOutputPane::refreshPatternTabs(bool force)
{
    RunControl *run = m_runTracker->activeRunControl();
    if (!m_tabWidget || !m_allTab || !run)
        return;

    // The library changes for many reasons (a pattern this run doesn't use was edited or
    // added, say); only redo the tabs if this run's own patterns are any different.
    const QList<Pattern> patterns = runPatterns();
    const qsizetype patternTabCount = m_patternTabs.size() - (unmatchedTab() ? 1 : 0);
    const bool unchanged = patterns.size() == patternTabCount
                           && std::equal(
                               patterns.cbegin(), patterns.cend(), m_patternTabs.cbegin(),
                               [](const Pattern &p, const PatternTab &t) {
                                   return p.toMap() == t.pattern.toMap();
                               });
    if (unchanged && !force)
        return;

    // Stay on the same tab if its pattern is still there.
    const OutputTabWidget *current = currentTab();
    const auto currentEntry = std::find_if(
        m_patternTabs.cbegin(), m_patternTabs.cend(), [current](const PatternTab &t) {
            return t.tab == current;
        });
    const QString currentPatternId = currentEntry != m_patternTabs.cend()
                                         ? currentEntry->pattern.id
                                         : QString();

    removePatternTabs();
    createPatternTabs(run->project(), patterns);
    replayAllIntoPatternTabs();

    for (const PatternTab &entry : std::as_const(m_patternTabs)) {
        if (!currentPatternId.isEmpty() && entry.pattern.id == currentPatternId)
            m_tabWidget->setCurrentWidget(entry.tab);
    }
    navigateStateChanged();
    updateFilter();
}

void FilteredOutputPane::reportDiscarded(int lines)
{
    if (!m_allTab)
        return;
    // In every tab, not just All: any of the dropped lines might have matched a pattern.
    // Not counted as a match anywhere.
    const QString notice = Tr::tr("[MultiFilterOutput: %1 lines of output were dropped here -- "
                                  "they arrived faster than they could be filtered.]")
                               .arg(lines);
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const int allLine = m_allTab->appendLine(notice, Utils::ErrorMessageFormat, -1, false, now);
    m_allTab->markLastLineAsNotice();
    for (const PatternTab &entry : std::as_const(m_patternTabs)) {
        entry.tab->appendLine(notice, Utils::ErrorMessageFormat, allLine, /*countsAsMatch=*/false, now);
        entry.tab->markLastLineAsNotice();
    }
    // A continuation run can't be continued across a gap: what follows didn't follow the match.
    m_router.resetContinuations();
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
    for (const PatternTab &entry : std::as_const(m_patternTabs))
        entry.tab->reset();
    m_router.resetContinuations();
    navigateStateChanged();
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

// Next/previous item (F6 / Shift+F6) steps through the current tab's matches: in a pattern
// tab its matching lines (not continuation lines), in All every line that matched a pattern.
bool FilteredOutputPane::canNavigate() const
{
    return true;
}

bool FilteredOutputPane::canNext() const
{
    const OutputTabWidget *tab = currentTab();
    return tab && tab->hasMatchStops();
}

bool FilteredOutputPane::canPrevious() const
{
    return canNext();
}

void FilteredOutputPane::goToNext()
{
    goToMatch(OutputTabWidget::Direction::Forward);
}

void FilteredOutputPane::goToPrev()
{
    goToMatch(OutputTabWidget::Direction::Backward);
}

OutputTabWidget *FilteredOutputPane::currentTab() const
{
    return m_tabWidget ? qobject_cast<OutputTabWidget *>(m_tabWidget->currentWidget()) : nullptr;
}

void FilteredOutputPane::goToMatch(OutputTabWidget::Direction direction)
{
    OutputTabWidget *tab = currentTab();
    if (!tab)
        return;
    const int lineId = tab->goToMatch(direction);
    // Keep All at the same line, without switching to it: switching tabs afterwards shows
    // the context, and stepping on in All continues from there.
    if (lineId >= 0 && tab != m_allTab)
        m_allTab->scrollToLine(lineId);
}

} // namespace MultiFilterOutput::Internal

#include "filteredoutputpane.h"

#include "filtersetaspect.h"
#include "linematcher.h"
#include "multifilteroutputconstants.h"
#include "multifilteroutputtr.h"
#include "outputtabwidget.h"
#include "patternlibrary.h"
#include "tablabel.h"

#include <projectexplorer/buildconfiguration.h>
#include <projectexplorer/project.h>
#include <projectexplorer/runcontrol.h>

#include <QLabel>
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
    setPriorityInStatusBar(-10); // just right of Application Output

    connect(m_runTracker, &RunTracker::activeRunChanged, this, &FilteredOutputPane::rebuildTabs);
    connect(m_runTracker,
            &RunTracker::activeRunLineReceived,
            this,
            &FilteredOutputPane::onLineReceived);
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
    m_allTab->clear();

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
    // copies data out of one at start time (copyDataFromRunConfiguration). The live
    // RunConfiguration this run was started from is reached indirectly, via the
    // BuildConfiguration it holds a reference to.
    BuildConfiguration *buildConfig = activeRunControl->buildConfiguration();
    RunConfiguration *runConfig = buildConfig ? buildConfig->activeRunConfiguration() : nullptr;
    FilterSetAspect *filterAspect = runConfig ? runConfig->aspect<FilterSetAspect>() : nullptr;
    const QStringList enabledIds = filterAspect ? filterAspect->enabledPatternIds() : QStringList();

    for (const QString &id : enabledIds) {
        const auto pattern = patternLibrary().patternById(id);
        if (!pattern)
            continue; // pattern deleted since selection was saved -- skip silently

        auto *tab = new OutputTabWidget(m_tabWidget);
        tab->setLinkParser(project);
        tab->setJumpTarget(m_allTab);
        connect(tab, &OutputTabWidget::matchCountChanged, this, [this, tab](int count) {
            for (int i = 0; i < m_tabWidget->count(); ++i) {
                if (m_tabWidget->widget(i) == tab) {
                    for (const auto &pair : std::as_const(m_patternTabs)) {
                        if (pair.second == tab)
                            m_tabWidget->setTabText(i, tabLabel(pair.first, count));
                    }
                }
            }
        });

        m_patternTabs.append({*pattern, tab});
        m_tabWidget->addTab(tab, tabLabel(*pattern, 0));
    }
}

void FilteredOutputPane::onLineReceived(const QString &line, Utils::OutputFormat format)
{
    if (!m_allTab)
        return;

    m_allTab->appendLine(line, format);

    for (const auto &pair : std::as_const(m_patternTabs)) {
        if (lineMatches(pair.first, line))
            pair.second->appendLine(line, format);
    }
}

void FilteredOutputPane::clearContents()
{
    if (m_allTab)
        m_allTab->clear();
    for (const auto &pair : std::as_const(m_patternTabs))
        pair.second->clear();
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

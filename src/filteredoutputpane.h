#pragma once

#include "pattern.h"
#include "runtracker.h"

#include <coreplugin/ioutputpane.h>

#include <QList>
#include <QPair>

QT_BEGIN_NAMESPACE
class QLabel;
class QTabWidget;
QT_END_NAMESPACE

namespace ProjectExplorer {
class RunControl;
}

namespace MultiFilterOutput {
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
    void onJumpRequested(int sourceBlockNumber);
    void updateTabLabel(const Pattern &pattern, OutputTabWidget *tab, int count);
    // Keeps every tab in this pane at the same zoom level: applied whenever a tab is
    // (re)created, and re-broadcast to every other tab (and persisted) whenever any one tab's
    // own wheelEvent() changes it.
    void onTabZoomChanged(qreal zoom);

    RunTracker *m_runTracker;
    QWidget *m_container = nullptr;
    QTabWidget *m_tabWidget = nullptr;
    OutputTabWidget *m_allTab = nullptr;
    QList<QPair<Pattern, OutputTabWidget *>> m_patternTabs;
    QLabel *m_runLabel = nullptr;
    qreal m_fontZoom = 0;
};

} // namespace MultiFilterOutput::Internal

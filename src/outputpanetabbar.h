#pragma once

#include <QTabBar>
#include <QTabWidget>

namespace MultiFilterOutput {

// QTabWidget's default tab bar renders unselected tabs through the platform style's native
// tab chrome, which typically ignores the application palette (Windows paints a light OS
// tab regardless of theme). Forcing a flat background via a stylesheet instead routes
// CE_TabBarTabLabel through Qt's QWindowsStyle fallback renderer, which deliberately dims
// unselected tab text -- trading the grey background for grey text. Painting the tab bar
// directly sidesteps both: background and (default) text match Application Output's own
// colors exactly, with no dimming in either state.
class OutputPaneTabBar : public QTabBar
{
    Q_OBJECT
public:
    using QTabBar::QTabBar;

protected:
    void paintEvent(QPaintEvent *event) override;
};

// QTabWidget::setTabBar() is protected, so installing OutputPaneTabBar needs a subclass
// (mirrors Qt Creator's own AppOutputPane::TabWidget, which does the same for DocumentTabBar).
class OutputPaneTabWidget : public QTabWidget
{
    Q_OBJECT
public:
    explicit OutputPaneTabWidget(QWidget *parent = nullptr);
};

} // namespace MultiFilterOutput

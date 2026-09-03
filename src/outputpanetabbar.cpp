#include "outputpanetabbar.h"

#include <utils/theme/theme.h>

#include <QPainter>

namespace MultiFilterOutput {

void OutputPaneTabBar::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    const QColor background = Utils::creatorTheme()->color(Utils::Theme::PaletteBase);
    const QColor normalText = Utils::creatorTheme()->color(Utils::Theme::PaletteWindowText);
    const QColor highlight = Utils::creatorTheme()->color(Utils::Theme::PaletteHighlight);
    const QColor separator = Utils::creatorTheme()->color(Utils::Theme::SplitterColor);

    painter.fillRect(rect(), background);
    painter.setFont(font());

    for (int i = 0; i < count(); ++i) {
        const QRect tabGeometry = tabRect(i);
        // Pattern::color (set via setTabTextColor() in FilteredOutputPane::updateTabLabel())
        // takes priority over the default text color; falls back to Application Output's own
        // normal text color for tabs the user hasn't customized.
        const QColor textColor = tabTextColor(i).isValid() ? tabTextColor(i) : normalText;
        painter.setPen(textColor);
        const QString elided = fontMetrics().elidedText(
            tabText(i), Qt::ElideRight, tabGeometry.width() - 16);
        painter.drawText(tabGeometry, Qt::AlignCenter, elided);

        if (i == currentIndex()) {
            QRect underline = tabGeometry;
            underline.setTop(underline.bottom() - 1);
            painter.fillRect(underline, highlight);
        }

        if (i > 0)
            painter.fillRect(QRect(tabGeometry.left(), 4, 1, tabGeometry.height() - 8), separator);
    }
}

OutputPaneTabWidget::OutputPaneTabWidget(QWidget *parent)
    : QTabWidget(parent)
{
    setTabBar(new OutputPaneTabBar(this));
}

} // namespace MultiFilterOutput

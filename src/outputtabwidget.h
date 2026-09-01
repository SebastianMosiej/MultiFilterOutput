#pragma once

#include "multifilteroutput_global.h"

#include <utils/outputformatter.h>

#include <QList>
#include <QPlainTextEdit>
#include <QPoint>

namespace ProjectExplorer {
class Project;
}

namespace MultiFilterOutput {

// A single output tab: owns a Utils::OutputFormatter that appends formatted, optionally
// linkified lines to itself. Used by FilteredOutputPane both for the "All" tab and for each
// enabled pattern's tab. A click that doesn't land on a linkified reference emits
// jumpRequested() carrying the "All" tab block number recorded for the clicked line, which
// FilteredOutputPane turns into "show the All tab, scrolled to that line".
class MULTIFILTEROUTPUT_EXPORT OutputTabWidget : public QPlainTextEdit
{
    Q_OBJECT

public:
    explicit OutputTabWidget(QWidget *parent = nullptr);

    void setLinkParser(ProjectExplorer::Project *project); // wires MultiFilterLineParser

    // Appends one line and returns the block number it was written to, for use as the
    // sourceBlockNumber argument on the subsequent calls that mirror this same line into
    // other tabs. The "All" tab is appended to first with no sourceBlockNumber (it is its
    // own source); each matching pattern tab is then given the number "All" returned, so a
    // click in a pattern tab can be mapped back to the right line of "All".
    int appendLine(const QString &line, Utils::OutputFormat format, int sourceBlockNumber = -1);

    int matchCount() const;

    // Empties the tab completely: document, source-block mapping, match count (with a
    // matchCountChanged(0) so the tab label follows) and the formatter's in-flight state.
    void reset();

    // Scrolls to and selects the given block, if it still exists.
    void scrollToBlock(int blockNumber);

signals:
    void matchCountChanged(int count);
    // Emitted on a click that isn't a linkified reference; sourceBlockNumber is the value
    // this tab recorded for the clicked line, or -1 if there is none.
    void jumpRequested(int sourceBlockNumber);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    Utils::OutputFormatter m_formatter;
    // Parallel to the appended lines: each entry is the sourceBlockNumber passed to
    // appendLine() for that line, or this widget's own block number when none was given.
    QList<int> m_sourceBlockNumbers;
    QPoint m_pressPos;
    int m_matchCount = 0;
};

} // namespace MultiFilterOutput

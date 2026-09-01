#pragma once

#include "multifilteroutput_global.h"

#include <utils/outputformatter.h>

#include <QPlainTextEdit>

namespace ProjectExplorer {
class Project;
}

namespace MultiFilterOutput {

// A single output tab: owns a Utils::OutputFormatter that appends formatted, optionally
// linkified lines to itself. Used by FilteredOutputPane (Task 11) both for the "All" tab
// and for each enabled pattern's tab; the pattern tabs are given the "All" tab as their
// jump target so a click that doesn't land on a linkified reference scrolls the "All" tab
// to (and highlights) the corresponding source line instead.
class MULTIFILTEROUTPUT_EXPORT OutputTabWidget : public QPlainTextEdit
{
    Q_OBJECT

public:
    explicit OutputTabWidget(QWidget *parent = nullptr);

    void setLinkParser(ProjectExplorer::Project *project); // wires MultiFilterLineParser
    void appendLine(const QString &line, Utils::OutputFormat format);
    int matchCount() const;

    // Sets the widget this tab should scroll to (and highlight the given block in) when a
    // click doesn't land on a linkified reference.
    void setJumpTarget(OutputTabWidget *allTabWidget);

signals:
    void matchCountChanged(int count);

protected:
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    Utils::OutputFormatter m_formatter;
    OutputTabWidget *m_jumpTarget = nullptr;
    QList<int> m_sourceBlockNumbers; // parallel to appended lines, index into m_jumpTarget
    int m_matchCount = 0;
};

} // namespace MultiFilterOutput

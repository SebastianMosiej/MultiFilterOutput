#include "outputtabwidget.h"

#include "multifilterlineparser.h"

#include <coreplugin/editormanager/editormanager.h>

#include <utils/link.h>

#include <QMouseEvent>
#include <QTextBlock>
#include <QTextCursor>

namespace MultiFilterOutput {

OutputTabWidget::OutputTabWidget(QWidget *parent)
    : QPlainTextEdit(parent)
{
    setReadOnly(true);
    setMaximumBlockCount(50000);
    m_formatter.setPlainTextEdit(this);

    // An absolute "file://" link is resolved by OutputFormatter itself (see
    // Utils::OutputFormatter::handleFileLink()) rather than being forwarded to the line
    // parser's handleLink(); mirror Core::OutputWindow's wiring so such links still open
    // an editor.
    connect(
        &m_formatter,
        &Utils::OutputFormatter::openInEditorRequested,
        this,
        [](const Utils::Link &link) { Core::EditorManager::openEditorAt(link); });
}

void OutputTabWidget::setLinkParser(ProjectExplorer::Project *project)
{
    m_formatter.setLineParsers({new Internal::MultiFilterLineParser(project)});
}

void OutputTabWidget::appendLine(const QString &line, Utils::OutputFormat format)
{
    m_formatter.appendMessage(line + "\n", format);
    // appendMessage's trailing newline advances the cursor into a new, still-empty block, so
    // the block that actually holds the line just appended is the one before the current
    // last block.
    m_sourceBlockNumbers.append(document()->blockCount() - 2);
    ++m_matchCount;
    emit matchCountChanged(m_matchCount);
}

int OutputTabWidget::matchCount() const
{
    return m_matchCount;
}

void OutputTabWidget::setJumpTarget(OutputTabWidget *allTabWidget)
{
    m_jumpTarget = allTabWidget;
}

void OutputTabWidget::mouseReleaseEvent(QMouseEvent *event)
{
    const QString href = anchorAt(event->pos());
    if (!href.isEmpty()) {
        m_formatter.handleLink(href);
    } else if (m_jumpTarget) {
        const int blockNumber = cursorForPosition(event->pos()).blockNumber();
        if (blockNumber >= 0 && blockNumber < m_sourceBlockNumbers.size()) {
            const int targetBlock = m_sourceBlockNumbers.at(blockNumber);
            QTextCursor cursor(m_jumpTarget->document()->findBlockByNumber(targetBlock));
            m_jumpTarget->setTextCursor(cursor);
            m_jumpTarget->centerCursor();
        }
    }

    QPlainTextEdit::mouseReleaseEvent(event);
}

} // namespace MultiFilterOutput

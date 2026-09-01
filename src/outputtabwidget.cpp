#include "outputtabwidget.h"

#include "multifilterlineparser.h"

#include <coreplugin/editormanager/editormanager.h>

#include <texteditor/fontsettings.h>
#include <texteditor/texteditorsettings.h>

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

    // Match Application Output's font, including live updates when the user changes it in
    // Preferences > Text Editor > Font & Colors -- Core::OutputWindow follows the same setting.
    setFont(TextEditor::TextEditorSettings::fontSettings().font());
    connect(
        TextEditor::TextEditorSettings::instance(),
        &TextEditor::TextEditorSettings::fontSettingsChanged,
        this,
        [this] { setFont(TextEditor::TextEditorSettings::fontSettings().font()); });

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

int OutputTabWidget::appendLine(
    const QString &line, Utils::OutputFormat format, int sourceBlockNumber)
{
    m_formatter.appendMessage(line + "\n", format);
    // appendMessage's trailing newline advances the cursor into a new, still-empty block, so
    // the block that actually holds the line just appended is the one before the current
    // last block.
    const int myBlock = document()->blockCount() - 2;
    m_sourceBlockNumbers.append(sourceBlockNumber >= 0 ? sourceBlockNumber : myBlock);

    // setMaximumBlockCount() silently discards blocks off the front once the cap is hit;
    // drop the matching front entries so the mapping stays aligned with the document.
    const qsizetype cap = maximumBlockCount();
    if (cap > 0 && m_sourceBlockNumbers.size() > cap)
        m_sourceBlockNumbers.remove(0, m_sourceBlockNumbers.size() - cap);

    ++m_matchCount;
    emit matchCountChanged(m_matchCount);
    return myBlock;
}

int OutputTabWidget::matchCount() const
{
    return m_matchCount;
}

void OutputTabWidget::reset()
{
    // Utils::OutputFormatter::clear() clears the attached text edit. Its sibling reset() is
    // deliberately NOT used: that one also deletes the installed line parsers, which would
    // silently disable link parsing for the rest of this tab's life. flush() first so any
    // in-flight incomplete line is materialized into the document and then cleared with it,
    // leaving the formatter with an empty carry-over buffer.
    m_formatter.flush();
    m_formatter.clear();
    m_sourceBlockNumbers.clear();
    m_matchCount = 0;
    emit matchCountChanged(m_matchCount);
}

void OutputTabWidget::scrollToBlock(int blockNumber)
{
    const QTextBlock block = document()->findBlockByNumber(blockNumber);
    if (!block.isValid())
        return;
    setTextCursor(QTextCursor(block));
    centerCursor();
}

void OutputTabWidget::mousePressEvent(QMouseEvent *event)
{
    m_pressPos = event->pos();
    QPlainTextEdit::mousePressEvent(event);
}

void OutputTabWidget::mouseReleaseEvent(QMouseEvent *event)
{
    // Drag-vs-click guard, in the spirit of Core::OutputWindow's linksActive handling: a
    // release that ends a drag, or that leaves a selection behind, is the user selecting
    // text -- not asking to follow a link or jump to the source line.
    const bool isClick = (event->pos() - m_pressPos).manhattanLength() < 4
                         && !textCursor().hasSelection();
    if (isClick) {
        const QString href = anchorAt(event->pos());
        if (!href.isEmpty()) {
            m_formatter.handleLink(href);
        } else {
            const int blockNumber = cursorForPosition(event->pos()).blockNumber();
            const bool known = blockNumber >= 0 && blockNumber < m_sourceBlockNumbers.size();
            emit jumpRequested(known ? m_sourceBlockNumbers.at(blockNumber) : -1);
        }
    }

    QPlainTextEdit::mouseReleaseEvent(event);
}

} // namespace MultiFilterOutput

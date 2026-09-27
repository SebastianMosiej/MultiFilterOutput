#include "outputtabwidget.h"

#include "multifilterlineparser.h"

#include <coreplugin/editormanager/editormanager.h>

#include <texteditor/fontsettings.h>

#include <utils/link.h>

#include <QAbstractScrollArea>
#include <QMouseEvent>
#include <QScrollBar>
#include <QTextBlock>
#include <QTextCursor>
#include <QWheelEvent>

namespace MultiFilterOutput {

OutputTabWidget::OutputTabWidget(QWidget *parent)
    : QPlainTextEdit(parent)
{
    setReadOnly(true);
    setMaximumBlockCount(50000);

    // Coalesces matchCountChanged(): appendLine() only starts this zero-interval timer, so a
    // burst of matches appended within one event-loop pass costs a single tab relabel.
    m_matchCountTimer.setSingleShot(true);
    m_matchCountTimer.setInterval(0);
    connect(&m_matchCountTimer, &QTimer::timeout, this, [this] {
        emit matchCountChanged(m_matchCount);
    });
    m_formatter.setPlainTextEdit(this);

    // Match Application Output's font, including live updates when the user changes it in
    // Preferences > Text Editor > Font & Colors -- Core::OutputWindow follows the same setting.
    setBaseFont(TextEditor::globalFontSettings().data().font());
    connect(
        &TextEditor::globalFontSettings(),
        &TextEditor::FontSettings::changed,
        this,
        [this] { setBaseFont(TextEditor::globalFontSettings().data().font()); });

    // An absolute "file://" link is resolved by OutputFormatter itself (see
    // Utils::OutputFormatter::handleFileLink()) rather than being forwarded to the line
    // parser's handleLink(); mirror Core::OutputWindow's wiring so such links still open
    // an editor.
    connect(
        &m_formatter,
        &Utils::OutputFormatter::openInEditorRequested,
        this,
        [](const Utils::Link &link) { Core::EditorManager::openEditorAt(link); });

    // Follow the end of the log like Application Output does. Any scroll -- the user's, or
    // scrollToLine()'s -- decides whether we keep following: leaving the bottom stops it,
    // returning to the bottom resumes it. Appended lines (and resizes/zoom) only grow the
    // range, so while following, snap back to the new bottom whenever the range changes.
    QScrollBar *bar = verticalScrollBar();
    connect(bar, &QScrollBar::valueChanged, this, [this, bar](int value) {
        m_followTail = value >= bar->maximum();
    });
    connect(bar, &QScrollBar::rangeChanged, this, [this, bar](int, int max) {
        if (m_followTail)
            bar->setValue(max);
    });
}

void OutputTabWidget::setLinkParser(ProjectExplorer::Project *project)
{
    m_formatter.setLineParsers({new Internal::MultiFilterLineParser(project)});
}

int OutputTabWidget::appendLine(
    const QString &line, Utils::OutputFormat format, int sourceLineId, bool countsAsMatch)
{
    const int lineId = sourceLineId >= 0 ? sourceLineId : m_nextLineId++;

    m_formatter.appendMessage(line + "\n", format);
    // appendMessage's trailing newline advances the cursor into a new, still-empty block, so
    // the block that actually holds the line just appended is the one before the last.
    // Nothing else sets userState on this document (there is no syntax highlighter), so it is
    // free to carry the line id -- see the class comment.
    document()->lastBlock().previous().setUserState(lineId);

    if (countsAsMatch) {
        ++m_matchCount;
        if (!m_matchCountTimer.isActive())
            m_matchCountTimer.start();
    }
    return lineId;
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
    m_nextLineId = 0;
    m_matchCount = 0;
    m_followTail = true;
    // Immediately, not coalesced: the label must not keep showing the old count until the
    // next event-loop pass -- and a still-pending coalesced emission is now stale.
    m_matchCountTimer.stop();
    emit matchCountChanged(m_matchCount);
}

bool OutputTabWidget::scrollToLine(int lineId)
{
    // Line ids only ever increase down the document, so walk up from the end: a jump target
    // is usually recent, and hitting a smaller id means the line has already been trimmed.
    // Blocks without an id (userState() == -1, e.g. the trailing empty block) are skipped.
    for (QTextBlock block = document()->lastBlock(); block.isValid(); block = block.previous()) {
        const int state = block.userState();
        if (state == lineId) {
            setTextCursor(QTextCursor(block));
            centerCursor();
            return true;
        }
        if (state >= 0 && state < lineId)
            break;
    }
    return false;
}

void OutputTabWidget::setBaseFont(const QFont &newFont)
{
    // Mirrors Core::OutputWindow::setBaseFont(): re-basing the font (e.g. a live Preferences
    // change) keeps whatever zoom offset is already applied, rather than discarding it.
    const qreal zoom = fontZoom();
    m_baseFontSize = newFont.pointSizeF();
    QFont tmp = newFont;
    tmp.setPointSizeF(qMax(m_baseFontSize + zoom, 4.0));
    setFont(tmp);
}

qreal OutputTabWidget::fontZoom() const
{
    return font().pointSizeF() - m_baseFontSize;
}

void OutputTabWidget::setFontZoom(qreal zoom)
{
    QFont f = font();
    f.setPointSizeF(qMax(m_baseFontSize + zoom, 4.0));
    setFont(f);
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
            emit jumpRequested(cursorForPosition(event->pos()).block().userState());
        }
    }

    QPlainTextEdit::mouseReleaseEvent(event);
}

void OutputTabWidget::wheelEvent(QWheelEvent *event)
{
    // Mirrors Core::OutputWindow::wheelEvent(): Ctrl+wheel zooms this tab; FilteredOutputPane
    // propagates fontZoomChanged() to every other tab in the pane so they all stay in sync
    // with each other.
    if (event->modifiers() & Qt::ControlModifier) {
        const float delta = event->angleDelta().y() / 120.f;
        if (delta < 0.f && font().pointSizeF() + delta < 4.f)
            return;
        zoomInF(delta);
        emit fontZoomChanged(fontZoom());
        return;
    }
    QAbstractScrollArea::wheelEvent(event);
}

} // namespace MultiFilterOutput

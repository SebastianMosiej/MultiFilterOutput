#include "outputtabwidget.h"

#include "multifilterlineparser.h"
#include "multifilteroutputtr.h"

#include <coreplugin/editormanager/editormanager.h>
#include <coreplugin/find/basetextfind.h>

#include <texteditor/fontsettings.h>

#include <utils/aggregate.h>
#include <utils/link.h>

#include <QAbstractScrollArea>
#include <QMouseEvent>
#include <QScrollBar>
#include <QTextBlock>
#include <QTextCursor>
#include <QWheelEvent>

namespace MultiFilterOutput {

namespace {

const QChar Escape(0x1b);

// If an ANSI escape sequence starts before `cut` but doesn't end before it, the position it
// starts at -- so the cut can move there instead of leaving half a sequence to be rendered
// as text. Otherwise `cut` unchanged. A CSI sequence ("ESC [" ...) ends at its final byte,
// 0x40-0x7e; any other sequence is ESC plus one character.
qsizetype cutBeforeOpenEscape(const QString &line, qsizetype cut)
{
    const qsizetype esc = line.lastIndexOf(Escape, cut - 1);
    if (esc < 0)
        return cut;
    if (esc + 1 >= cut)
        return esc;
    if (line.at(esc + 1) != '[')
        return cut;
    for (qsizetype i = esc + 2; i < cut; ++i) {
        const char16_t c = line.at(i).unicode();
        if (c >= 0x40 && c <= 0x7e)
            return cut;
    }
    return esc;
}

// `line` as shown in a tab with the given setMaxLineLength(): cut after maxLength characters
// -- earlier rather than separate a surrogate pair or split an escape sequence -- and
// followed by how much was left out.
QString displayText(const QString &line, int maxLength)
{
    if (maxLength <= 0 || line.size() <= maxLength)
        return line;
    qsizetype cut = maxLength;
    if (cut > 1 && line.at(cut - 1).isHighSurrogate())
        --cut;
    cut = cutBeforeOpenEscape(line, cut);
    QString kept = line.left(cut);
    // Utils::OutputFormatter carries ANSI state from one line to the next, so if this line
    // set a color, the reset that would have ended it may be among what was cut: reset
    // explicitly, or every later line in the tab stays colored.
    if (line.contains(Escape))
        kept += Escape + QStringLiteral("[0m");
    return Tr::tr("%1 ... [%2 more characters not shown]")
        .arg(kept, QString::number(line.size() - cut)); // not chained: see tabLabel()
}

} // namespace

int nextLineId(int id, int modulus)
{
    return id + 1 >= modulus ? 0 : id + 1;
}

qint64 lineIdAge(int newest, int target, int modulus)
{
    // In 64 bits: the difference of two ints near the ends of the range overflows an int.
    const qint64 age = (qint64(newest) - target) % modulus;
    return age < 0 ? age + modulus : age;
}

OutputTabWidget::OutputTabWidget(QWidget *parent)
    : QPlainTextEdit(parent)
{
    setReadOnly(true);
    setMaximumBlockCount(50000);

    // Ctrl+F: Qt Creator's Find toolbar searches the IFindSupport aggregated with the focused
    // widget (or a parent), and every output pane shares a Find toolbar placeholder -- so
    // this is all it takes, exactly as Core::OutputWindow does it. The aggregate owns the
    // find support and deletes it along with this widget.
    Utils::Aggregation::aggregate({this, new Core::BaseTextFind<QPlainTextEdit>(this)});

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
    int lineId = sourceLineId;
    if (lineId < 0) {
        lineId = m_nextLineId;
        m_nextLineId = nextLineId(m_nextLineId);
    }

    m_formatter.appendMessage(displayText(line, m_maxLineLength) + "\n", format);
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
    m_matchStops = 0;
    m_matchCount = 0;
    m_followTail = true;
    // Immediately, not coalesced: the label must not keep showing the old count until the
    // next event-loop pass -- and a still-pending coalesced emission is now stale.
    m_matchCountTimer.stop();
    emit matchCountChanged(m_matchCount);
}

bool OutputTabWidget::scrollToLine(int lineId)
{
    const auto show = [this](const QTextBlock &block) {
        setTextCursor(QTextCursor(block));
        centerCursor();
        return true;
    };

    // In a tab numbering its own lines (All -- the only jump target), ids are consecutive,
    // one block per line, wrapping at LineIdModulus. So the target is `age` blocks above the
    // newest line, or already trimmed if it is older than the tab holds. Comparing ages
    // rather than ids stays right when the ids wrapped in between.
    const QTextBlock newest = document()->lastBlock().previous(); // last is the empty one
    if (!newest.isValid() || newest.userState() < 0)
        return false;
    const qint64 age = lineIdAge(newest.userState(), lineId);
    if (age > newest.blockNumber())
        return false;
    const QTextBlock candidate = document()->findBlockByNumber(int(newest.blockNumber() - age));
    if (candidate.userState() == lineId)
        return show(candidate);

    // Not where consecutive ids put it (a tab given ids by another, say): look for it by
    // equality alone, which assumes no ordering at all.
    for (QTextBlock block = newest; block.isValid(); block = block.previous()) {
        if (block.userState() == lineId)
            return show(block);
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

namespace {

// Block format property marking a line as a match stop (see markLastLineAsMatch()).
constexpr int MatchStopProperty = QTextFormat::UserProperty + 1;

bool isMatchStop(const QTextBlock &block)
{
    return block.blockFormat().boolProperty(MatchStopProperty);
}

} // namespace

void OutputTabWidget::markLastLineAsMatch()
{
    // The last block is the empty one appendLine()'s newline opened; the line is before it.
    const QTextBlock block = document()->lastBlock().previous();
    if (!block.isValid())
        return;
    QTextBlockFormat format = block.blockFormat();
    format.setProperty(MatchStopProperty, true);
    QTextCursor(block).setBlockFormat(format);
    ++m_matchStops;
}

bool OutputTabWidget::hasMatchStops() const
{
    return m_matchStops > 0;
}

int OutputTabWidget::goToMatch(Direction direction)
{
    if (m_matchStops == 0)
        return -1;
    const bool forward = direction == Direction::Forward;
    const QTextBlock start = textCursor().block();

    // Every block at most once, starting just past the current one and wrapping around, so
    // the current line itself comes last -- and is found if it is the only match.
    QTextBlock block = forward ? start.next() : start.previous();
    for (int visited = 0; visited < document()->blockCount(); ++visited) {
        if (!block.isValid())
            block = forward ? document()->firstBlock() : document()->lastBlock();
        if (isMatchStop(block)) {
            QTextCursor cursor(block);
            cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
            setTextCursor(cursor);
            centerCursor();
            return block.userState();
        }
        block = forward ? block.next() : block.previous();
    }
    return -1;
}

Utils::Result<> OutputTabWidget::saveContents(const Utils::FilePath &path) const
{
    const Utils::Result<qint64> written = path.writeFileContents(toPlainText().toUtf8());
    if (!written)
        return Utils::ResultError(written.error());
    return Utils::ResultOk;
}

void OutputTabWidget::setMaxLineLength(int maxLength)
{
    m_maxLineLength = maxLength;
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

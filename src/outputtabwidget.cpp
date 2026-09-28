#include "outputtabwidget.h"

#include "qtccompat.h"

#include "linematcher.h"
#include "multifilterlineparser.h"
#include "multifilteroutputtr.h"
#include "pattern.h"

#include <coreplugin/editormanager/editormanager.h>
#include <coreplugin/find/basetextfind.h>


#include <utils/link.h>

#include <QAbstractScrollArea>
#include <QDateTime>
#include <QMouseEvent>
#include <QPainter>
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

// Block format property marking a line as a match stop (see markLastLineAsMatch()).
constexpr int MatchStopProperty = QTextFormat::UserProperty + 1;
// The line's Utils::OutputFormat, and whether it is the pane's own notice (see lines()).
constexpr int OutputFormatProperty = QTextFormat::UserProperty + 2;
constexpr int NoticeProperty = QTextFormat::UserProperty + 3;
// When the line arrived, ms since the epoch (see appendLine()).
constexpr int TimestampProperty = QTextFormat::UserProperty + 4;
// The line is the previous run's output, kept greyed out (see grayOutOldContent()).
constexpr int OldRunProperty = QTextFormat::UserProperty + 5;

void setBlockProperty(const QTextBlock &block, int property, const QVariant &value)
{
    QTextBlockFormat format = block.blockFormat();
    format.setProperty(property, value);
    QTextCursor(block).setBlockFormat(format);
}

bool isMatchStop(const QTextBlock &block)
{
    return block.blockFormat().boolProperty(MatchStopProperty);
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

// The strip left of the text that the timestamps are painted in (like Qt's line-number-area
// example): a plain child widget in the tab's left viewport margin, painted by the tab.
class TimestampGutter final : public QWidget
{
public:
    explicit TimestampGutter(OutputTabWidget *tab)
        : QWidget(tab)
        , m_tab(tab)
    {}

protected:
    void paintEvent(QPaintEvent *event) override { m_tab->paintTimestampGutter(event); }

private:
    OutputTabWidget *m_tab;
};

OutputTabWidget::OutputTabWidget(QWidget *parent)
    : QPlainTextEdit(parent)
{
    setReadOnly(true);
    // Like Core::OutputWindow: an output view has no use for undo, and keeping it would store
    // every appended line a second time. (A non-zero maximumBlockCount used to disable it as a
    // side effect; the limit is in characters now, see setMaxCharacterCount().)
    setUndoRedoEnabled(false);

    m_timestampGutter = new TimestampGutter(this);
    m_timestampGutter->hide();
    connect(this, &QPlainTextEdit::updateRequest, this, [this](const QRect &rect, int dy) {
        if (!m_showTimestamps)
            return;
        if (dy != 0)
            m_timestampGutter->scroll(0, dy);
        else
            m_timestampGutter->update(0, rect.y(), m_timestampGutter->width(), rect.height());
    });

    // Ctrl+F: Qt Creator's Find toolbar searches the IFindSupport aggregated with the focused
    // widget (or a parent), and every output pane shares a Find toolbar placeholder -- so
    // this is all it takes, exactly as Core::OutputWindow does it. The aggregate owns the
    // find support and deletes it along with this widget.
    Compat::Aggregation::aggregate({this, new Core::BaseTextFind<QPlainTextEdit>(this)});

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
    setBaseFont(Compat::textEditorFont());
    Compat::onTextEditorFontChanged(this, [this] { setBaseFont(Compat::textEditorFont()); });

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
    const QString &line,
    Utils::OutputFormat format,
    int sourceLineId,
    bool countsAsMatch,
    qint64 timestamp)
{
    int lineId = sourceLineId;
    if (lineId < 0) {
        lineId = m_nextLineId;
        m_nextLineId = nextLineId(m_nextLineId);
    }

    const QString shown = displayText(line, m_maxLineLength);
    makeRoomFor(shown.size() + 1); // + its block separator
    m_formatter.appendMessage(shown + "\n", format);
    // appendMessage's trailing newline advances the cursor into a new, still-empty block, so
    // the block that actually holds the line just appended is the one before the last.
    // Nothing else sets userState on this document (there is no syntax highlighter), so it is
    // free to carry the line id -- see the class comment.
    QTextBlock block = document()->lastBlock().previous();
    block.setUserState(lineId);
    setBlockProperty(block, OutputFormatProperty, int(format));
    if (timestamp > 0)
        setBlockProperty(block, TimestampProperty, timestamp);
    if (!m_filter.text.isEmpty())
        filterAppendedLine(block);

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
    m_afterContextRemaining = 0;
    m_matchCount = 0;
    m_followTail = true;
    // Immediately, not coalesced: the label must not keep showing the old count until the
    // next event-loop pass -- and a still-pending coalesced emission is now stale.
    m_matchCountTimer.stop();
    emit matchCountChanged(m_matchCount);
}

bool OutputTabWidget::scrollToLine(int lineId)
{
    const QTextBlock block = blockForLine(lineId);
    if (!block.isValid())
        return false;
    setTextCursor(QTextCursor(block));
    centerCursor();
    return true;
}

QTextBlock OutputTabWidget::blockForLine(int lineId) const
{
    // In a tab numbering its own lines (All -- the only jump target), ids are consecutive,
    // one block per line, wrapping at LineIdModulus. So the target is `age` blocks above the
    // newest line, or already trimmed if it is older than the tab holds. Comparing ages
    // rather than ids stays right when the ids wrapped in between.
    const QTextBlock newest = document()->lastBlock().previous(); // last is the empty one
    if (!newest.isValid() || newest.userState() < 0)
        return {};
    const qint64 age = lineIdAge(newest.userState(), lineId);
    if (age > newest.blockNumber())
        return {};
    const QTextBlock candidate = document()->findBlockByNumber(int(newest.blockNumber() - age));
    if (candidate.userState() == lineId)
        return candidate;

    // Not where consecutive ids put it (a tab given ids by another, say): look for it by
    // equality alone, which assumes no ordering at all.
    for (QTextBlock block = newest; block.isValid(); block = block.previous()) {
        if (block.userState() == lineId)
            return block;
    }
    return {};
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

void OutputTabWidget::markLastLineAsMatch()
{
    // The last block is the empty one appendLine()'s newline opened; the line is before it.
    const QTextBlock block = document()->lastBlock().previous();
    if (!block.isValid())
        return;
    setBlockProperty(block, MatchStopProperty, true);
    ++m_matchStops;
}

void OutputTabWidget::clearMatchStops()
{
    for (QTextBlock block = document()->firstBlock(); block.isValid(); block = block.next()) {
        if (isMatchStop(block))
            setBlockProperty(block, MatchStopProperty, false);
    }
    m_matchStops = 0;
}

void OutputTabWidget::markLineAsMatch(int lineId)
{
    const QTextBlock block = blockForLine(lineId);
    if (!block.isValid() || isMatchStop(block))
        return;
    setBlockProperty(block, MatchStopProperty, true);
    ++m_matchStops;
}

QString OutputTabWidget::lastLineText() const
{
    // The last block is the empty one appendLine()'s newline opened; the line is before it.
    return document()->lastBlock().previous().text();
}

void OutputTabWidget::highlightLastLine(const QList<MatchSpan> &spans, const QColor &color)
{
    const QTextBlock block = document()->lastBlock().previous();
    if (!block.isValid() || spans.isEmpty())
        return;
    const qsizetype lineLength = block.length() - 1; // without the block separator
    QTextCharFormat tint;
    tint.setBackground(color);
    for (const MatchSpan &span : spans) {
        const qsizetype start = qBound<qsizetype>(0, span.start, lineLength);
        const qsizetype end = qBound<qsizetype>(start, span.start + span.length, lineLength);
        if (start == end)
            continue;
        QTextCursor cursor(block);
        cursor.setPosition(int(block.position() + start));
        cursor.setPosition(int(block.position() + end), QTextCursor::KeepAnchor);
        // Merged, not set: keeps a linkified file reference's own formatting (see
        // MultiFilterLineParser) and the output format's text color.
        cursor.mergeCharFormat(tint);
    }
}

// Halfway from the text color to the background, like Core::OutputWindow::grayOutOldContent().
static QTextCharFormat oldContentFormat(const QPalette &palette)
{
    const QColor background = palette.base().color();
    const QColor text = palette.text().color();
    QTextCharFormat format;
    format.setForeground(QColor(
        (background.red() + text.red()) / 2,
        (background.green() + text.green()) / 2,
        (background.blue() + text.blue()) / 2));
    return format;
}

static void markBlockAsOld(const QTextBlock &block, const QTextCharFormat &format)
{
    setBlockProperty(block, OldRunProperty, true);
    QTextCursor cursor(block);
    cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
    // Merged: links keep being links, only the color changes.
    cursor.mergeCharFormat(format);
}

void OutputTabWidget::grayOutOldContent()
{
    const QTextCharFormat format = oldContentFormat(palette());
    for (QTextBlock b = document()->firstBlock(); b != document()->lastBlock(); b = b.next()) {
        if (!b.blockFormat().boolProperty(OldRunProperty))
            markBlockAsOld(b, format);
    }
    clearMatchStops();
    m_matchCount = 0;
    m_matchCountTimer.stop();
    emit matchCountChanged(m_matchCount);
}

void OutputTabWidget::markLastLineAsOld()
{
    const QTextBlock block = document()->lastBlock().previous();
    if (block.isValid())
        markBlockAsOld(block, oldContentFormat(palette()));
}

void OutputTabWidget::markLastLineAsNotice()
{
    const QTextBlock block = document()->lastBlock().previous();
    if (block.isValid())
        setBlockProperty(block, NoticeProperty, true);
}

static OutputTabWidget::Line lineFromBlock(const QTextBlock &block)
{
    const QTextBlockFormat format = block.blockFormat();
    return {
        block.text(),
        Utils::OutputFormat(format.intProperty(OutputFormatProperty)),
        block.userState(),
        format.boolProperty(NoticeProperty),
        format.property(TimestampProperty).toLongLong(),
        format.boolProperty(OldRunProperty)};
}

std::optional<OutputTabWidget::Line> OutputTabWidget::line(int lineId) const
{
    const QTextBlock block = blockForLine(lineId);
    if (!block.isValid())
        return std::nullopt;
    return lineFromBlock(block);
}

QList<OutputTabWidget::Line> OutputTabWidget::lines() const
{
    QList<Line> result;
    for (QTextBlock b = document()->firstBlock(); b != document()->lastBlock(); b = b.next())
        result.append(lineFromBlock(b));
    return result;
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

namespace {

// Whether `filter` lets a line with this (displayed) text through -- matched the way pattern
// tabs match, so the quick filter and the Pattern Library agree on what e.g. an inverted,
// case-insensitive regex means.
bool filterMatches(const LineFilter &filter, const QString &text)
{
    if (filter.text.isEmpty())
        return true;
    Pattern pattern;
    pattern.patternText = filter.text;
    pattern.isRegex = filter.isRegex;
    pattern.caseSensitive = filter.caseSensitivity == Qt::CaseSensitive;
    pattern.invert = filter.inverted;
    return lineMatches(pattern, text);
}

} // namespace

void OutputTabWidget::setFilter(const LineFilter &filter)
{
    if (filter == m_filter)
        return;
    m_filter = filter;

    // Every line but the trailing empty block, which always stays visible.
    QList<QTextBlock> blocks;
    for (QTextBlock b = document()->firstBlock(); b != document()->lastBlock(); b = b.next())
        blocks << b;
    const bool showAll = filter.text.isEmpty();
    QList<bool> visible(blocks.size(), showAll);
    qsizetype lastMatch = -1;
    if (!showAll) {
        for (qsizetype i = 0; i < blocks.size(); ++i) {
            if (!filterMatches(filter, blocks.at(i).text()))
                continue;
            lastMatch = i;
            const qsizetype from = qMax<qsizetype>(0, i - filter.beforeContext);
            const qsizetype to = qMin(blocks.size() - 1, i + filter.afterContext);
            for (qsizetype k = from; k <= to; ++k)
                visible[k] = true;
        }
    }
    // Lines appended from now on may still be after-context of the last match.
    m_afterContextRemaining = lastMatch < 0
        ? 0
        : qMax<qsizetype>(0, filter.afterContext - (blocks.size() - 1 - lastMatch));

    for (qsizetype i = 0; i < blocks.size(); ++i)
        blocks[i].setVisible(visible.at(i));

    // Re-lays the document out for the changed visibility -- what Core::OutputWindow does
    // after filtering, too.
    setDocument(document());
    if (m_followTail)
        verticalScrollBar()->setValue(verticalScrollBar()->maximum());
}

void OutputTabWidget::filterAppendedLine(QTextBlock block)
{
    const auto setVisible = [this](QTextBlock b, bool visible) {
        if (b.isVisible() == visible)
            return;
        b.setVisible(visible);
        document()->markContentsDirty(b.position(), b.length());
    };

    if (filterMatches(m_filter, block.text())) {
        setVisible(block, true);
        QTextBlock before = block.previous();
        for (int i = 0; i < m_filter.beforeContext && before.isValid(); ++i, before = before.previous())
            setVisible(before, true);
        m_afterContextRemaining = m_filter.afterContext;
    } else if (m_afterContextRemaining > 0) {
        setVisible(block, true);
        --m_afterContextRemaining;
    } else {
        setVisible(block, false);
    }
}

LineFilter OutputTabWidget::filter() const
{
    return m_filter;
}

Utils::Result<> OutputTabWidget::saveContents(const Utils::FilePath &path) const
{
    const Utils::Result<qint64> written = path.writeFileContents(toPlainText().toUtf8());
    if (!written)
        return Utils::ResultError(written.error());
    return Utils::ResultOk;
}

QString formatTimestamp(qint64 msecsSinceEpoch)
{
    return QDateTime::fromMSecsSinceEpoch(msecsSinceEpoch).toString("hh:mm:ss.zzz");
}

void OutputTabWidget::setShowTimestamps(bool show)
{
    if (show == m_showTimestamps)
        return;
    m_showTimestamps = show;
    layoutTimestampGutter();
}

bool OutputTabWidget::showTimestamps() const
{
    return m_showTimestamps;
}

int OutputTabWidget::timestampGutterWidth() const
{
    return m_showTimestamps ? fontMetrics().horizontalAdvance(QStringLiteral("00:00:00.000")) + 8
                            : 0;
}

void OutputTabWidget::layoutTimestampGutter()
{
    const int width = timestampGutterWidth();
    setViewportMargins(width, 0, 0, 0);
    const QRect area = contentsRect();
    m_timestampGutter->setGeometry(area.left(), area.top(), width, area.height());
    m_timestampGutter->setVisible(m_showTimestamps);
    m_timestampGutter->update();
}

void OutputTabWidget::resizeEvent(QResizeEvent *event)
{
    QPlainTextEdit::resizeEvent(event);
    layoutTimestampGutter();
}

void OutputTabWidget::changeEvent(QEvent *event)
{
    QPlainTextEdit::changeEvent(event);
    // Zoom and the Preferences font change the width "00:00:00.000" needs.
    if (event->type() == QEvent::FontChange)
        layoutTimestampGutter();
}

void OutputTabWidget::paintTimestampGutter(QPaintEvent *event)
{
    QPainter painter(m_timestampGutter);
    painter.fillRect(event->rect(), palette().color(QPalette::AlternateBase));
    painter.setPen(palette().color(QPalette::PlaceholderText));
    painter.setFont(font());

    const int right = m_timestampGutter->width() - 4;
    QTextBlock block = firstVisibleBlock();
    qreal top = blockBoundingGeometry(block).translated(contentOffset()).top();
    while (block.isValid() && top <= event->rect().bottom()) {
        const qreal height = blockBoundingRect(block).height();
        // Hidden by the quick filter: takes no room and gets no timestamp.
        if (block.isVisible() && top + height >= event->rect().top()) {
            const qint64 timestamp = block.blockFormat().property(TimestampProperty).toLongLong();
            if (timestamp > 0) {
                painter.drawText(
                    QRectF(0, top, right, fontMetrics().height()),
                    Qt::AlignRight | Qt::AlignVCenter,
                    formatTimestamp(timestamp));
            }
        }
        top += height;
        block = block.next();
    }
}

void OutputTabWidget::setMaxCharacterCount(qsizetype maxCharacters)
{
    m_maxCharacters = maxCharacters;
    makeRoomFor(0); // a lower limit applies to what the tab already holds, too
}

void OutputTabWidget::makeRoomFor(qsizetype incomingCharacters)
{
    if (m_maxCharacters <= 0)
        return;
    // As Core::OutputWindow does it: drop whole blocks off the front -- the same way
    // setMaximumBlockCount() trims, which line ids and match stops are built to survive --
    // until the new text fits, but never the last block, which the new line goes into.
    qsizetype planned = document()->characterCount() + incomingCharacters;
    if (planned <= m_maxCharacters)
        return;
    int keepBlocks = document()->blockCount();
    for (QTextBlock b = document()->firstBlock();
         b.isValid() && planned > m_maxCharacters && keepBlocks > 1;
         b = b.next()) {
        planned -= b.length();
        --keepBlocks;
    }
    setMaximumBlockCount(keepBlocks);
    setMaximumBlockCount(0); // trimming done; 0 = no block limit
    setUndoRedoEnabled(false);
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

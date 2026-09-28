#pragma once

#include "multifilteroutput_global.h"

#include <utils/filepath.h>
#include <utils/outputformatter.h>
#include <utils/result.h>

#include <QPlainTextEdit>
#include <QTextBlock>
#include <QPoint>
#include <QTimer>

#include <limits>

QT_BEGIN_NAMESPACE
class QWheelEvent;
QT_END_NAMESPACE

namespace ProjectExplorer {
class Project;
}

namespace MultiFilterOutput {

// Line ids (see OutputTabWidget) live in QTextBlock::userState(), an int where -1 means "no
// id", so they cycle through [0, LineIdModulus) rather than growing until they overflow.
constexpr int LineIdModulus = std::numeric_limits<int>::max();

// The id after `id`, wrapping to 0 at `modulus`.
MULTIFILTEROUTPUT_EXPORT int nextLineId(int id, int modulus = LineIdModulus);

// How many lines before `newest` the line with id `target` was appended, assuming consecutive
// ids that wrap at `modulus` -- i.e. (newest - target) mod modulus. A target "newer" than
// `newest` therefore comes out as a huge age, which reads as long gone.
MULTIFILTEROUTPUT_EXPORT qint64 lineIdAge(int newest, int target, int modulus = LineIdModulus);

// The pane's quick filter (the "Filter output..." box), as applied to one tab. Empty text
// shows everything.
struct MULTIFILTEROUTPUT_EXPORT LineFilter
{
    QString text;
    bool isRegex = false;
    Qt::CaseSensitivity caseSensitivity = Qt::CaseInsensitive;
    bool inverted = false;
    // Lines shown before/after each matching line, as context.
    int beforeContext = 0;
    int afterContext = 0;

    bool operator==(const LineFilter &) const = default;
};

// A single output tab: owns a Utils::OutputFormatter that appends formatted, optionally
// linkified lines to itself. Used by FilteredOutputPane both for the "All" tab and for each
// enabled pattern's tab. A click that doesn't land on a linkified reference emits
// jumpRequested() carrying the "All" tab line id recorded for the clicked line, which
// FilteredOutputPane turns into "show the All tab, scrolled to that line".
//
// Lines are identified by a line id rather than a block number: setMaximumBlockCount()
// discards blocks off the front once the cap is hit, renumbering every block that remains,
// whereas a line id is stored on the line's own block (QTextBlock::userState()) and so stays
// attached to it -- and disappears with it -- however much is trimmed. Ids wrap at
// LineIdModulus instead of overflowing.
class MULTIFILTEROUTPUT_EXPORT OutputTabWidget : public QPlainTextEdit
{
    Q_OBJECT

public:
    explicit OutputTabWidget(QWidget *parent = nullptr);

    void setLinkParser(ProjectExplorer::Project *project); // wires MultiFilterLineParser

    // Appends one line and returns its line id, for use as the sourceLineId argument on the
    // subsequent calls that mirror this same line into other tabs. The "All" tab is appended
    // to first with no sourceLineId, so it numbers its own lines 0, 1, 2, ...; each matching
    // pattern tab is then given the id "All" returned, so a click in a pattern tab can be
    // mapped back to the right line of "All". countsAsMatch is false for a continuation line
    // swept in alongside a real match (see FilteredOutputPane::onLineReceived) -- it's still
    // shown, just not counted.
    int appendLine(
        const QString &line, Utils::OutputFormat format, int sourceLineId = -1,
        bool countsAsMatch = true);

    int matchCount() const;

    // Empties the tab completely: document, line ids, match count (with a
    // matchCountChanged(0) so the tab label follows) and the formatter's in-flight state.
    void reset();

    // Scrolls to and selects the line with the given line id. Returns false, leaving the view
    // untouched, if no such line is (any longer) in this tab.
    bool scrollToLine(int lineId);

    // Match stops for next/previous-match navigation (F6 / Shift+F6): which lines count is
    // the caller's decision -- FilteredOutputPane marks a pattern tab's matches (not its
    // continuation lines) and the All lines that matched any pattern. Stored on the line's
    // own block, so marks survive trimming like line ids do.
    void markLastLineAsMatch();
    bool hasMatchStops() const;
    // For re-matching the tab's lines against changed patterns: forget every match stop, then
    // mark lines again by line id (a no-op for an id no longer in the tab).
    void clearMatchStops();
    void markLineAsMatch(int lineId);

    // A note the pane itself added (e.g. "... lines dropped"), not a line of the app's output.
    void markLastLineAsNotice();

    // The lines the tab still holds, oldest first, as shown -- for replaying All through
    // changed patterns.
    struct Line
    {
        QString text;
        Utils::OutputFormat format;
        int lineId;
        bool isNotice;
    };
    QList<Line> lines() const;

    enum class Direction { Forward, Backward };
    // Moves the cursor to the next/previous marked line after/before the current one,
    // wrapping around at the end/start, selects that line and scrolls to it. Returns its line
    // id (see appendLine()), or -1 -- leaving the view alone -- if no line is marked.
    int goToMatch(Direction direction);

    // Sets the font this tab treats as "unzoomed", preserving whatever zoom offset (if any)
    // is currently applied -- e.g. when the Preferences > Text Editor font changes live.
    void setBaseFont(const QFont &font);

    // The current zoom offset from the base font, in points. Applying the same value to
    // every tab in a FilteredOutputPane (see FilteredOutputPane::onTabZoomChanged) is how
    // they're kept in sync with each other.
    qreal fontZoom() const;
    void setFontZoom(qreal zoom);

    // Lines longer than this many characters are shown cut short, followed by a note saying
    // how much was left out -- QPlainTextEdit's layout cost grows steeply with block length,
    // so one enormous line can stall the whole pane. Display only: what matched is decided
    // on the full line before it gets here. 0 (the default) means no limit. Applies to lines
    // appended from now on.
    void setMaxLineLength(int maxLength);

    // Keeps at most this many characters, dropping the oldest lines as new ones arrive --
    // Application Output's own kind of limit (see Core::OutputWindow::setMaxCharCount()), so
    // the pane can follow its setting. The newest line is always kept, even if it alone is
    // longer. 0 (the default) means no limit.
    void setMaxCharacterCount(qsizetype maxCharacters);

    // Writes the tab's text, as shown, to `path` as UTF-8 -- "Save tab to file" in the pane's
    // toolbar.
    Utils::Result<> saveContents(const Utils::FilePath &path) const;

    // Hides the lines the filter doesn't match (plus any context lines around those it does),
    // and keeps doing so for lines appended from now on. Setting the filter it already has is
    // a no-op.
    void setFilter(const LineFilter &filter);
    LineFilter filter() const;

signals:
    // Coalesced: emitted once per event-loop pass however many matches were appended in it
    // (except from reset(), which emits 0 straight away).
    void matchCountChanged(int count);
    // Emitted on a click that isn't a linkified reference; sourceLineId is the line id this
    // tab recorded for the clicked line, or -1 if there is none.
    void jumpRequested(int sourceLineId);
    // Emitted only when this tab's own wheelEvent() changes the zoom (not when setFontZoom()
    // is called to apply an externally-driven value), so listeners can propagate it onward
    // without feeding back into a loop.
    void fontZoomChanged(qreal zoom);

protected:
    // The block holding the line with this id, or an invalid block if the tab no longer
    // holds it.
    QTextBlock blockForLine(int lineId) const;

    // Drops the oldest lines so that incomingCharacters more fit in the character limit.
    void makeRoomFor(qsizetype incomingCharacters);

    // Applies the current filter to a line just appended.
    void filterAppendedLine(QTextBlock block);

    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

private:
    Utils::OutputFormatter m_formatter;
    // The line id appendLine() gives the next line appended without a sourceLineId.
    int m_nextLineId = 0;
    // Lines marked by markLastLineAsMatch(); only ever counts up (trimming doesn't lower it),
    // so hasMatchStops() may say yes while every marked line is already gone -- goToMatch()
    // then just finds nothing.
    int m_matchStops = 0;
    int m_maxLineLength = 0;
    qsizetype m_maxCharacters = 0;
    LineFilter m_filter;
    // After-context lines still to reveal after the last matching line appended.
    int m_afterContextRemaining = 0;
    QPoint m_pressPos;
    int m_matchCount = 0;
    QTimer m_matchCountTimer;
    // The point size setBaseFont() was last given, i.e. the size fontZoom() is relative to.
    qreal m_baseFontSize = 0;
    // Whether the view sticks to the end of the log as lines arrive: true while the vertical
    // scroll bar sits at the bottom, false once the user scrolls anywhere else, and true
    // again when they scroll back down to the bottom.
    bool m_followTail = true;
};

} // namespace MultiFilterOutput

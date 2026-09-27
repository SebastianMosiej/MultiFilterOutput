#pragma once

#include "multifilteroutput_global.h"

#include <utils/outputformatter.h>

#include <QPlainTextEdit>
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
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

private:
    Utils::OutputFormatter m_formatter;
    // The line id appendLine() gives the next line appended without a sourceLineId.
    int m_nextLineId = 0;
    int m_maxLineLength = 0;
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

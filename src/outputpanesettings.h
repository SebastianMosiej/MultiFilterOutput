#pragma once

#include "multifilteroutput_global.h"

#include <QObject>

namespace MultiFilterOutput {

enum class OutputPanePlacement { NextToApplicationOutput, AtEnd };

// Persisted, observable setting for where the MultiFilterOutput pane's status-bar button
// sits. Deliberately bypasses the Options dialog's Apply/Cancel working-copy convention (see
// PatternLibrarySettingsPage): setPlacement() persists and notifies immediately, since the
// requirement is that the pane visibly moves right away, not after the dialog is confirmed.
class MULTIFILTEROUTPUT_EXPORT OutputPaneSettings : public QObject
{
    Q_OBJECT

public:
    OutputPanePlacement placement() const;
    void setPlacement(OutputPanePlacement placement);

    // Whether clicking a non-link line in a pattern tab switches to the All tab and scrolls
    // to that line. Read fresh at click time (see FilteredOutputPane::onJumpRequested), so
    // unlike placement this needs no change signal.
    bool jumpToAllOnClick() const;
    void setJumpToAllOnClick(bool enabled);

    // Whether FilteredOutputPane should pop itself to the front, with focus, the first time
    // output arrives for a newly-started run -- read fresh by FilteredOutputPane at that
    // moment (see FilteredOutputPane::onLineReceived), so like jumpToAllOnClick this needs no
    // change signal.
    bool focusPaneOnRunStart() const;
    void setFocusPaneOnRunStart(bool enabled);

    static constexpr int DefaultMaxDisplayedLineLength = 10000;

    // How many characters of a line every tab (All and each pattern tab) shows before cutting
    // it short -- see OutputTabWidget::setMaxLineLength(). 0 means no limit. Unlike the two
    // settings above this has a change signal: FilteredOutputPane pushes a new value to its
    // open tabs straight away. Only affects lines appended after the change.
    int maxDisplayedLineLength() const;
    void setMaxDisplayedLineLength(int length);

    // Whether every tab word-wraps long lines, toggled from the pane's toolbar. On by default,
    // like Application Output's "Word-wrap output". Applied to open tabs straight away (see
    // wordWrapChanged()).
    bool wordWrap() const;
    void setWordWrap(bool enabled);

    // Whether the pane adds an "Unmatched" tab -- the lines no (non-inverted) pattern of the
    // run took -- whenever the run has at least one non-inverted pattern. On by default;
    // applied to the open pane straight away (see showUnmatchedTabChanged()).
    bool showUnmatchedTab() const;
    void setShowUnmatchedTab(bool enabled);

    // How much output each tab keeps, in characters -- the oldest lines go first (see
    // OutputTabWidget::setMaxCharacterCount()). 0, the default, means the same as
    // Application Output (its "Limit output to ... characters" setting). Applied to open
    // tabs straight away.
    qsizetype maxCharactersPerTab() const;
    void setMaxCharactersPerTab(qsizetype characters);

    // Whether matched text is also highlighted in the All tab (in each pattern's color), not
    // just in the pattern tabs. Off by default. Read fresh for every line, so a change applies
    // to output arriving from then on.
    bool highlightMatchesInAll() const;
    void setHighlightMatchesInAll(bool enabled);

    // Whether every tab shows when each line arrived, in a gutter left of the text (see
    // OutputTabWidget::setShowTimestamps()); toggled from the pane's toolbar. Off by default.
    bool showTimestamps() const;
    void setShowTimestamps(bool enabled);

    // How many stopped runs' output the pane keeps so the run selector can switch back to
    // them (see RunTracker::setFinishedRunsKept()); 0 drops a run's output when it stops.
    static constexpr int DefaultFinishedRunsKept = 3;
    int finishedRunsKept() const;
    void setFinishedRunsKept(int count);

    void load();

signals:
    void placementChanged(OutputPanePlacement placement);
    void maxDisplayedLineLengthChanged(int length);
    void wordWrapChanged(bool enabled);
    void showUnmatchedTabChanged(bool enabled);
    void maxCharactersPerTabChanged(qsizetype characters);
    void showTimestampsChanged(bool enabled);
    void finishedRunsKeptChanged(int count);

private:
    OutputPanePlacement m_placement = OutputPanePlacement::NextToApplicationOutput;
    bool m_jumpToAllOnClick = true;
    bool m_focusPaneOnRunStart = false;
    int m_maxDisplayedLineLength = DefaultMaxDisplayedLineLength;
    bool m_wordWrap = true;
    bool m_showUnmatchedTab = true;
    qsizetype m_maxCharactersPerTab = 0;
    bool m_highlightMatchesInAll = false;
    bool m_showTimestamps = false;
    int m_finishedRunsKept = DefaultFinishedRunsKept;
};

MULTIFILTEROUTPUT_EXPORT OutputPaneSettings &outputPaneSettings();

} // namespace MultiFilterOutput

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

    void load();

signals:
    void placementChanged(OutputPanePlacement placement);
    void maxDisplayedLineLengthChanged(int length);

private:
    OutputPanePlacement m_placement = OutputPanePlacement::NextToApplicationOutput;
    bool m_jumpToAllOnClick = true;
    bool m_focusPaneOnRunStart = false;
    int m_maxDisplayedLineLength = DefaultMaxDisplayedLineLength;
};

MULTIFILTEROUTPUT_EXPORT OutputPaneSettings &outputPaneSettings();

} // namespace MultiFilterOutput

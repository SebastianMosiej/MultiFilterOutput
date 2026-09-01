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

    void load();

signals:
    void placementChanged(OutputPanePlacement placement);

private:
    OutputPanePlacement m_placement = OutputPanePlacement::NextToApplicationOutput;
    bool m_jumpToAllOnClick = true;
};

MULTIFILTEROUTPUT_EXPORT OutputPaneSettings &outputPaneSettings();

} // namespace MultiFilterOutput

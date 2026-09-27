#include "outputpanesettings.h"

#include "multifilteroutputconstants.h"

#include <coreplugin/icore.h>

#include <utils/qtcsettings.h>

namespace MultiFilterOutput {

OutputPanePlacement OutputPaneSettings::placement() const
{
    return m_placement;
}

void OutputPaneSettings::setPlacement(OutputPanePlacement placement)
{
    if (m_placement == placement)
        return;
    m_placement = placement;

    Utils::QtcSettings *settings = Core::ICore::settings();
    settings->beginGroup(Constants::SETTINGS_GROUP);
    settings->setValue(
        Constants::OUTPUT_PANE_PLACEMENT_KEY, placement == OutputPanePlacement::AtEnd);
    settings->endGroup();
    settings->sync();

    emit placementChanged(m_placement);
}

bool OutputPaneSettings::jumpToAllOnClick() const
{
    return m_jumpToAllOnClick;
}

void OutputPaneSettings::setJumpToAllOnClick(bool enabled)
{
    if (m_jumpToAllOnClick == enabled)
        return;
    m_jumpToAllOnClick = enabled;

    Utils::QtcSettings *settings = Core::ICore::settings();
    settings->beginGroup(Constants::SETTINGS_GROUP);
    settings->setValue(Constants::JUMP_TO_ALL_ON_CLICK_KEY, enabled);
    settings->endGroup();
    settings->sync();
}

bool OutputPaneSettings::focusPaneOnRunStart() const
{
    return m_focusPaneOnRunStart;
}

void OutputPaneSettings::setFocusPaneOnRunStart(bool enabled)
{
    if (m_focusPaneOnRunStart == enabled)
        return;
    m_focusPaneOnRunStart = enabled;

    Utils::QtcSettings *settings = Core::ICore::settings();
    settings->beginGroup(Constants::SETTINGS_GROUP);
    settings->setValue(Constants::FOCUS_PANE_ON_RUN_START_KEY, enabled);
    settings->endGroup();
    settings->sync();
}

int OutputPaneSettings::maxDisplayedLineLength() const
{
    return m_maxDisplayedLineLength;
}

void OutputPaneSettings::setMaxDisplayedLineLength(int length)
{
    length = qMax(0, length);
    if (m_maxDisplayedLineLength == length)
        return;
    m_maxDisplayedLineLength = length;

    Utils::QtcSettings *settings = Core::ICore::settings();
    settings->beginGroup(Constants::SETTINGS_GROUP);
    settings->setValue(Constants::MAX_DISPLAYED_LINE_LENGTH_KEY, length);
    settings->endGroup();
    settings->sync();

    emit maxDisplayedLineLengthChanged(m_maxDisplayedLineLength);
}

void OutputPaneSettings::load()
{
    Utils::QtcSettings *settings = Core::ICore::settings();
    settings->beginGroup(Constants::SETTINGS_GROUP);
    const bool atEnd = settings->value(Constants::OUTPUT_PANE_PLACEMENT_KEY, false).toBool();
    m_jumpToAllOnClick = settings->value(Constants::JUMP_TO_ALL_ON_CLICK_KEY, true).toBool();
    m_focusPaneOnRunStart =
        settings->value(Constants::FOCUS_PANE_ON_RUN_START_KEY, false).toBool();
    // Clamped like setMaxDisplayedLineLength(): a hand-edited negative value means no limit.
    m_maxDisplayedLineLength = qMax(
        0,
        settings->value(Constants::MAX_DISPLAYED_LINE_LENGTH_KEY, DefaultMaxDisplayedLineLength)
            .toInt());
    settings->endGroup();
    m_placement = atEnd ? OutputPanePlacement::AtEnd : OutputPanePlacement::NextToApplicationOutput;
}

OutputPaneSettings &outputPaneSettings()
{
    static OutputPaneSettings instance;
    return instance;
}

} // namespace MultiFilterOutput

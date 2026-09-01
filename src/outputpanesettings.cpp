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

void OutputPaneSettings::load()
{
    Utils::QtcSettings *settings = Core::ICore::settings();
    settings->beginGroup(Constants::SETTINGS_GROUP);
    const bool atEnd = settings->value(Constants::OUTPUT_PANE_PLACEMENT_KEY, false).toBool();
    m_jumpToAllOnClick = settings->value(Constants::JUMP_TO_ALL_ON_CLICK_KEY, true).toBool();
    settings->endGroup();
    m_placement = atEnd ? OutputPanePlacement::AtEnd : OutputPanePlacement::NextToApplicationOutput;
}

OutputPaneSettings &outputPaneSettings()
{
    static OutputPaneSettings instance;
    return instance;
}

} // namespace MultiFilterOutput

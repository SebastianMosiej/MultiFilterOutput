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

bool OutputPaneSettings::wordWrap() const
{
    return m_wordWrap;
}

void OutputPaneSettings::setWordWrap(bool enabled)
{
    if (m_wordWrap == enabled)
        return;
    m_wordWrap = enabled;

    Utils::QtcSettings *settings = Core::ICore::settings();
    settings->beginGroup(Constants::SETTINGS_GROUP);
    settings->setValue(Constants::WORD_WRAP_KEY, enabled);
    settings->endGroup();
    settings->sync();

    emit wordWrapChanged(m_wordWrap);
}

bool OutputPaneSettings::showUnmatchedTab() const
{
    return m_showUnmatchedTab;
}

void OutputPaneSettings::setShowUnmatchedTab(bool enabled)
{
    if (m_showUnmatchedTab == enabled)
        return;
    m_showUnmatchedTab = enabled;

    Utils::QtcSettings *settings = Core::ICore::settings();
    settings->beginGroup(Constants::SETTINGS_GROUP);
    settings->setValue(Constants::SHOW_UNMATCHED_TAB_KEY, enabled);
    settings->endGroup();
    settings->sync();

    emit showUnmatchedTabChanged(m_showUnmatchedTab);
}

qsizetype OutputPaneSettings::maxCharactersPerTab() const
{
    return m_maxCharactersPerTab;
}

void OutputPaneSettings::setMaxCharactersPerTab(qsizetype characters)
{
    characters = qMax<qsizetype>(0, characters);
    if (m_maxCharactersPerTab == characters)
        return;
    m_maxCharactersPerTab = characters;

    Utils::QtcSettings *settings = Core::ICore::settings();
    settings->beginGroup(Constants::SETTINGS_GROUP);
    settings->setValue(Constants::MAX_CHARACTERS_PER_TAB_KEY, characters);
    settings->endGroup();
    settings->sync();

    emit maxCharactersPerTabChanged(m_maxCharactersPerTab);
}

bool OutputPaneSettings::highlightMatchesInAll() const
{
    return m_highlightMatchesInAll;
}

void OutputPaneSettings::setHighlightMatchesInAll(bool enabled)
{
    if (m_highlightMatchesInAll == enabled)
        return;
    m_highlightMatchesInAll = enabled;

    Utils::QtcSettings *settings = Core::ICore::settings();
    settings->beginGroup(Constants::SETTINGS_GROUP);
    settings->setValue(Constants::HIGHLIGHT_IN_ALL_KEY, enabled);
    settings->endGroup();
    settings->sync();
}

bool OutputPaneSettings::showTimestamps() const
{
    return m_showTimestamps;
}

void OutputPaneSettings::setShowTimestamps(bool enabled)
{
    if (m_showTimestamps == enabled)
        return;
    m_showTimestamps = enabled;

    Utils::QtcSettings *settings = Core::ICore::settings();
    settings->beginGroup(Constants::SETTINGS_GROUP);
    settings->setValue(Constants::SHOW_TIMESTAMPS_KEY, enabled);
    settings->endGroup();
    settings->sync();

    emit showTimestampsChanged(m_showTimestamps);
}

int OutputPaneSettings::finishedRunsKept() const
{
    return m_finishedRunsKept;
}

void OutputPaneSettings::setFinishedRunsKept(int count)
{
    count = qMax(0, count);
    if (m_finishedRunsKept == count)
        return;
    m_finishedRunsKept = count;

    Utils::QtcSettings *settings = Core::ICore::settings();
    settings->beginGroup(Constants::SETTINGS_GROUP);
    settings->setValue(Constants::FINISHED_RUNS_KEPT_KEY, count);
    settings->endGroup();
    settings->sync();

    emit finishedRunsKeptChanged(m_finishedRunsKept);
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
    m_wordWrap = settings->value(Constants::WORD_WRAP_KEY, true).toBool();
    m_showUnmatchedTab = settings->value(Constants::SHOW_UNMATCHED_TAB_KEY, true).toBool();
    m_highlightMatchesInAll = settings->value(Constants::HIGHLIGHT_IN_ALL_KEY, false).toBool();
    m_showTimestamps = settings->value(Constants::SHOW_TIMESTAMPS_KEY, false).toBool();
    m_finishedRunsKept = qMax(
        0, settings->value(Constants::FINISHED_RUNS_KEPT_KEY, DefaultFinishedRunsKept).toInt());
    m_maxCharactersPerTab = qMax<qsizetype>(
        0, settings->value(Constants::MAX_CHARACTERS_PER_TAB_KEY, 0).toLongLong());
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

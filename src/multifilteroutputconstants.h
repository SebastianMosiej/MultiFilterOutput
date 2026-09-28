#pragma once

namespace MultiFilterOutput::Constants {

const char SETTINGS_GROUP[] = "MultiFilterOutput";
const char OUTPUT_PANE_ID[] = "MultiFilterOutput.FilteredOutputPane";
const char OPTIONS_CATEGORY_ID[] = "MultiFilterOutput.Category";
const char OPTIONS_PAGE_ID[] = "MultiFilterOutput.PatternLibraryPage";
const char FILTER_SET_ASPECT_ID[] = "MultiFilterOutput.FilterSetAspect";
const char OUTPUT_PANE_PLACEMENT_KEY[] = "OutputPanePlacedAtEnd";
const char OUTPUT_FONT_ZOOM_KEY[] = "OutputFontZoom";
const char JUMP_TO_ALL_ON_CLICK_KEY[] = "JumpToAllOnClick";
const char FOCUS_PANE_ON_RUN_START_KEY[] = "FocusPaneOnRunStart";
const char MAX_DISPLAYED_LINE_LENGTH_KEY[] = "MaxDisplayedLineLength";
const char WORD_WRAP_KEY[] = "WordWrap";
const char SHOW_UNMATCHED_TAB_KEY[] = "ShowUnmatchedTab";
const char MAX_CHARACTERS_PER_TAB_KEY[] = "MaxCharactersPerTab";
const char HIGHLIGHT_IN_ALL_KEY[] = "HighlightMatchesInAll";
const char SHOW_TIMESTAMPS_KEY[] = "ShowTimestamps";
const char FINISHED_RUNS_KEPT_KEY[] = "FinishedRunsKept";
// The pane's action context, so the zoom shortcuts (Ctrl++ / Ctrl+- / Ctrl+0) reach it.
const char PANE_CONTEXT[] = "MultiFilterOutput.PaneContext";

// The format of everything under SETTINGS_GROUP, so that migrating old data is a decision
// made by version rather than a guess from the values themselves. Written by
// PatternLibrary::save(); absent (read as 0) in settings from before it existed.
//   0: legacy -- a never-colored pattern was stored as QColor(Qt::gray) instead of no color.
//   1: gray is a real color; no color is stored as an invalid QColor.
// Bump it, and add the migration to PatternLibrary::load(), whenever the stored format changes.
const char SETTINGS_SCHEMA_VERSION_KEY[] = "SchemaVersion";
const int SETTINGS_SCHEMA_VERSION = 1;

} // namespace MultiFilterOutput::Constants

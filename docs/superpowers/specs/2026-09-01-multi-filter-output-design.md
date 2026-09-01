# MultiFilterOutput — Design Spec

Date: 2026-09-01
Status: approved (pending spec review)

## Summary

`MultiFilterOutput` is a standalone Qt Creator plugin that lets a user define
named text/regex patterns, choose which of those patterns apply to a given
run configuration, and — while that run configuration's app is executing —
see the matching output lines split into separate tabs in a new bottom-panel
pane, next to the built-in "Application Output" pane.

The plugin makes **zero changes to Qt Creator's own source**. Everything it
needs — `RunControl`'s output signal, run start/stop notifications, "which
run's tab is currently visible," run-configuration aspect registration,
`EditorManager::openEditorAt`, `FileInProjectFinder` — is reachable through
Qt Creator's existing exported (`*_EXPORT`) public API. This was verified
against the `c:\dev\qt-creator` source tree (see "Feasibility notes" below).

## Goals

- Define a global, reusable library of named filter patterns (regex or plain
  text), each with case-sensitivity and an invert ("exclude these lines")
  option.
- Per run configuration, choose which subset of the pattern library applies.
- While a run is active, show one tab per enabled pattern with only the
  matching (or, if inverted, non-matching) lines, plus an "All" tab with the
  unfiltered mirror — next to the built-in Application Output pane, not
  inside it.
- Each pattern tab's label shows a running match count.
- Clicking a filtered line jumps to the same line in "All" for context.
- Clicking a recognized file:line[:col] reference within a line (including
  `qrc:` QML paths) opens that location in the code editor.
- Persist everything (pattern library, per-run-config selection) across Qt
  Creator restarts.

## Non-goals

- No changes to `AppOutputPane`, `Core::OutputWindow`, or any other built-in
  Qt Creator plugin/class.
- No cross-run history: switching which run is "active" resets the pane's
  tabs. (Full history remains available in the built-in Application Output
  pane, which the plugin does not touch or replace.)
- No support for showing multiple runs' filtered output simultaneously; the
  pane tracks only the currently-visible run, mirroring how
  `AppOutputPane` itself focuses one run's tab at a time.

## Architecture

### Components

1. **Pattern Library** — global list of named filter definitions:
   `{ id (uuid), name, patternText, isRegex, caseSensitive, invert, color }`.
   `color` tints that pattern's tab label/icon in `FilteredOutputPane` for
   quick visual identification when several tabs are open; purely cosmetic,
   no effect on matching. Persisted via `Utils::Store`/`QtcSettings`,
   following the pattern used by the Todo plugin's `Settings` class
   (`src/plugins/todo/settings.h/.cpp` in the reference tree). Exposed to
   the rest of the plugin via a global accessor, e.g.
   `PatternLibrary &patternLibrary()`.

2. **Pattern Library options page** — `Core::IOptionsPage` with a list-view
   editor (Add/Edit/Remove), mirroring Todo's `OptionsDialog` +
   `KeywordDialog` pattern. Regex entries are validated with
   `QRegularExpression::isValid()` at save time; invalid patterns are
   rejected inline rather than stored.

3. **`FilterSetAspect`** — a `Utils::BaseAspect` subclass registered once via
   `RunConfiguration::registerAspect<FilterSetAspect>()` in the plugin's
   `IPlugin::initialize()` (same mechanism `DebuggerRunConfigurationAspect`
   uses — see `src/plugins/debugger/debuggerplugin.cpp` in the reference
   tree). Stores the set of Pattern Library ids enabled for that run
   configuration; rendered as a checkable list in the run configuration's
   details panel via `addToLayoutImpl()`. Persisted through the aspect's own
   `fromMap`/`toMap`, so it lives with the rest of the run configuration's
   settings.

4. **Run Tracker** — plugin-owned singleton. At startup, seeds itself from
   `ProjectExplorerPlugin::allRunControls()` (covers runs already active
   before the plugin loads); afterward, follows
   `ProjectExplorerPlugin::runControlStarted`/`runControlStoped`. For every
   `RunControl` it knows about, it connects:
   - `appendMessage(QString msg, Utils::OutputFormat format)` — output text
   - `outputVisibilityChanged(bool)` — reflects `RunControl::isOutputVisible()`,
     which `AppOutputPane` itself sets true only when that run's tab is both
     current AND Application Output itself is the currently-visible bottom
     pane. **Correction (found in final review, 2026-09-01):** this is NOT a
     general "this is the active run" signal — it goes false for every run
     the instant Application Output stops being the visible pane, including
     when the user switches to *this plugin's own* pane. `RunTracker` must
     latch the previously-active run when no tracked run currently reports
     `isOutputVisible()`, rather than following the signal down to null —
     see `RunTracker::updateActiveRunControl()`.

   `appendMessage` delivers arbitrary chunks, not necessarily line-aligned.
   The tracker buffers each `RunControl`'s trailing partial line and only
   emits complete lines downstream (the same problem `Core::OutputWindow`'s
   own chunk handling solves, solved independently here since we don't reuse
   that class).

5. **Line Matcher** — pure function `bool matches(const Pattern&, const
   QString& line)`, built on `QRegularExpression`/`QString::contains`.
   Inclusion for a pattern is `matches(pattern, line) XOR pattern.invert`.
   Unit-testable with no Qt Creator runtime.

6. **`FilteredOutputPane`** — `Core::IOutputPane` subclass, registered with a
   priority placing it next to Application Output in the bottom-panel
   switcher. Owns a `QTabWidget`:
   - **"All" tab** — unfiltered mirror of the active run's output. Own
     `Utils::OutputFormatter` bound to this tab's `QPlainTextEdit` via
     `setPlainTextEdit()`.
   - **One tab per enabled pattern** (resolved from the active run's
     `FilterSetAspect` against the current Pattern Library; ids that no
     longer resolve — e.g. a pattern deleted after being selected — are
     silently skipped, no crash). Each also has its own `OutputFormatter`,
     fed only lines that passed that pattern's `matches(...) XOR invert`
     check.
   - Tab label shows a running match count, e.g. `Errors (12)`; an inverted
     pattern's tab is prefixed, e.g. `Not: Debug (85)`.
   - Toolbar (`toolBarWidgets()`) shows which run is being mirrored, e.g.
     `Filtering: MyApp (pid 1234)`, since multiple run configurations can
     exist.
   - No active run (nothing ever tracked, or the last tracked run's process
     has fully exited and a genuinely different run has since become active):
     placeholder empty state, no stale content shown.
   - **Updated (user-directed, 2026-09-01, final review):** tabs are torn
     down and rebuilt only when a genuinely *different* run becomes active —
     not the instant the current run's process stops. This matches
     Application Output's own behavior of keeping a finished run's content
     visible after exit, so the user can still review filtered output once
     the app has closed. No cross-run history is retained once a rebuild
     does happen (switching to a different run still clears the previous
     run's tabs).

7. **File-link detection** — each tab's `OutputFormatter` is configured with
   a small custom `Utils::OutputLineParser` subclass. It reuses the same
   regex family `QtSupport::Internal::QtOutputLineParser` already applies to
   ordinary app output (`src/plugins/qtsupport/qtoutputformatter.cpp` in the
   reference tree): QML/`file:`/`qrc:` URLs with `:line[:col]`, `Q_ASSERT`/
   `Q_ASSERT_X` failures, `Object::... in file:line` warnings, and QTest
   failure locations (Unix and Windows forms). A shared
   `Utils::FileInProjectFinder`, scoped to the active project's source
   files, resolves `qrc:` URLs back to real files on disk (parses the
   project's `.qrc` files the same way `QtOutputLineParser` does).
   `OutputFormatter::appendMessage()` auto-linkifies matches; clicking one
   calls `Core::EditorManager::openEditorAt({filePath, line, column})`.

   Both `Utils::OutputFormatter` and `Utils::OutputLineParser` are
   `QTCREATOR_UTILS_EXPORT` — public API of the `Utils` library, usable from
   an external plugin without depending on `Core::OutputWindow` or
   `qtsupport` internals. `QtOutputLineParser` itself is not exported, so its
   *regex patterns* are reused (small constants, cheap to duplicate) rather
   than its class.

### Click handling (per tab)

Each tab's `QPlainTextEdit`-derived widget checks `anchorAt(pos)` in its
`mouseReleaseEvent` first:
- **Hit** (click landed on a linkified file:line:col reference): forward to
  the tab's `OutputFormatter::handleLink()`, which opens the editor location.
- **Miss**: fall back to "jump to this same line in the All tab" — the
  filtered tab records, per appended line, the corresponding block/line
  number in "All" at append time, used as the jump target.

Note: because `AppOutputPane` (and its `RunControlTab`/output widget) live
in `ProjectExplorer::Internal` with no exported public surface, there is no
way for an external plugin to scroll or highlight a line inside the
*built-in* Application Output tab. Click-to-jump therefore targets the
plugin's own "All" tab (a full, unfiltered mirror of the same content) —
functionally equivalent, but a distinct widget instance from the one
`AppOutputPane` owns.

## Data flow summary

```
RunControl::appendMessage(chunk, format)
        │
        ▼
RunTracker: buffer partial line, emit complete lines
        │  (only for the currently-tracked active RunControl — latched,
        │   not strictly "isOutputVisible() is true right now"; see the
        │   correction above)
        ▼
FilteredOutputPane: for each complete line
        │
        ├─► append to "All" tab's OutputFormatter (always)
        │
        └─► for each enabled pattern P (from active run's FilterSetAspect):
                if matches(P, line) XOR P.invert:
                    append to P's tab OutputFormatter
                    increment P's tab match count
                    record source line-in-"All" anchor for click-to-jump
```

`OutputFormatter::appendMessage()` on each tab additionally performs
file-link detection/linkification as text is appended (see component 7).

## Settings & run-configuration UI

- **Pattern Library page**: list view (Name / Pattern / Regex or Plain /
  Case-sensitive / Invert / Color) with Add/Edit/Remove, under its own
  options category.
- **Run configuration panel**: `FilterSetAspect` renders a checkable list of
  all Pattern Library entries directly in the run configuration's details,
  next to existing aspects like "Run in terminal." Nothing checked → the
  pane shows only the "All" tab.

## Visual layout

Bottom panel switcher:
```
[Issues] [Search Results] [Application Output] [MultiFilterOutput] [Compile Output] ...
```

Inside `MultiFilterOutput`, with a run active:
```
[All] [Errors (12)] [Not: Debug (85)] [Network (3)]
┌───────────────────────────────────────────────────┐
│ 12:03:41 ERROR  connection refused                  │
│ 12:03:44 ERROR  qrc:/qt/qml/QuickView.qml:67:13:     │
│          Unable to assign [undefined] to bool        │  <- file ref is a clickable link
└───────────────────────────────────────────────────┘
Toolbar: "Filtering: MyApp (pid 1234)"
```

## Error handling

- Invalid regex is rejected at save time in the Pattern Library editor
  (`QRegularExpression::isValid()`), so a broken pattern can never be stored.
- `FilterSetAspect` stores pattern ids, not copies; ids that no longer
  resolve in the library (deleted after being selected) are silently skipped
  when rebuilding tabs.
- No active run: placeholder state, not stale content.

## Testing

- Unit tests: Line Matcher (regex / plain text / case-sensitivity /
  invert), `FilterSetAspect::fromMap`/`toMap` round-trip, partial-line
  buffering in Run Tracker.
- Manual/integration pass: run a sample app with known output; verify each
  tab's content and match counts; verify click-to-jump anchors into "All";
  verify a `qrc:` QML warning link opens the correct file/line/column;
  verify tab rebuild on run switch; verify settings and run-config
  selections persist across a Qt Creator restart.

## Feasibility notes (verified against c:\dev\qt-creator)

- `RunControl` is `PROJECTEXPLORER_EXPORT`; `appendMessage(QString,
  Utils::OutputFormat)`, `isOutputVisible()`/`outputVisibilityChanged(bool)`
  are public (`src/plugins/projectexplorer/runcontrol.h`).
- `ProjectExplorerPlugin::allRunControls()` (static) and
  `runControlStarted`/`runControlStoped` signals
  (`src/plugins/projectexplorer/projectexplorer.h`) give full run
  enumeration/notification without touching `AppOutputPane`.
- `AppOutputPane` and its tab internals live in `ProjectExplorer::Internal`,
  not exported — confirms sibling-pane design is the only externally-clean
  integration; no sub-tab injection into the built-in pane is possible
  without patching core.
- `RunConfiguration::registerAspect<T>()` is a public static template,
  already used cross-plugin by Debugger/Profiler/Valgrind — direct precedent
  for `FilterSetAspect`.
- `Utils::OutputFormatter`/`Utils::OutputLineParser` are
  `QTCREATOR_UTILS_EXPORT`; `Utils::FileInProjectFinder` (used for `qrc:`
  resolution) is likewise part of the public `Utils` library.
- `Core::EditorManager::openEditorAt(const Utils::Link&, ...)` is the public
  entry point used to jump to a file/line/column.

## Plugin skeleton (build wiring)

Following the reference tree's `src/plugins/todo` pattern:
- `MultiFilterOutput.json.in` — plugin metadata, dependencies filled from
  CMake's `PLUGIN_DEPENDS`.
- `multifilteroutputplugin.cpp` — `IPlugin` subclass; `initialize()` wires up
  the Pattern Library options page, registers `FilterSetAspect`, constructs
  the Run Tracker, and registers `FilteredOutputPane`.
- `CMakeLists.txt`:
  ```cmake
  add_qtc_plugin(MultiFilterOutput
    PLUGIN_DEPENDS Core ProjectExplorer
    SOURCES ...
  )
  ```

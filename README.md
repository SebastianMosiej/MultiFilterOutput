# MultiFilterOutput

A Qt Creator plugin that splits a running application's console output into
separate tabs by user-defined text/regex patterns — so you can watch just
the `ERROR` lines, just one subsystem's logging, or everything except the
noisy `DEBUG` chatter, without losing the full unfiltered log.

## Features

- **Pattern Library** — define named filters (plain text or regex), each
  with case sensitivity, an invert option ("show everything *except*
  matches"), a tab color, and an order (drag-and-drop or **Move Up**/**Move
  Down**) that also determines each pattern's left-to-right tab position.
  Managed under **Tools > Options > MultiFilterOutput > Pattern Library**.
- **Continuation-line grouping** — for patterns whose real-world matches
  span multiple lines (an error followed by an indented stack trace or file
  excerpt), turn on **Group continuation lines** and lines that match a
  second "continuation" pattern (default: starts with 1–3 spaces) get swept
  into the same tab right after a match, until a line that doesn't. Swept-in
  lines display but don't count toward the tab's match number, so ordinary
  single-line matches are entirely unaffected.
- **Per-run-configuration filter selection** — choose which patterns apply
  to a given run configuration from an **Output Filters** section in its
  Run Settings. Patterns are defined once, globally, and can be reused
  across as many run configurations as you like.
- **A dedicated output pane** — sits next to the built-in *Application
  Output* pane in the bottom-panel switcher (configurable — see below).
  Contains an **All** tab (an unfiltered mirror of the active run's output)
  plus one tab per pattern enabled for that run.
- **Configurable pane position** — choose, from the Pattern Library
  settings page, whether the pane's status-bar button sits next to
  *Application Output* or at the far right end of the bottom-panel
  switcher. Takes effect after a restart (Qt Creator only re-sorts those
  buttons once, at startup).
- **Matches Application Output's font** — every tab uses the same font as
  *Application Output* (Preferences > Text Editor > Font & Colors), live:
  changing it there updates already-open tabs immediately.
- **Synced, persisted zoom** — Ctrl+wheel zooms a tab; every other tab in
  the same pane (the **All** tab and every pattern tab) follows to the same
  size, across new runs and across Qt Creator restarts.
- **Follows the tail** — every tab stays pinned to the newest line as
  output streams in, the way *Application Output* does. Scroll up and it
  stops following, so a log that is still growing doesn't yank the view out
  from under you; scroll back to the bottom and it resumes.
- **Live match counts** — each tab's label updates as output streams in,
  e.g. `Errors (12)`; an inverted pattern's tab is prefixed, e.g.
  `Not: Debug (85)`.
- **Focus on run start** — optionally, have MultiFilterOutput pop itself to the
  front (instead of *Application Output*) the first time a newly-started run
  produces output. Off by default; enable it from the Pattern Library
  settings page. (If *Application Output*'s own "Open Application Output when
  running" preference is set to **Always** rather than the default **On First
  Output Only**, it can still reclaim focus on later output lines.)
- **Click-to-jump** — click any line in a filtered tab to jump to and
  highlight that same line in **All**, recovering the surrounding context a
  filtered view strips away. Can be turned off from the Pattern Library
  settings page for anyone who'd rather a click just select text normally.
- **Click-to-open** — click a recognized `file:line[:col]` reference inside
  a line (QML warnings, `Q_ASSERT` failures, object-warning messages,
  including `qrc:` paths resolved back to your actual source files) to open
  it directly in the code editor.
- **Long-line truncation** — lines longer than a configurable limit
  (default 10 000 characters; set it to **No limit** to turn this off) are
  shown cut short in every tab, followed by how many characters were left
  out, so one enormous line can't stall the view. Patterns still match
  against the whole line.
- **Persists across restarts** — the Pattern Library, each run
  configuration's filter selection, the pane position, the zoom level, the
  click-to-jump setting, and the maximum displayed line length are all saved
  to Qt Creator's settings.
- **Output survives the run** — a run's filtered tabs stay up after the
  process exits (matching Application Output's own behavior), so you can
  still review what happened after the app has closed. They only reset
  when a different run becomes the active one.
- **Opt-in** — disabled by default on a fresh install; you choose to turn
  it on.

## Requirements

- **Qt Creator 21** (`21.0.0-beta1`, plugin API version `20.0.82`). The
  plugin is built against the `21.0` branch and will not load into Qt
  Creator 20 or earlier: out-of-tree plugins are tied to the exact
  `Core`/`ProjectExplorer`/`TextEditor` API version they compile against,
  and a mismatch is reported as an unresolved dependency on `core`.
- A Qt Creator source/build tree of that version — an end-user Qt Creator
  *installation* is not enough, since it ships no headers and no exported
  `QtCreator` CMake package.
- Qt 6 (Widgets module), matching the Qt version that Qt Creator tree was
  itself built with.
- CMake 3.16+, or 3.22+ to build the unit tests (they rely on the
  `ENVIRONMENT_MODIFICATION` test property).
- A C++20 compiler (tested with MSVC 2022 on Windows).

## Building

Out-of-tree, via Qt Creator's exported CMake package — no changes to Qt
Creator's own source are needed or made.

```powershell
cmake -S <this-repo> -B <this-repo>\build -G "Visual Studio 17 2022" -DCMAKE_PREFIX_PATH="<path-to-built-qt-creator>;<path-to-qt6-install>"
cmake --build <this-repo>\build --config RelWithDebInfo
```

The plugin DLL is produced under `build\lib\qtcreator\plugins\<config>\`.
To build the unit tests as well, add `-DWITH_TESTS=ON` to the configure
step; run them with `ctest` from the build directory.

To launch Qt Creator with the freshly-built plugin loaded, build the
generated `RunQtCreator` target (available whenever CMake can locate a
`qtcreator` executable next to the `QtCreator::Core` package it configured
against).

## Installing and enabling the plugin

MultiFilterOutput ships **disabled by default**.

First put `MultiFilterOutput.dll` somewhere Qt Creator looks. The user
plugin directory needs no write access to the Qt Creator installation and
survives reinstalling it — on Windows:

```
%LOCALAPPDATA%\QtProject\qtcreator\plugins\20.0.82\
```

That last directory is named after the **plugin API version** (`20.0.82`),
not the `21.0.0-beta1` display version — Qt Creator scans
`<major>.<minor>.<patch>` directories down to its own compatibility
version. Alternatively, launch Qt Creator with `-pluginpath` pointed at the
build output, which is what the `RunQtCreator` target does.

Then:

1. Open **Help > About Plugins…**
2. Find **MultiFilterOutput** in the list and check it.
3. Restart Qt Creator.

If the plugin is listed but cannot be enabled, and the error mentions an
unresolved dependency on `core`, it was built against a different Qt
Creator version — see **Requirements**.

## Usage

1. **Define your patterns.** Open **Tools > Options > MultiFilterOutput >
   Pattern Library** and click **Add…**. For each pattern, set:
   - **Name** — shown as the tab label.
   - **Pattern** — the text or regular expression to match against each
     output line.
   - **Regular expression** — toggle plain-text vs. regex matching. An
     invalid regex blocks saving, with an inline error, until fixed.
   - **Case sensitive** — off by default.
   - **Exclude matching lines (invert)** — show every line *except* the
     ones that match, instead of only the matches.
   - **Group continuation lines** — turn on if this pattern's real matches
     can span multiple lines (e.g. an error followed by an indented detail
     block); set **Continuation line starts with** to a regex describing
     those follow-on lines (hover the ⓘ icon for the full explanation).
   - **Tab color** — tints that pattern's tab text for quick visual
     identification.
   - Drag a row (or use **Move Up**/**Move Down**) to reorder patterns —
     order determines each pattern's tab position.
2. **Choose which patterns apply to a run.** Open your project's run
   configuration and find the **Output Filters** section — check the
   patterns you want active for that specific run configuration. Different
   run configurations can enable different subsets of the same library.
3. **(Optional) Configure the pane.** Still on the Pattern Library settings
   page: pick where the pane's status-bar button sits (**Output Pane
   Position**), whether clicking a line in a pattern tab should jump to
   **All** or just behave like a normal text click, whether the pane
   should take focus away from **Application Output** when a run starts, and
   the **Maximum displayed line length** for all tabs.
4. **Run your app.** A **MultiFilterOutput** button appears in the
   bottom-panel switcher next to **Application Output** (or at the end,
   per your position setting).
5. **Read the filtered output.** Open the pane: an **All** tab mirrors the
   full unfiltered log, and one tab per pattern you enabled for this run
   shows only its matching (or, if inverted, non-matching) lines, with a
   live count in the tab label. Ctrl+scroll to zoom — every tab in the pane
   follows.
6. **Recover context.** Click a line in a filtered tab to jump to and
   highlight the same line in **All** (if the jump-to-click setting is on).
7. **Jump to source.** Click a recognized file/line reference within a
   line — e.g. a QML runtime warning like
   `qrc:/qt/qml/MyView.qml:67:13: Unable to assign [undefined] to bool` —
   to open that file at that exact line and column in the editor.

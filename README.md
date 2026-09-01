# MultiFilterOutput

A Qt Creator plugin that splits a running application's console output into
separate tabs by user-defined text/regex patterns — so you can watch just
the `ERROR` lines, just one subsystem's logging, or everything except the
noisy `DEBUG` chatter, without losing the full unfiltered log.

## Features

- **Pattern Library** — define named filters (plain text or regex), each
  with case sensitivity, an invert option ("show everything *except*
  matches"), and a tab color. Managed under
  **Tools > Options > MultiFilterOutput > Pattern Library**.
- **Per-run-configuration filter selection** — choose which patterns apply
  to a given run configuration from an **Output Filters** section in its
  Run Settings. Patterns are defined once, globally, and can be reused
  across as many run configurations as you like.
- **A dedicated output pane** — sits next to the built-in *Application
  Output* pane in the bottom-panel switcher. Contains an **All** tab (an
  unfiltered mirror of the active run's output) plus one tab per pattern
  enabled for that run.
- **Live match counts** — each tab's label updates as output streams in,
  e.g. `Errors (12)`; an inverted pattern's tab is prefixed, e.g.
  `Not: Debug (85)`.
- **Click-to-jump** — click any line in a filtered tab to jump to and
  highlight that same line in **All**, recovering the surrounding context a
  filtered view strips away.
- **Click-to-open** — click a recognized `file:line[:col]` reference inside
  a line (QML warnings, `Q_ASSERT` failures, object-warning messages,
  including `qrc:` paths resolved back to your actual source files) to open
  it directly in the code editor.
- **Persists across restarts** — the Pattern Library and each run
  configuration's filter selection are saved to Qt Creator's settings.
- **Output survives the run** — a run's filtered tabs stay up after the
  process exits (matching Application Output's own behavior), so you can
  still review what happened after the app has closed. They only reset
  when a different run becomes the active one.
- **Opt-in** — disabled by default on a fresh install; you choose to turn
  it on.

## Requirements

- A Qt Creator source/build tree whose `Core`/`ProjectExplorer` API version
  matches what this plugin is built against (out-of-tree plugins are tied
  to the exact Qt Creator version they compile against).
- Qt 6 (Widgets module), matching the Qt version that Qt Creator tree was
  itself built with.
- CMake 3.16+.
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
step and build the individual `tst_*` targets.

To launch Qt Creator with the freshly-built plugin loaded, build the
generated `RunQtCreator` target (available whenever CMake can locate a
`qtcreator` executable next to the `QtCreator::Core` package it configured
against).

## Enabling the plugin

MultiFilterOutput ships **disabled by default**. After installing it into
your Qt Creator plugins directory (or launching with `-pluginpath` pointed
at the build output):

1. Open **Help > About Plugins…**
2. Find **MultiFilterOutput** in the list and check it.
3. Restart Qt Creator.

## Usage

1. **Define your patterns.** Open **Tools > Options > MultiFilterOutput >
   Pattern Library** and click **Add…**. For each pattern, set:
   - **Name** — shown as the tab label.
   - **Pattern** — the text or regular expression to match against each
     output line.
   - **Regular expression** — toggle plain-text vs. regex matching. An
     invalid regex blocks **OK** with an inline error until fixed.
   - **Case sensitive** — off by default.
   - **Exclude matching lines (invert)** — show every line *except* the
     ones that match, instead of only the matches.
   - **Tab color** — tints that pattern's tab text for quick visual
     identification.
2. **Choose which patterns apply to a run.** Open your project's run
   configuration and find the **Output Filters** section — check the
   patterns you want active for that specific run configuration. Different
   run configurations can enable different subsets of the same library.
3. **Run your app.** A **MultiFilterOutput** button appears in the
   bottom-panel switcher next to **Application Output**.
4. **Read the filtered output.** Open the pane: an **All** tab mirrors the
   full unfiltered log, and one tab per pattern you enabled for this run
   shows only its matching (or, if inverted, non-matching) lines, with a
   live count in the tab label.
5. **Recover context.** Click a line in a filtered tab to jump to and
   highlight the same line in **All**.
6. **Jump to source.** Click a recognized file/line reference within a
   line — e.g. a QML runtime warning like
   `qrc:/qt/qml/MyView.qml:67:13: Unable to assign [undefined] to bool` —
   to open that file at that exact line and column in the editor.

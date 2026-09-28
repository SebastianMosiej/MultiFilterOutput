#pragma once

#include "multifilteroutput_global.h"

#include <utils/outputformat.h>

#include <QColor>
#include <QFlags>
#include <QString>
#include <QVariantMap>

namespace MultiFilterOutput {

class MULTIFILTEROUTPUT_EXPORT Pattern
{
public:
    QString id;
    QString name;
    QString patternText;
    // Lines matching this are left out even if they match patternText (e.g. "ERROR" but not
    // "ERROR: known flaky"). Read like patternText -- regex or plain text, same case
    // sensitivity -- and applied after invert. Empty (the default): excludes nothing.
    QString excludeText;
    bool isRegex = true;
    bool caseSensitive = false;
    // Plain text only (a regex says so itself, with \b): the text -- and excludeText -- must
    // not touch a word character on either side, so "err" finds "err:" but not "error" or
    // "stderr". Ignored for regex patterns and for empty text.
    bool wholeWord = false;
    bool invert = false;
    // Invalid (the default) means "no tint" -- the tab keeps the pane's normal text color;
    // see FilteredOutputPane::updateTabLabel().
    QColor color;
    // When set, a line that doesn't itself match patternText is still swept into this
    // pattern's tab -- without counting as a match -- as long as it matches
    // continuationPattern (always a regex), starting right after a line that did match.
    // Sweeping stops at the first line that doesn't match continuationPattern.
    bool groupContinuationLines = false;
    QString continuationPattern = QStringLiteral("^ {1,3}");
    // Like grep -B/-A: this many lines before/after each match are shown in the tab too,
    // without counting as matches. Only lines on the pattern's channels count, and a line is
    // never shown twice (overlapping context just joins up). 0..MaxContextLines.
    static constexpr int MaxContextLines = 100;
    int contextBefore = 0;
    int contextAfter = 0;
    // When set, FilterSetAspect enables this pattern by default for every newly created run
    // configuration (not for ones already saved -- see FilterSetAspect::FilterSetAspect()).
    bool isDefault = false;

    // Which output a pattern looks at. A line from any other channel is invisible to it: not
    // matched, not swept in as a continuation line, and not ending a continuation run.
    enum Channel {
        StdOutChannel = 0x1,
        StdErrChannel = 0x2,
        // Qt Creator's own lines: "Starting ...", "... exited with code 0", debugger and
        // other messages -- every Utils::OutputFormat except the application's stdout/stderr.
        MessagesChannel = 0x4,
        AllChannels = StdOutChannel | StdErrChannel | MessagesChannel,
    };
    Q_DECLARE_FLAGS(Channels, Channel)
    Channels channels = AllChannels;

    static Channel channelOf(Utils::OutputFormat format);
    bool looksAt(Utils::OutputFormat format) const;

    static Pattern createNew();
    QVariantMap toMap() const;
    static Pattern fromMap(const QVariantMap &map);
};

Q_DECLARE_OPERATORS_FOR_FLAGS(Pattern::Channels)

} // namespace MultiFilterOutput

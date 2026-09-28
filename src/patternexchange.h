#pragma once

#include "multifilteroutput_global.h"
#include "pattern.h"

#include <QByteArray>
#include <QList>
#include <QString>
#include <QStringList>

namespace MultiFilterOutput {

// Moving patterns between Pattern Library instances (another machine, a teammate) as a JSON
// file -- the Pattern Library settings page's Export... and Import... buttons. Free of widgets
// so it can be tested on its own.

// The whole list as a JSON document: {"format": "MultiFilterOutput.PatternLibrary",
// "version": 1, "patterns": [...]}, each pattern its Pattern::toMap() fields, with the color as
// "#rrggbb" ("#aarrggbb" if not opaque) or absent for none.
MULTIFILTEROUTPUT_EXPORT QByteArray exportPatterns(const QList<Pattern> &patterns);

struct ParsedPatterns
{
    QList<Pattern> patterns;
    // Why nothing could be read (not JSON, not a pattern library file). Empty on success.
    QString error;
    // Read, but with problems worth telling the user about (see validatePatterns(); a file
    // from a newer version).
    QStringList warnings;
};
// With an `idSeed` (a project's pattern file: see ProjectPatterns), a pattern without an id
// of its own -- or sharing one -- is given one derived from the seed and its place in the
// file, the same every time the file is read, rather than a random one: run configurations
// select patterns by id, and the file is read again whenever anything near it changes.
MULTIFILTEROUTPUT_EXPORT ParsedPatterns parsePatterns(
    const QByteArray &json, const QString &idSeed = {});

// Importing merges by id. A pattern whose id the library doesn't have is added; one it has
// with identical contents is skipped; one it has with different contents is a conflict,
// settled for all of them at once:
enum class ImportConflicts {
    Replace,      // the imported version replaces the library's
    KeepExisting, // the library's version stays; the imported one is dropped
    KeepBoth,     // the imported one is added as a copy, with a new id and "(imported)" name
};

struct ImportPlan
{
    QList<Pattern> added;
    QList<Pattern> conflicting; // the imported versions
    int unchanged = 0;
};
MULTIFILTEROUTPUT_EXPORT ImportPlan planImport(
    const QList<Pattern> &library, const QList<Pattern> &imported);

// The library after importing: replaced patterns stay where they were; the added patterns go
// at the end in file order, followed by any kept-both copies.
MULTIFILTEROUTPUT_EXPORT QList<Pattern> applyImport(
    const QList<Pattern> &library, const ImportPlan &plan, ImportConflicts conflicts);

} // namespace MultiFilterOutput

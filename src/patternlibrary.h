#pragma once

#include "multifilteroutput_global.h"
#include "pattern.h"

#include <QList>
#include <QObject>
#include <QStringList>

#include <optional>

namespace Utils {
class QtcSettings;
}

namespace MultiFilterOutput {

class MULTIFILTEROUTPUT_EXPORT PatternLibrary : public QObject
{
    Q_OBJECT

public:
    QList<Pattern> patterns() const;
    std::optional<Pattern> patternById(const QString &id) const;
    // The patterns whose ids are in `ids`, in the library's own order (the order the user
    // arranges on the settings page) rather than the order of `ids`. Ids with no pattern in
    // the library -- e.g. one deleted since a run configuration selected it -- are skipped.
    QList<Pattern> patternsWithIds(const QStringList &ids) const;

    void addOrUpdate(const Pattern &pattern);
    void remove(const QString &id);
    // Replaces the whole list in one go, emitting patternsChanged() exactly once. Used by
    // the options page to push its working copy across on apply() -- and only on apply(),
    // so that Cancel really cancels.
    void setPatterns(const QList<Pattern> &patterns);

    // Reads the library from `settings` and remembers them as where it lives: from then on
    // every change (addOrUpdate(), remove(), setPatterns()) is written straight back, so no
    // caller can forget to persist one. A library that has never been loaded -- e.g. a
    // local one in a unit test -- keeps its changes in memory only.
    void load(Utils::QtcSettings *settings);
    // Writes the library to the settings it was loaded from; does nothing if it never was.
    void save() const;

signals:
    void patternsChanged();
    // Emitted by remove() and setPatterns() with the ids of patterns that are gone, just
    // before the patternsChanged() for the same change -- so run configurations can forget
    // those ids (see FilterSetAspect). Deliberately not derived from "ids the library doesn't
    // know": that would also wipe every selection if the library ever came up empty.
    void patternsRemoved(const QStringList &ids);

private:
    // Every mutator ends here: persist (see load()), then notify -- patternsRemoved() first,
    // if the change dropped any patterns.
    void commitChange(const QStringList &removedIds = {});

    QList<Pattern> m_patterns;
    Utils::QtcSettings *m_settings = nullptr;
};

MULTIFILTEROUTPUT_EXPORT PatternLibrary &patternLibrary();

// Checks patterns loaded from settings -- which may have been hand-edited or written by an
// older version -- for problems the Pattern Library editor would never let through, and
// returns a human-readable description of each. Only ids are repaired: a missing id gets a
// fresh one, and of several patterns sharing an id the first keeps it (so run configurations
// that selected it still find it) while the others get fresh ones. Everything else -- empty
// pattern text, an invalid regular expression -- is reported but left as is: matching already
// fails safe on it, and the user can fix it in the editor.
MULTIFILTEROUTPUT_EXPORT QStringList validatePatterns(QList<Pattern> &patterns);

} // namespace MultiFilterOutput

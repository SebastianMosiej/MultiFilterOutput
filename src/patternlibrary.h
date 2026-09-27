#pragma once

#include "multifilteroutput_global.h"
#include "pattern.h"

#include <QList>
#include <QObject>
#include <QStringList>

#include <optional>

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

    void load();
    void save() const;

signals:
    void patternsChanged();

private:
    QList<Pattern> m_patterns;
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

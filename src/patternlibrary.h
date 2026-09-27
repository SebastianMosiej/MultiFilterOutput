#pragma once

#include "multifilteroutput_global.h"
#include "pattern.h"

#include <QList>
#include <QObject>

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

} // namespace MultiFilterOutput

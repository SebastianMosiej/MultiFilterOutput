#include "patternlibrary.h"

#include "multifilteroutputconstants.h"
#include "multifilteroutputtr.h"

#include <utils/qtcsettings.h>

#include <QRegularExpression>
#include <QSet>

#include <algorithm>

namespace MultiFilterOutput {

QList<Pattern> PatternLibrary::patterns() const
{
    return m_patterns;
}

std::optional<Pattern> PatternLibrary::patternById(const QString &id) const
{
    for (const Pattern &p : m_patterns) {
        if (p.id == id)
            return p;
    }
    return std::nullopt;
}

QList<Pattern> PatternLibrary::patternsWithIds(const QStringList &ids) const
{
    QList<Pattern> result;
    for (const Pattern &p : m_patterns) {
        if (ids.contains(p.id))
            result.append(p);
    }
    return result;
}

void PatternLibrary::addOrUpdate(const Pattern &pattern)
{
    for (Pattern &existing : m_patterns) {
        if (existing.id == pattern.id) {
            existing = pattern;
            commitChange();
            return;
        }
    }
    m_patterns.append(pattern);
    commitChange();
}

void PatternLibrary::remove(const QString &id)
{
    const qsizetype before = m_patterns.size();
    m_patterns.removeIf([&id](const Pattern &p) { return p.id == id; });
    if (m_patterns.size() != before)
        commitChange({id});
}

void PatternLibrary::setPatterns(const QList<Pattern> &patterns)
{
    QStringList removedIds;
    for (const Pattern &old : std::as_const(m_patterns)) {
        const bool kept = std::any_of(patterns.cbegin(), patterns.cend(), [&old](const Pattern &p) {
            return p.id == old.id;
        });
        if (!kept)
            removedIds.append(old.id);
    }
    m_patterns = patterns;
    commitChange(removedIds);
}

void PatternLibrary::commitChange(const QStringList &removedIds)
{
    save();
    if (!removedIds.isEmpty())
        emit patternsRemoved(removedIds);
    emit patternsChanged();
}

QStringList PatternLibrary::load(Utils::QtcSettings *settings)
{
    m_patterns.clear();
    m_settings = settings;

    settings->beginGroup(Constants::SETTINGS_GROUP);
    const int schemaVersion = settings->value(Constants::SETTINGS_SCHEMA_VERSION_KEY, 0).toInt();

    const int count = settings->beginReadArray("patterns");
    for (int i = 0; i < count; ++i) {
        settings->setArrayIndex(i);
        // Whatever keys the entry has -- Pattern::fromMap() picks the ones it knows. A field
        // saved before it existed is simply absent, so fromMap()'s own default applies.
        QVariantMap map;
        for (const Utils::Key &key : settings->childKeys())
            map.insert(Utils::stringFromKey(key), settings->value(key));
        m_patterns.append(Pattern::fromMap(map));
    }
    settings->endArray();

    settings->endGroup();

    QStringList problems;
    if (schemaVersion > Constants::SETTINGS_SCHEMA_VERSION) {
        problems << Tr::tr("The Pattern Library settings were written by a newer version of "
                           "MultiFilterOutput (settings format %1; this version understands up to "
                           "%2). Anything that version added will be lost the next time the "
                           "Pattern Library is saved.")
                        .arg(schemaVersion)
                        .arg(Constants::SETTINGS_SCHEMA_VERSION);
    }
    // Migrations, oldest first. Each one runs only for data older than the format it fixes;
    // the save() below then stamps the current version, so it never runs twice.
    if (schemaVersion < 1) {
        // Before version 1 a never-colored pattern was stored as QColor(Qt::gray), not as
        // "no color" -- indistinguishable from a deliberate gray, but only possible then.
        for (Pattern &p : m_patterns) {
            if (p.color == QColor(Qt::gray))
                p.color = QColor();
        }
    }
    const auto ids = [this] {
        QStringList result;
        for (const Pattern &p : std::as_const(m_patterns))
            result << p.id;
        return result;
    };
    const QStringList idsAsStored = ids();
    problems += validatePatterns(m_patterns);
    const bool idsRepaired = ids() != idsAsStored;

    // Write migrated or repaired data back straight away rather than at shutdown: a repaired
    // id that only lived in memory would come back different after a crash, orphaning any
    // run configuration that selected the pattern meanwhile. Never over a newer format,
    // though -- loading alone must not downgrade it (see the warning above).
    if (schemaVersion <= Constants::SETTINGS_SCHEMA_VERSION
        && (schemaVersion < Constants::SETTINGS_SCHEMA_VERSION || idsRepaired)) {
        save();
    }

    emit patternsChanged();
    return problems;
}

void PatternLibrary::save() const
{
    Utils::QtcSettings *settings = m_settings;
    if (!settings)
        return;
    settings->beginGroup(Constants::SETTINGS_GROUP);
    settings->setValue(Constants::SETTINGS_SCHEMA_VERSION_KEY, Constants::SETTINGS_SCHEMA_VERSION);

    settings->beginWriteArray("patterns", int(m_patterns.size()));
    for (int i = 0; i < m_patterns.size(); ++i) {
        settings->setArrayIndex(i);
        // Every field Pattern::toMap() has, so a new field is persisted without touching this.
        const QVariantMap map = m_patterns.at(i).toMap();
        for (auto it = map.cbegin(); it != map.cend(); ++it)
            settings->setValue(Utils::keyFromString(it.key()), it.value());
    }
    settings->endArray();

    settings->endGroup();
    settings->sync();
}

QStringList validatePatterns(QList<Pattern> &patterns)
{
    QStringList problems;
    QSet<QString> seenIds;
    for (Pattern &p : patterns) {
        const QString name = p.name.isEmpty() ? Tr::tr("(unnamed)") : p.name;

        if (p.id.isEmpty()) {
            p.id = Pattern::createNew().id;
            problems << Tr::tr("Pattern \"%1\" had no id; it was given a new one.").arg(name);
        } else if (seenIds.contains(p.id)) {
            const QString sharedId = p.id;
            p.id = Pattern::createNew().id;
            problems << Tr::tr("Pattern \"%1\" had the same id (%2) as an earlier pattern; it was "
                               "given a new one, so run configurations that selected that id "
                               "keep using the earlier pattern.")
                            .arg(name, sharedId);
        }
        seenIds.insert(p.id);

        if (p.patternText.isEmpty()) {
            problems << Tr::tr("Pattern \"%1\" has no pattern text, so it matches every line.")
                            .arg(name);
        } else if (p.isRegex && !QRegularExpression(p.patternText).isValid()) {
            problems << Tr::tr("Pattern \"%1\" is not a valid regular expression, so it never "
                               "matches.")
                            .arg(name);
        }

        if (p.isRegex && !p.excludeText.isEmpty()
            && !QRegularExpression(p.excludeText).isValid()) {
            problems << Tr::tr("Pattern \"%1\" has an exclude expression that is not a valid "
                               "regular expression, so it excludes nothing.")
                            .arg(name);
        }

        if (p.groupContinuationLines
            && (p.continuationPattern.isEmpty()
                || !QRegularExpression(p.continuationPattern).isValid())) {
            problems << Tr::tr("Pattern \"%1\" groups continuation lines, but its continuation "
                               "pattern is not a valid regular expression, so no lines are "
                               "grouped.")
                            .arg(name);
        }
    }
    return problems;
}

PatternLibrary &patternLibrary()
{
    static PatternLibrary instance;
    return instance;
}

} // namespace MultiFilterOutput

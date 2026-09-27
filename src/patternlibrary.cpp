#include "patternlibrary.h"

#include "multifilteroutputconstants.h"
#include "multifilteroutputtr.h"

#include <coreplugin/icore.h>
#include <coreplugin/messagemanager.h>

#include <utils/qtcsettings.h>

#include <QRegularExpression>
#include <QSet>

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
            emit patternsChanged();
            return;
        }
    }
    m_patterns.append(pattern);
    emit patternsChanged();
}

void PatternLibrary::remove(const QString &id)
{
    const qsizetype before = m_patterns.size();
    m_patterns.removeIf([&id](const Pattern &p) { return p.id == id; });
    if (m_patterns.size() != before)
        emit patternsChanged();
}

void PatternLibrary::setPatterns(const QList<Pattern> &patterns)
{
    m_patterns = patterns;
    emit patternsChanged();
}

void PatternLibrary::load()
{
    m_patterns.clear();

    Utils::QtcSettings *settings = Core::ICore::settings();
    settings->beginGroup(Constants::SETTINGS_GROUP);

    const int count = settings->beginReadArray("patterns");
    for (int i = 0; i < count; ++i) {
        settings->setArrayIndex(i);
        QVariantMap map;
        map.insert("id", settings->value("id"));
        map.insert("name", settings->value("name"));
        map.insert("patternText", settings->value("patternText"));
        map.insert("isRegex", settings->value("isRegex"));
        map.insert("caseSensitive", settings->value("caseSensitive"));
        map.insert("invert", settings->value("invert"));
        map.insert("color", settings->value("color"));
        // Left unset (rather than inserted with a null QVariant) for patterns saved before
        // this field existed, so fromMap()'s own default applies instead of an empty string.
        if (settings->contains("groupContinuationLines"))
            map.insert("groupContinuationLines", settings->value("groupContinuationLines"));
        if (settings->contains("continuationPattern"))
            map.insert("continuationPattern", settings->value("continuationPattern"));
        if (settings->contains("isDefault"))
            map.insert("isDefault", settings->value("isDefault"));
        m_patterns.append(Pattern::fromMap(map));
    }
    settings->endArray();

    settings->endGroup();

    // General Messages rather than a debug log: these need the user to look at the Pattern
    // Library, and the log is not somewhere they would ever see them.
    for (const QString &problem : validatePatterns(m_patterns))
        Core::MessageManager::writeSilently(Tr::tr("MultiFilterOutput: %1").arg(problem));

    emit patternsChanged();
}

void PatternLibrary::save() const
{
    Utils::QtcSettings *settings = Core::ICore::settings();
    settings->beginGroup(Constants::SETTINGS_GROUP);

    settings->beginWriteArray("patterns", int(m_patterns.size()));
    for (int i = 0; i < m_patterns.size(); ++i) {
        settings->setArrayIndex(i);
        const QVariantMap map = m_patterns.at(i).toMap();
        settings->setValue("id", map.value("id"));
        settings->setValue("name", map.value("name"));
        settings->setValue("patternText", map.value("patternText"));
        settings->setValue("isRegex", map.value("isRegex"));
        settings->setValue("caseSensitive", map.value("caseSensitive"));
        settings->setValue("invert", map.value("invert"));
        settings->setValue("color", map.value("color"));
        settings->setValue("groupContinuationLines", map.value("groupContinuationLines"));
        settings->setValue("continuationPattern", map.value("continuationPattern"));
        settings->setValue("isDefault", map.value("isDefault"));
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

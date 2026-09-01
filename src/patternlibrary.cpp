#include "patternlibrary.h"

#include "multifilteroutputconstants.h"

#include <coreplugin/icore.h>

#include <utils/qtcsettings.h>

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
        m_patterns.append(Pattern::fromMap(map));
    }
    settings->endArray();

    settings->endGroup();

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
    }
    settings->endArray();

    settings->endGroup();
    settings->sync();
}

PatternLibrary &patternLibrary()
{
    static PatternLibrary instance;
    return instance;
}

} // namespace MultiFilterOutput

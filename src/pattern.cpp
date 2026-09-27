#include "pattern.h"

#include <QUuid>

namespace MultiFilterOutput {

Pattern Pattern::createNew()
{
    Pattern p;
    p.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    p.name = QStringLiteral("New Pattern");
    return p;
}

QVariantMap Pattern::toMap() const
{
    return {
        {"id", id},
        {"name", name},
        {"patternText", patternText},
        {"isRegex", isRegex},
        {"caseSensitive", caseSensitive},
        {"invert", invert},
        {"color", color},
        {"groupContinuationLines", groupContinuationLines},
        {"continuationPattern", continuationPattern},
        {"isDefault", isDefault},
    };
}

Pattern Pattern::fromMap(const QVariantMap &map)
{
    Pattern p;
    p.id = map.value("id").toString();
    p.name = map.value("name").toString();
    p.patternText = map.value("patternText").toString();
    p.isRegex = map.value("isRegex", true).toBool();
    p.caseSensitive = map.value("caseSensitive", false).toBool();
    p.invert = map.value("invert", false).toBool();
    // Taken as is -- including gray. Migrating the legacy "gray means no tint" sentinel is
    // PatternLibrary::load()'s job, and only for settings from before the schema version.
    p.color = map.value("color").value<QColor>();
    p.groupContinuationLines = map.value("groupContinuationLines", false).toBool();
    p.continuationPattern = map.value("continuationPattern", QStringLiteral("^ {1,3}")).toString();
    p.isDefault = map.value("isDefault", false).toBool();
    return p;
}

} // namespace MultiFilterOutput

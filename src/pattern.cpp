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
    p.color = map.value("color", QColor(Qt::gray)).value<QColor>();
    p.groupContinuationLines = map.value("groupContinuationLines", false).toBool();
    p.continuationPattern = map.value("continuationPattern", QStringLiteral("^ {1,3}")).toString();
    return p;
}

} // namespace MultiFilterOutput

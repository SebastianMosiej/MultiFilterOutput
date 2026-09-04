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
    p.color = map.value("color").value<QColor>();
    // Older releases defaulted every pattern to an explicit QColor(Qt::gray) instead of "no
    // tint" (see pattern.h), so that sentinel is already persisted for any pattern nobody
    // deliberately colored -- indistinguishable on disk from an intentional choice, but the
    // same value across every never-customized pattern makes it safe to treat as legacy and
    // migrate back to unset.
    if (p.color == QColor(Qt::gray))
        p.color = QColor();
    p.groupContinuationLines = map.value("groupContinuationLines", false).toBool();
    p.continuationPattern = map.value("continuationPattern", QStringLiteral("^ {1,3}")).toString();
    p.isDefault = map.value("isDefault", false).toBool();
    return p;
}

} // namespace MultiFilterOutput

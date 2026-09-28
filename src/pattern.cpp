#include "pattern.h"

#include <QUuid>

#include <algorithm>

namespace MultiFilterOutput {

Pattern::Channel Pattern::channelOf(Utils::OutputFormat format)
{
    switch (format) {
    case Utils::StdOutFormat:
        return StdOutChannel;
    case Utils::StdErrFormat:
        return StdErrChannel;
    default:
        return MessagesChannel;
    }
}

bool Pattern::looksAt(Utils::OutputFormat format) const
{
    return channels.testFlag(channelOf(format));
}

QStringList Pattern::normalizedTags(const QStringList &tags)
{
    // Commas separate tags where they are typed (see tagsFromText()), so one inside a tag
    // (hand-edited settings) splits it.
    QStringList result;
    for (const QString &tag : tags) {
        for (const QString &part : tag.split(u',')) {
            const QString trimmed = part.trimmed();
            if (!trimmed.isEmpty() && !result.contains(trimmed))
                result.append(trimmed);
        }
    }
    return result;
}

QStringList Pattern::tagsFromText(const QString &text)
{
    return normalizedTags({text});
}

QStringList patternTags(const QList<Pattern> &patterns)
{
    QStringList tags;
    for (const Pattern &pattern : patterns) {
        for (const QString &tag : pattern.tags) {
            if (!tags.contains(tag))
                tags.append(tag);
        }
    }
    std::sort(tags.begin(), tags.end(), [](const QString &a, const QString &b) {
        return a.compare(b, Qt::CaseInsensitive) < 0;
    });
    return tags;
}

QStringList patternIdsWithTag(const QList<Pattern> &patterns, const QString &tag)
{
    QStringList ids;
    for (const Pattern &pattern : patterns) {
        if (pattern.tags.contains(tag))
            ids.append(pattern.id);
    }
    return ids;
}

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
        {"excludeText", excludeText},
        {"isRegex", isRegex},
        {"caseSensitive", caseSensitive},
        {"wholeWord", wholeWord},
        {"invert", invert},
        {"color", color},
        {"groupContinuationLines", groupContinuationLines},
        {"continuationPattern", continuationPattern},
        {"contextBefore", contextBefore},
        {"contextAfter", contextAfter},
        {"isDefault", isDefault},
        {"tags", tags},
        {"channels", int(channels.toInt())},
    };
}

Pattern Pattern::fromMap(const QVariantMap &map)
{
    Pattern p;
    p.id = map.value("id").toString();
    p.name = map.value("name").toString();
    p.patternText = map.value("patternText").toString();
    p.excludeText = map.value("excludeText").toString();
    p.isRegex = map.value("isRegex", true).toBool();
    p.caseSensitive = map.value("caseSensitive", false).toBool();
    p.wholeWord = map.value("wholeWord", false).toBool();
    p.invert = map.value("invert", false).toBool();
    // Taken as is -- including gray. Migrating the legacy "gray means no tint" sentinel is
    // PatternLibrary::load()'s job, and only for settings from before the schema version.
    p.color = map.value("color").value<QColor>();
    p.groupContinuationLines = map.value("groupContinuationLines", false).toBool();
    p.continuationPattern = map.value("continuationPattern", QStringLiteral("^ {1,3}")).toString();
    p.isDefault = map.value("isDefault", false).toBool();
    p.tags = normalizedTags(map.value("tags").toStringList());
    p.contextBefore = qBound(0, map.value("contextBefore", 0).toInt(), MaxContextLines);
    p.contextAfter = qBound(0, map.value("contextAfter", 0).toInt(), MaxContextLines);
    // Absent in settings from before channels existed; none at all (or only unknown bits)
    // would make a pattern that can never match. Either way: all channels.
    const Channels channels
        = Channels::fromInt(map.value("channels", int(AllChannels)).toInt()) & AllChannels;
    p.channels = channels ? channels : Channels(AllChannels);
    return p;
}

} // namespace MultiFilterOutput

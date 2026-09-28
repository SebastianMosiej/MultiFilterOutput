#include "patternexchange.h"

#include "multifilteroutputtr.h"
#include "patternlibrary.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QUuid>

#include <algorithm>

namespace MultiFilterOutput {

namespace {

const char FormatTag[] = "MultiFilterOutput.PatternLibrary";
constexpr int FormatVersion = 1;

} // namespace

QByteArray exportPatterns(const QList<Pattern> &patterns)
{
    QJsonArray array;
    for (const Pattern &pattern : patterns) {
        QVariantMap map = pattern.toMap();
        // JSON has no color type: a name, like the ones Qt's own style sheets take.
        map.remove("color");
        if (pattern.color.isValid()) {
            map.insert(
                "color",
                pattern.color.name(
                    pattern.color.alpha() == 255 ? QColor::HexRgb : QColor::HexArgb));
        }
        array.append(QJsonObject::fromVariantMap(map));
    }
    const QJsonObject root{
        {"format", QString::fromLatin1(FormatTag)},
        {"version", FormatVersion},
        {"patterns", array},
    };
    return QJsonDocument(root).toJson(QJsonDocument::Indented);
}

ParsedPatterns parsePatterns(const QByteArray &json, const QString &idSeed)
{
    ParsedPatterns result;
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(json, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        result.error = Tr::tr("The file is not valid JSON: %1 (at character %2).")
                           .arg(parseError.errorString())
                           .arg(parseError.offset);
        return result;
    }
    const QJsonObject root = document.object();
    if (!document.isObject() || root.value("format").toString() != QLatin1String(FormatTag)) {
        result.error = Tr::tr("The file is not a MultiFilterOutput pattern library export.");
        return result;
    }
    if (!root.value("patterns").isArray()) {
        result.error = Tr::tr("The file holds no list of patterns.");
        return result;
    }
    if (const int version = root.value("version").toInt(); version > FormatVersion) {
        result.warnings << Tr::tr("The file was written by a newer version of MultiFilterOutput "
                                  "(format %1; this version understands up to %2). Anything that "
                                  "version added is left out.")
                               .arg(version)
                               .arg(FormatVersion);
    }

    const QJsonArray array = root.value("patterns").toArray();
    for (qsizetype i = 0; i < array.size(); ++i) {
        if (!array.at(i).isObject()) {
            result.warnings << Tr::tr("Entry %1 of the pattern list is not a pattern; it was "
                                      "skipped.")
                                   .arg(i + 1);
            continue;
        }
        QVariantMap map = array.at(i).toObject().toVariantMap();
        // Back from its name; one that names no color means none.
        const QColor color(map.value("color").toString());
        map.remove("color");
        if (color.isValid())
            map.insert("color", color);
        result.patterns.append(Pattern::fromMap(map));
    }
    // Stable ids for a file that is read again and again (see the header).
    if (!idSeed.isEmpty()) {
        static const QUuid ns("{6f1c2a57-0b7e-4c1e-9d0a-5a3f3e8b2c11}");
        QSet<QString> seen;
        for (qsizetype i = 0; i < result.patterns.size(); ++i) {
            Pattern &p = result.patterns[i];
            if (p.id.isEmpty() || seen.contains(p.id)) {
                p.id = QUuid::createUuidV5(ns, idSeed + u'\n' + QString::number(i))
                           .toString(QUuid::WithoutBraces);
                result.warnings << Tr::tr("Pattern \"%1\" has no id of its own; it was given one "
                                          "from its place in the file, which changes if entries "
                                          "are added or moved above it. Give it an \"id\" so run "
                                          "configurations keep finding it.")
                                       .arg(p.name);
            }
            seen.insert(p.id);
        }
    }
    // What the file may hold that the editor never would: missing or shared ids (repaired),
    // invalid expressions (reported).
    result.warnings += validatePatterns(result.patterns);
    return result;
}

ImportPlan planImport(const QList<Pattern> &library, const QList<Pattern> &imported)
{
    ImportPlan plan;
    for (const Pattern &pattern : imported) {
        const auto existing = std::find_if(library.cbegin(), library.cend(), [&](const Pattern &p) {
            return p.id == pattern.id;
        });
        if (existing == library.cend())
            plan.added.append(pattern);
        else if (existing->toMap() == pattern.toMap())
            ++plan.unchanged;
        else
            plan.conflicting.append(pattern);
    }
    return plan;
}

QList<Pattern> applyImport(
    const QList<Pattern> &library, const ImportPlan &plan, ImportConflicts conflicts)
{
    QList<Pattern> result = library;
    QList<Pattern> copies;
    for (const Pattern &pattern : plan.conflicting) {
        switch (conflicts) {
        case ImportConflicts::Replace:
            for (Pattern &existing : result) {
                if (existing.id == pattern.id)
                    existing = pattern;
            }
            break;
        case ImportConflicts::KeepExisting:
            break;
        case ImportConflicts::KeepBoth: {
            Pattern copy = pattern;
            copy.id = Pattern::createNew().id;
            copy.name = Tr::tr("%1 (imported)").arg(pattern.name);
            copies.append(copy);
            break;
        }
        }
    }
    return result + plan.added + copies;
}

} // namespace MultiFilterOutput

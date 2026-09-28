#include "../src/patternexchange.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtTest>

using namespace MultiFilterOutput;

class tst_PatternExchange : public QObject
{
    Q_OBJECT

private slots:
    void exportThenParseKeepsEveryField();
    void exportIsReadableJson();
    void parseRejectsWhatIsNotAPatternLibrary();
    void parseRepairsIdsAndReportsProblems();
    void parseWarnsAboutNewerFiles();
    void seededParseGivesStableIds();
    void planSortsIntoAddedUnchangedAndConflicting();
    void applyReplaceKeepsTheLibraryOrder();
    void applyKeepExistingOnlyAdds();
    void applyKeepBothAddsRenamedCopies();
};

static Pattern pattern(const QString &id, const QString &name, const QString &text)
{
    Pattern p;
    p.id = id;
    p.name = name;
    p.patternText = text;
    return p;
}

static QStringList ids(const QList<Pattern> &patterns)
{
    QStringList result;
    for (const Pattern &p : patterns)
        result << p.id;
    return result;
}

void tst_PatternExchange::exportThenParseKeepsEveryField()
{
    Pattern p = pattern("id-1", "Errors", "ERR(OR)?");
    p.excludeText = "flaky";
    p.isRegex = false;
    p.caseSensitive = true;
    p.wholeWord = true;
    p.invert = true;
    p.color = QColor(10, 20, 30, 128);
    p.groupContinuationLines = true;
    p.continuationPattern = "^\\s+";
    p.contextBefore = 1;
    p.contextAfter = 2;
    p.isDefault = true;
    p.channels = Pattern::StdErrChannel;
    p.tags = {"net"};
    // Every field off its default, so one the file loses shows up below.
    const QVariantMap defaults = Pattern().toMap();
    const QVariantMap fields = p.toMap();
    for (auto it = fields.cbegin(); it != fields.cend(); ++it)
        QVERIFY2(it.value() != defaults.value(it.key()), qPrintable(it.key()));
    const Pattern plain = pattern("id-2", "Plain", "x"); // no color

    const ParsedPatterns back = parsePatterns(exportPatterns({p, plain}));

    QVERIFY2(back.error.isEmpty(), qPrintable(back.error));
    QCOMPARE(back.patterns.size(), 2);
    QCOMPARE(back.patterns.at(0).toMap(), p.toMap());
    QCOMPARE(back.patterns.at(1).toMap(), plain.toMap());
    QVERIFY(back.warnings.isEmpty());
}

void tst_PatternExchange::exportIsReadableJson()
{
    Pattern red = pattern("id-1", "Red", "x");
    red.color = QColor(255, 0, 0);
    const QJsonObject root = QJsonDocument::fromJson(exportPatterns({red})).object();
    QCOMPARE(root.value("format").toString(), QStringLiteral("MultiFilterOutput.PatternLibrary"));
    QCOMPARE(root.value("version").toInt(), 1);
    const QJsonObject first = root.value("patterns").toArray().first().toObject();
    QCOMPARE(first.value("name").toString(), QStringLiteral("Red"));
    QCOMPARE(first.value("color").toString(), QStringLiteral("#ff0000"));
}

void tst_PatternExchange::parseRejectsWhatIsNotAPatternLibrary()
{
    QVERIFY(!parsePatterns("not json {").error.isEmpty());
    QVERIFY(!parsePatterns("[1, 2]").error.isEmpty());
    QVERIFY(!parsePatterns(R"({"patterns": []})").error.isEmpty()); // no format tag
    QVERIFY(!parsePatterns(R"({"format": "Something.Else", "version": 1, "patterns": []})")
                 .error.isEmpty());
    QVERIFY(!parsePatterns(R"({"format": "MultiFilterOutput.PatternLibrary", "version": 1})")
                 .error.isEmpty()); // no pattern list

    const ParsedPatterns empty = parsePatterns(
        R"({"format": "MultiFilterOutput.PatternLibrary", "version": 1, "patterns": []})");
    QVERIFY(empty.error.isEmpty());
    QVERIFY(empty.patterns.isEmpty());
}

void tst_PatternExchange::parseRepairsIdsAndReportsProblems()
{
    const ParsedPatterns parsed = parsePatterns(R"({
        "format": "MultiFilterOutput.PatternLibrary", "version": 1,
        "patterns": [
            {"id": "same", "name": "First", "patternText": "a"},
            {"id": "same", "name": "Second", "patternText": "b"},
            {"name": "No id", "patternText": "c", "color": "not a color"},
            "not an object"
        ]})");
    QVERIFY(parsed.error.isEmpty());
    QCOMPARE(parsed.patterns.size(), 3);
    QCOMPARE(parsed.patterns.at(0).id, QStringLiteral("same"));
    QVERIFY(parsed.patterns.at(1).id != "same");
    QVERIFY(!parsed.patterns.at(2).id.isEmpty());
    QVERIFY(!parsed.patterns.at(2).color.isValid());
    QCOMPARE(parsed.warnings.size(), 3); // shared id, missing id, the non-object entry
}

void tst_PatternExchange::parseWarnsAboutNewerFiles()
{
    const ParsedPatterns parsed = parsePatterns(
        R"({"format": "MultiFilterOutput.PatternLibrary", "version": 99,
            "patterns": [{"id": "x", "name": "X", "patternText": "x"}]})");
    QVERIFY(parsed.error.isEmpty());
    QCOMPARE(parsed.patterns.size(), 1);
    QCOMPARE(parsed.warnings.size(), 1);
}

void tst_PatternExchange::seededParseGivesStableIds()
{
    const QByteArray json = R"({
        "format": "MultiFilterOutput.PatternLibrary", "version": 1,
        "patterns": [
            {"name": "No id", "patternText": "a"},
            {"id": "same", "name": "First", "patternText": "b"},
            {"id": "same", "name": "Second", "patternText": "c"}
        ]})";
    const ParsedPatterns first = parsePatterns(json, "/work/app/.qtcreator/p.json");
    const ParsedPatterns again = parsePatterns(json, "/work/app/.qtcreator/p.json");
    QVERIFY(!first.patterns.at(0).id.isEmpty());
    QCOMPARE(again.patterns.at(0).id, first.patterns.at(0).id);
    QCOMPARE(first.patterns.at(1).id, QStringLiteral("same"));
    QVERIFY(first.patterns.at(2).id != "same");
    QCOMPARE(again.patterns.at(2).id, first.patterns.at(2).id);
    QVERIFY(first.patterns.at(0).id != first.patterns.at(2).id);
    QCOMPARE(first.warnings.size(), 2); // both still reported

    // Another file: other ids.
    QVERIFY(parsePatterns(json, "/work/other/.qtcreator/p.json").patterns.at(0).id
            != first.patterns.at(0).id);
}

void tst_PatternExchange::planSortsIntoAddedUnchangedAndConflicting()
{
    const QList<Pattern> library{pattern("a", "A", "a"), pattern("b", "B", "b")};
    Pattern changedB = pattern("b", "B", "b changed");
    const ImportPlan plan = planImport(
        library, {pattern("a", "A", "a"), changedB, pattern("c", "C", "c")});

    QCOMPARE(ids(plan.added), QStringList{"c"});
    QCOMPARE(ids(plan.conflicting), QStringList{"b"});
    QCOMPARE(plan.conflicting.first().patternText, QStringLiteral("b changed"));
    QCOMPARE(plan.unchanged, 1);
}

void tst_PatternExchange::applyReplaceKeepsTheLibraryOrder()
{
    const QList<Pattern> library{pattern("a", "A", "a"), pattern("b", "B", "b")};
    const ImportPlan plan = planImport(
        library, {pattern("c", "C", "c"), pattern("a", "A", "a changed")});

    const QList<Pattern> result = applyImport(library, plan, ImportConflicts::Replace);
    QCOMPARE(ids(result), (QStringList{"a", "b", "c"}));
    QCOMPARE(result.at(0).patternText, QStringLiteral("a changed"));
}

void tst_PatternExchange::applyKeepExistingOnlyAdds()
{
    const QList<Pattern> library{pattern("a", "A", "a")};
    const ImportPlan plan = planImport(
        library, {pattern("a", "A", "a changed"), pattern("c", "C", "c")});

    const QList<Pattern> result = applyImport(library, plan, ImportConflicts::KeepExisting);
    QCOMPARE(ids(result), (QStringList{"a", "c"}));
    QCOMPARE(result.at(0).patternText, QStringLiteral("a"));
}

void tst_PatternExchange::applyKeepBothAddsRenamedCopies()
{
    const QList<Pattern> library{pattern("a", "A", "a")};
    const ImportPlan plan = planImport(library, {pattern("a", "A", "a changed")});

    const QList<Pattern> result = applyImport(library, plan, ImportConflicts::KeepBoth);
    QCOMPARE(result.size(), 2);
    QCOMPARE(result.at(0).patternText, QStringLiteral("a"));
    QVERIFY(result.at(1).id != "a" && !result.at(1).id.isEmpty());
    QCOMPARE(result.at(1).name, QStringLiteral("A (imported)"));
    QCOMPARE(result.at(1).patternText, QStringLiteral("a changed"));
}

QTEST_GUILESS_MAIN(tst_PatternExchange)

#include "tst_patternexchange.moc"

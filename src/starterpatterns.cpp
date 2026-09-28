#include "starterpatterns.h"

#include "multifilteroutputtr.h"
#include "patternexchange.h"

namespace MultiFilterOutput {

namespace {

Pattern starter(const char *idSuffix, const QString &name, const QString &regex, const QColor &color)
{
    Pattern p;
    p.id = QStringLiteral("MultiFilterOutput.Starter.") + QLatin1String(idSuffix);
    p.name = name;
    p.patternText = regex;
    p.isRegex = true;
    p.color = color;
    p.tags = {QStringLiteral("starter")};
    return p;
}

} // namespace

QList<Pattern> starterPatterns()
{
    // Whole words, so "0 errors" or "terror" stay out.
    Pattern errors = starter(
        "Errors", Tr::tr("Errors"), R"(\b(error|fatal|critical)\b)", QColor(0xe0, 0x40, 0x40));
    Pattern warnings = starter(
        "Warnings", Tr::tr("Warnings"), R"(\b(warning|warn)\b)", QColor(0xe0, 0xa0, 0x20));
    // console.log() and friends ("qml: ..."), and messages pointing into a .qml file.
    Pattern qml = starter("Qml", Tr::tr("QML"), R"(^qml: |\.qml:\d+)", QColor(0xa0, 0x60, 0xe0));
    // Q_ASSERT ("ASSERT: ..."), Q_ASSERT_X ("ASSERT failure in ..."), and the C runtime's
    // assert() on MSVC ("Assertion failed: ...") and glibc ("Assertion `...' failed.").
    Pattern assertions = starter(
        "Assertions", Tr::tr("Assertions"), R"(^ASSERT[: ]|\bassertion\b.*\bfailed\b)",
        QColor(0xd0, 0x20, 0x90));
    // With the "   Loc: [file(line)]" line QtTest prints under each.
    Pattern qtTest = starter(
        "QtTest", Tr::tr("Qt Test failures"), R"(^(FAIL!|XPASS|QFATAL)\s+:)",
        QColor(0x40, 0x80, 0xe0));
    qtTest.caseSensitive = true;
    qtTest.groupContinuationLines = true;
    qtTest.continuationPattern = R"(^\s+Loc: \[)";

    return {errors, warnings, qml, assertions, qtTest};
}

QList<Pattern> withStarterPatterns(const QList<Pattern> &library)
{
    return applyImport(
        library, planImport(library, starterPatterns()), ImportConflicts::KeepExisting);
}

} // namespace MultiFilterOutput

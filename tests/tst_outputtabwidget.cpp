#include "../src/outputtabwidget.h"

#include <coreplugin/find/ifindsupport.h>

#include <utils/aggregate.h>

#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTextBlock>
#include <QtTest>

using namespace MultiFilterOutput;

class tst_OutputTabWidget : public QObject
{
    Q_OBJECT

private slots:
    void jumpLandsOnMirroredLineAfterAllTabTrims();
    void matchCountChangedIsCoalesced();
    void jumpToLineTrimmedFromAllTabFails();
    void clickMapsCorrectlyAfterPatternTabTrims();
    void longLineIsTruncatedForDisplay();
    void linesUpToLimitAreUntouched();
    void zeroMaxLineLengthMeansNoLimit();
    void truncationNeverSplitsSurrogatePair();
    void truncatedLineStillJumps();
    void truncationShowsPlaceholdersLiterally();
    void truncationDoesNotLeakColorIntoLaterLines();
    void truncationNeverSplitsEscapeSequence();
    void lineIdsWrapInsteadOfOverflowing();
    void lineIdAgeAcrossTheWrap();
    void ctrlFFindsTextInTab();
    void goToMatchStepsThroughMarkedLinesAndWraps();
    void goToMatchWithoutMarksDoesNothing();
    void matchStopsSurviveTrimming();
    void resetClearsMatchStops();
    void saveContentsWritesTabTextAsUtf8();
    void saveContentsReportsFailure();
    void filterShowsOnlyMatchingLines();
    void invertedRegexCaseSensitiveFilter();
    void filterShowsContextLines();
    void filterAppliesToLinesAppendedLater();
    void linesReportTextFormatIdAndNotices();
    void linesOnlyReportWhatTheTabStillHolds();
    void matchStopsCanBeReassignedByLineId();
    void characterLimitDropsOldestLines();
    void characterLimitKeepsNewestLineEvenIfLonger();
    void noCharacterLimitByDefault();
    void lastLineTextIsTheLineAsShown();
    void highlightLastLineTintsExactlyTheSpans();
    void timestampsAreStoredAndReported();
    void formatTimestampShowsLocalTime();
    void timestampGutterAppearsOnlyWhenShown();
    void lineLooksUpOneLineById();

private:
    // Clicks the given block of `tab` and returns what jumpRequested() carried, or -2 if it
    // wasn't emitted at all.
    static int clickBlock(OutputTabWidget &tab, int blockNumber);
};

int tst_OutputTabWidget::clickBlock(OutputTabWidget &tab, int blockNumber)
{
    const QTextBlock block = tab.document()->findBlockByNumber(blockNumber);
    if (!block.isValid())
        return -2;
    const QPoint pos = tab.cursorRect(QTextCursor(block)).center();

    QSignalSpy spy(&tab, &OutputTabWidget::jumpRequested);
    QTest::mouseClick(tab.viewport(), Qt::LeftButton, {}, pos);
    return spy.isEmpty() ? -2 : spy.first().first().toInt();
}

// "All" silently drops its oldest lines once it reaches maximumBlockCount(); a line mirrored
// into a pattern tab must still jump to that same line of "All" afterwards, not to whatever
// now occupies the block number the line had when it was appended.
void tst_OutputTabWidget::jumpLandsOnMirroredLineAfterAllTabTrims()
{
    OutputTabWidget all;
    all.setMaximumBlockCount(10);
    OutputTabWidget pattern;
    pattern.resize(400, 200);
    pattern.show();
    QVERIFY(QTest::qWaitForWindowExposed(&pattern));

    for (int i = 0; i < 25; ++i) {
        const QString line = QStringLiteral("line %1").arg(i);
        const int source = all.appendLine(line, Utils::StdOutFormat);
        if (i == 20)
            pattern.appendLine(line, Utils::StdOutFormat, source);
    }

    const int source = clickBlock(pattern, 0);
    QVERIFY(source >= 0);
    QVERIFY(all.scrollToLine(source));
    QCOMPARE(all.textCursor().block().text(), QStringLiteral("line 20"));
}

// A pattern-tab line whose "All" counterpart has since been trimmed has nowhere to jump to;
// scrollToLine() must say so and leave the view alone, not land on some other line.
void tst_OutputTabWidget::jumpToLineTrimmedFromAllTabFails()
{
    OutputTabWidget all;
    all.setMaximumBlockCount(10);

    int firstLine = -1;
    for (int i = 0; i < 25; ++i) {
        const int line = all.appendLine(QStringLiteral("line %1").arg(i), Utils::StdOutFormat);
        if (i == 0)
            firstLine = line;
    }
    const int cursorBefore = all.textCursor().position();

    QVERIFY(!all.scrollToLine(firstLine));
    QCOMPARE(all.textCursor().position(), cursorBefore);
}

// A pattern tab trims its own front too, once it holds more matches than its cap; a click on
// any of its remaining lines must still carry that line's own "All" line id.
void tst_OutputTabWidget::clickMapsCorrectlyAfterPatternTabTrims()
{
    OutputTabWidget pattern;
    pattern.setMaximumBlockCount(5);
    pattern.resize(400, 300);
    pattern.show();
    QVERIFY(QTest::qWaitForWindowExposed(&pattern));

    // Pretend "All" gave these lines ids 100, 110, 120, ...
    for (int i = 0; i < 12; ++i)
        pattern.appendLine(QStringLiteral("match %1").arg(i), Utils::StdOutFormat, 100 + i * 10);

    const int lines = pattern.document()->blockCount() - 1; // minus the trailing empty block
    QVERIFY(lines > 0);
    for (int block = 0; block < lines; ++block) {
        const int expectedMatch = 12 - lines + block;
        QCOMPARE(
            pattern.document()->findBlockByNumber(block).text(),
            QStringLiteral("match %1").arg(expectedMatch));
        QCOMPARE(clickBlock(pattern, block), 100 + expectedMatch * 10);
    }
}

// Every matchCountChanged() relabels a tab, which relayouts the whole tab bar; a burst of
// appended lines must cost one relabel, not one per line.
void tst_OutputTabWidget::matchCountChangedIsCoalesced()
{
    OutputTabWidget tab;
    QSignalSpy spy(&tab, &OutputTabWidget::matchCountChanged);

    for (int i = 0; i < 100; ++i)
        tab.appendLine(QStringLiteral("line %1").arg(i), Utils::StdOutFormat);
    tab.appendLine("continuation", Utils::StdOutFormat, -1, /*countsAsMatch=*/false);
    QCOMPARE(tab.matchCount(), 100);

    QTRY_COMPARE(spy.size(), 1);
    QCOMPARE(spy.first().first().toInt(), 100);
    QTest::qWait(20);
    QCOMPARE(spy.size(), 1);
}

static QString lastLineText(const OutputTabWidget &tab)
{
    return tab.document()->lastBlock().previous().text();
}

void tst_OutputTabWidget::longLineIsTruncatedForDisplay()
{
    OutputTabWidget tab;
    tab.setMaxLineLength(10);
    tab.appendLine("abcdefghijklmnopqrstuvwxy", Utils::StdOutFormat);
    QCOMPARE(lastLineText(tab), QStringLiteral("abcdefghij ... [15 more characters not shown]"));
}

void tst_OutputTabWidget::linesUpToLimitAreUntouched()
{
    OutputTabWidget tab;
    tab.setMaxLineLength(10);
    tab.appendLine("short", Utils::StdOutFormat);
    QCOMPARE(lastLineText(tab), QStringLiteral("short"));
    tab.appendLine("abcdefghij", Utils::StdOutFormat);
    QCOMPARE(lastLineText(tab), QStringLiteral("abcdefghij"));
}

void tst_OutputTabWidget::zeroMaxLineLengthMeansNoLimit()
{
    OutputTabWidget tab;
    tab.setMaxLineLength(0);
    const QString line(100000, 'x');
    tab.appendLine(line, Utils::StdOutFormat);
    QCOMPARE(lastLineText(tab), line);
}

void tst_OutputTabWidget::truncationNeverSplitsSurrogatePair()
{
    OutputTabWidget tab;
    tab.setMaxLineLength(4);
    // "abc" + U+1F600 (a surrogate pair) + "de": a cut after 4 UTF-16 units would split it.
    tab.appendLine(QStringLiteral("abc") + QString::fromUcs4(U"\U0001F600") + "de", Utils::StdOutFormat);
    QCOMPARE(lastLineText(tab), QStringLiteral("abc ... [4 more characters not shown]"));
}

// Truncation changes what a line displays, not which line it is: click-to-jump still maps a
// truncated pattern-tab line back to its line in "All".
void tst_OutputTabWidget::truncatedLineStillJumps()
{
    OutputTabWidget pattern;
    pattern.setMaxLineLength(10);
    pattern.resize(400, 200);
    pattern.show();
    QVERIFY(QTest::qWaitForWindowExposed(&pattern));

    pattern.appendLine(QString(50, 'x'), Utils::StdOutFormat, 42);
    QCOMPARE(clickBlock(pattern, 0), 42);
}

// printf-style or URL-encoded output routinely contains "%1"; it must not be mistaken for a
// placeholder of the truncation note.
void tst_OutputTabWidget::truncationShowsPlaceholdersLiterally()
{
    OutputTabWidget tab;
    tab.setMaxLineLength(10);
    tab.appendLine("a%1b%2cdefghijklmn", Utils::StdOutFormat);
    QCOMPARE(lastLineText(tab), QStringLiteral("a%1b%2cdef ... [8 more characters not shown]"));
}

static QColor foregroundOf(const QTextBlock &block)
{
    return block.begin().fragment().charFormat().foreground().color();
}

// The formatter's ANSI state carries over from line to line: if truncation drops a colored
// line's closing reset, every later line in the tab would stay colored.
void tst_OutputTabWidget::truncationDoesNotLeakColorIntoLaterLines()
{
    OutputTabWidget tab;
    tab.setMaxLineLength(10);
    tab.appendLine("before", Utils::StdOutFormat);
    tab.appendLine(QStringLiteral("\x1b[31m") + QString(20, 'x') + "\x1b[0m", Utils::StdOutFormat);
    tab.appendLine("after", Utils::StdOutFormat);

    const QTextBlock before = tab.document()->findBlockByNumber(0);
    const QTextBlock after = tab.document()->lastBlock().previous();
    QCOMPARE(after.text(), QStringLiteral("after"));
    QCOMPARE(foregroundOf(after), foregroundOf(before));
}

// Cutting inside an escape sequence would leave half of it to be rendered as text.
void tst_OutputTabWidget::truncationNeverSplitsEscapeSequence()
{
    OutputTabWidget tab;
    tab.setMaxLineLength(10);
    // The cut at 10 falls inside "\x1b[31m", which starts at index 7.
    tab.appendLine(QStringLiteral("abcdefg\x1b[31mhijklmnop"), Utils::StdOutFormat);
    QVERIFY2(lastLineText(tab).startsWith("abcdefg ..."), qPrintable(lastLineText(tab)));
}

// After 2^31 lines in one run an int id would overflow into negatives, which read as "no id";
// ids cycle instead. (Shown with a small modulus -- the real one is INT_MAX.)
void tst_OutputTabWidget::lineIdsWrapInsteadOfOverflowing()
{
    QCOMPARE(nextLineId(5, 100), 6);
    QCOMPARE(nextLineId(98, 100), 99);
    QCOMPARE(nextLineId(99, 100), 0);
    QCOMPARE(nextLineId(LineIdModulus - 1), 0);
}

// scrollToLine() finds a line by how far back from the newest one it is; that distance must
// stay right when the ids have wrapped in between.
void tst_OutputTabWidget::lineIdAgeAcrossTheWrap()
{
    QCOMPARE(lineIdAge(10, 7, 100), 3);
    QCOMPARE(lineIdAge(10, 10, 100), 0);
    QCOMPARE(lineIdAge(2, 98, 100), 4);
    QCOMPARE(lineIdAge(0, 99, 100), 1);
    // Not appended yet (or from before a reset): far older than any tab holds.
    QCOMPARE(lineIdAge(10, 12, 100), 98);
    QCOMPARE(lineIdAge(1, LineIdModulus - 1), 2);
}

// Qt Creator's Find toolbar (Ctrl+F) searches whatever IFindSupport it finds aggregated with
// the focused widget or one of its parents -- the same lookup used here.
void tst_OutputTabWidget::ctrlFFindsTextInTab()
{
    OutputTabWidget tab;
    tab.appendLine("first line", Utils::StdOutFormat);
    tab.appendLine("the needle is here", Utils::StdOutFormat);

    auto *find = Utils::Aggregation::query<Core::IFindSupport>(&tab);
    QVERIFY(find);
    QCOMPARE(find->findStep("needle", {}), Core::IFindSupport::Found);
    QCOMPARE(tab.textCursor().selectedText(), QStringLiteral("needle"));
    QCOMPARE(tab.textCursor().block().text(), QStringLiteral("the needle is here"));
    QCOMPARE(find->findStep("absent", {}), Core::IFindSupport::NotFound);
}

static void appendMarked(OutputTabWidget &tab, const QString &line, int sourceLineId = -1)
{
    tab.appendLine(line, Utils::StdOutFormat, sourceLineId);
    tab.markLastLineAsMatch();
}

// F6 / Shift+F6 step from match to match -- skipping unmarked lines such as continuation
// lines -- and wrap around at either end, like Qt Creator's other navigable panes.
void tst_OutputTabWidget::goToMatchStepsThroughMarkedLinesAndWraps()
{
    OutputTabWidget tab;
    appendMarked(tab, "match one", 10);
    tab.appendLine("  continuation", Utils::StdOutFormat, 11, /*countsAsMatch=*/false);
    appendMarked(tab, "match two", 12);
    tab.appendLine("  continuation", Utils::StdOutFormat, 13, /*countsAsMatch=*/false);
    QVERIFY(tab.hasMatchStops());
    tab.moveCursor(QTextCursor::Start);

    QCOMPARE(tab.goToMatch(OutputTabWidget::Direction::Forward), 12);
    QCOMPARE(tab.textCursor().selectedText(), QStringLiteral("match two"));
    QCOMPARE(tab.goToMatch(OutputTabWidget::Direction::Forward), 10); // wrapped
    QCOMPARE(tab.textCursor().selectedText(), QStringLiteral("match one"));
    QCOMPARE(tab.goToMatch(OutputTabWidget::Direction::Backward), 12); // wrapped back
    QCOMPARE(tab.goToMatch(OutputTabWidget::Direction::Backward), 10);
}

void tst_OutputTabWidget::goToMatchWithoutMarksDoesNothing()
{
    OutputTabWidget tab;
    tab.appendLine("plain", Utils::StdOutFormat);
    tab.moveCursor(QTextCursor::Start);
    const int positionBefore = tab.textCursor().position();

    QVERIFY(!tab.hasMatchStops());
    QCOMPARE(tab.goToMatch(OutputTabWidget::Direction::Forward), -1);
    QCOMPARE(tab.textCursor().position(), positionBefore);
}

// Marks live on the line's block, so trimming the front of the tab takes them with it and
// leaves the rest where they are.
void tst_OutputTabWidget::matchStopsSurviveTrimming()
{
    OutputTabWidget tab;
    tab.setMaximumBlockCount(6);
    for (int i = 0; i < 20; ++i) {
        if (i % 2 == 0)
            appendMarked(tab, QStringLiteral("match %1").arg(i));
        else
            tab.appendLine(QStringLiteral("other %1").arg(i), Utils::StdOutFormat);
    }
    tab.moveCursor(QTextCursor::End);

    tab.goToMatch(OutputTabWidget::Direction::Backward);
    QCOMPARE(tab.textCursor().selectedText(), QStringLiteral("match 18"));
    tab.goToMatch(OutputTabWidget::Direction::Backward);
    QCOMPARE(tab.textCursor().selectedText(), QStringLiteral("match 16"));
}

// A new run or Clear empties the tab; F6 must not stay enabled for matches that are gone.
void tst_OutputTabWidget::resetClearsMatchStops()
{
    OutputTabWidget tab;
    appendMarked(tab, "match");
    tab.reset();
    QVERIFY(!tab.hasMatchStops());
}

void tst_OutputTabWidget::saveContentsWritesTabTextAsUtf8()
{
    OutputTabWidget tab;
    tab.appendLine("first", Utils::StdOutFormat);
    // Non-ASCII, including a character outside Latin-1, must survive as UTF-8.
    const QString second = QStringLiteral("zażółć ✓");
    tab.appendLine(second, Utils::StdOutFormat);
    QTemporaryDir dir;
    const Utils::FilePath path = Utils::FilePath::fromString(dir.filePath("tab.txt"));

    const Utils::Result<> result = tab.saveContents(path);

    QVERIFY2(result.has_value(), qPrintable(result.has_value() ? QString() : result.error()));
    QFile file(path.toFSPathString());
    QVERIFY(file.open(QIODevice::ReadOnly));
    QCOMPARE(QString::fromUtf8(file.readAll()), "first\n" + second + "\n");
}

void tst_OutputTabWidget::saveContentsReportsFailure()
{
    OutputTabWidget tab;
    tab.appendLine("text", Utils::StdOutFormat);
    QTemporaryDir dir;
    // A directory that doesn't exist.
    const Utils::FilePath path = Utils::FilePath::fromString(dir.filePath("missing/tab.txt"));

    QVERIFY(!tab.saveContents(path).has_value());
}

// The lines a filtered tab currently shows (not counting the trailing empty block).
static QStringList visibleLines(const OutputTabWidget &tab)
{
    QStringList lines;
    for (QTextBlock b = tab.document()->firstBlock(); b != tab.document()->lastBlock(); b = b.next()) {
        if (b.isVisible())
            lines << b.text();
    }
    return lines;
}

static void appendAll(OutputTabWidget &tab, const QStringList &lines)
{
    for (const QString &line : lines)
        tab.appendLine(line, Utils::StdOutFormat);
}

void tst_OutputTabWidget::filterShowsOnlyMatchingLines()
{
    OutputTabWidget tab;
    appendAll(tab, {"alpha", "ERROR one", "beta", "error two"});

    tab.setFilter({"error"});
    QCOMPARE(visibleLines(tab), (QStringList{"ERROR one", "error two"}));

    tab.setFilter({});
    QCOMPARE(visibleLines(tab), (QStringList{"alpha", "ERROR one", "beta", "error two"}));
}

void tst_OutputTabWidget::invertedRegexCaseSensitiveFilter()
{
    OutputTabWidget tab;
    appendAll(tab, {"ERROR one", "error two", "info"});

    LineFilter filter;
    filter.text = "^ERR";
    filter.isRegex = true;
    filter.caseSensitivity = Qt::CaseSensitive;
    filter.inverted = true;
    tab.setFilter(filter);

    QCOMPARE(visibleLines(tab), (QStringList{"error two", "info"}));
}

void tst_OutputTabWidget::filterShowsContextLines()
{
    OutputTabWidget tab;
    appendAll(tab, {"a", "b", "MATCH", "c", "d", "e"});

    LineFilter filter;
    filter.text = "MATCH";
    filter.beforeContext = 1;
    filter.afterContext = 2;
    tab.setFilter(filter);

    QCOMPARE(visibleLines(tab), (QStringList{"b", "MATCH", "c", "d"}));
}

// Output keeps arriving while a filter is set: new lines are filtered as they come, context
// included.
void tst_OutputTabWidget::filterAppliesToLinesAppendedLater()
{
    OutputTabWidget tab;
    LineFilter filter;
    filter.text = "MATCH";
    filter.beforeContext = 1;
    filter.afterContext = 1;
    tab.setFilter(filter);

    appendAll(tab, {"x", "y", "MATCH 1", "after", "z", "MATCH 2"});

    QCOMPARE(visibleLines(tab), (QStringList{"y", "MATCH 1", "after", "z", "MATCH 2"}));
}

// Replaying All through changed patterns needs each line back as it was appended: text,
// stdout/stderr, its line id, and whether it was the pane's own notice rather than output.
void tst_OutputTabWidget::linesReportTextFormatIdAndNotices()
{
    OutputTabWidget tab;
    tab.appendLine("out", Utils::StdOutFormat);
    tab.appendLine("err", Utils::StdErrFormat);
    tab.appendLine("[3 lines dropped]", Utils::ErrorMessageFormat);
    tab.markLastLineAsNotice();

    const QList<OutputTabWidget::Line> lines = tab.lines();

    QCOMPARE(lines.size(), 3);
    QCOMPARE(lines.at(0).text, QStringLiteral("out"));
    QCOMPARE(lines.at(0).format, Utils::StdOutFormat);
    QCOMPARE(lines.at(0).lineId, 0);
    QVERIFY(!lines.at(0).isNotice);
    QCOMPARE(lines.at(1).format, Utils::StdErrFormat);
    QCOMPARE(lines.at(1).lineId, 1);
    QCOMPARE(lines.at(2).format, Utils::ErrorMessageFormat);
    QVERIFY(lines.at(2).isNotice);
}

void tst_OutputTabWidget::linesOnlyReportWhatTheTabStillHolds()
{
    OutputTabWidget tab;
    tab.setMaximumBlockCount(4);
    for (int i = 0; i < 10; ++i)
        tab.appendLine(QStringLiteral("line %1").arg(i), Utils::StdOutFormat);

    const QList<OutputTabWidget::Line> lines = tab.lines();

    QCOMPARE(lines.size(), 3); // the cap's 4 blocks include the trailing empty one
    QCOMPARE(lines.first().text, QStringLiteral("line 7"));
    QCOMPARE(lines.first().lineId, 7);
    QCOMPARE(lines.last().text, QStringLiteral("line 9"));
}

// After a replay the match stops must follow the new patterns, not the old ones.
void tst_OutputTabWidget::matchStopsCanBeReassignedByLineId()
{
    OutputTabWidget tab;
    appendMarked(tab, "old match");     // id 0
    tab.appendLine("new match", Utils::StdOutFormat); // id 1

    tab.clearMatchStops();
    QVERIFY(!tab.hasMatchStops());
    tab.markLineAsMatch(1);
    tab.markLineAsMatch(42); // not in the tab: ignored

    tab.moveCursor(QTextCursor::End);
    QCOMPARE(tab.goToMatch(OutputTabWidget::Direction::Backward), 1);
    QCOMPARE(tab.goToMatch(OutputTabWidget::Direction::Backward), 1); // the only stop
}

// Application Output limits what it keeps in characters (10 million by default), not lines;
// a tab following that setting keeps the newest lines within the limit.
void tst_OutputTabWidget::characterLimitDropsOldestLines()
{
    OutputTabWidget tab;
    // Each 9-character line takes 10 characters with its block separator, plus 1 for the
    // trailing empty block: 3 lines would be 31 > 30.
    tab.setMaxCharacterCount(30);
    for (int i = 0; i < 10; ++i) {
        tab.appendLine(QStringLiteral("line %1").arg(i, 4, 10, QChar('0')), Utils::StdOutFormat);
        QVERIFY2(tab.document()->characterCount() <= 30,
                 qPrintable(QString::number(tab.document()->characterCount())));
    }

    const QList<OutputTabWidget::Line> lines = tab.lines();
    QCOMPARE(lines.size(), 2);
    QCOMPARE(lines.first().text, QStringLiteral("line 0008"));
    QCOMPARE(lines.last().text, QStringLiteral("line 0009"));
    // Trimmed through the same front-of-document mechanism line ids are built to survive.
    QCOMPARE(lines.last().lineId, 9);
}

void tst_OutputTabWidget::characterLimitKeepsNewestLineEvenIfLonger()
{
    OutputTabWidget tab;
    tab.setMaxCharacterCount(10);
    tab.appendLine("short", Utils::StdOutFormat);
    tab.appendLine(QString(50, 'x'), Utils::StdOutFormat);

    const QList<OutputTabWidget::Line> lines = tab.lines();
    QCOMPARE(lines.size(), 1);
    QCOMPARE(lines.first().text, QString(50, 'x'));
}

void tst_OutputTabWidget::noCharacterLimitByDefault()
{
    OutputTabWidget tab;
    for (int i = 0; i < 60000; ++i)
        tab.appendLine(QStringLiteral("line %1").arg(i), Utils::StdOutFormat);

    // The old hard-coded cap was 50 000 lines.
    QCOMPARE(tab.lines().size(), 60000);
    // That cap also switched undo off as a side effect (a non-zero maximumBlockCount does);
    // without it, every appended line would be stored a second time in the undo history.
    QVERIFY(!tab.document()->isUndoRedoEnabled());
}

void tst_OutputTabWidget::lastLineTextIsTheLineAsShown()
{
    OutputTabWidget tab;
    tab.setMaxLineLength(5);
    tab.appendLine("first", Utils::StdOutFormat);
    tab.appendLine("abcdefgh", Utils::StdOutFormat);
    QCOMPARE(tab.lastLineText(), QStringLiteral("abcde ... [3 more characters not shown]"));
}

// The background of the character at `index` of the last line.
static QColor backgroundAt(const OutputTabWidget &tab, int index)
{
    const QTextBlock block = tab.document()->lastBlock().previous();
    QTextCursor cursor(block);
    cursor.setPosition(block.position() + index + 1); // the format of the char before the cursor
    return cursor.charFormat().background().color();
}

void tst_OutputTabWidget::highlightLastLineTintsExactlyTheSpans()
{
    OutputTabWidget tab;
    tab.appendLine("an ERROR and another ERROR", Utils::StdOutFormat);
    const QColor tint(255, 200, 0);

    tab.highlightLastLine({{3, 5}, {21, 5}, {24, 100}}, tint);

    QVERIFY(backgroundAt(tab, 2) != tint);
    QCOMPARE(backgroundAt(tab, 3), tint);
    QCOMPARE(backgroundAt(tab, 7), tint);
    QVERIFY(backgroundAt(tab, 8) != tint);
    QCOMPARE(backgroundAt(tab, 21), tint);
    QCOMPARE(backgroundAt(tab, 25), tint); // the out-of-range span was clipped, not dropped
    // Only formatting changed, never the text.
    QCOMPARE(tab.lastLineText(), QStringLiteral("an ERROR and another ERROR"));
}

void tst_OutputTabWidget::timestampsAreStoredAndReported()
{
    OutputTabWidget tab;
    tab.appendLine("stamped", Utils::StdOutFormat, -1, true, 1234567890123);
    tab.appendLine("unstamped", Utils::StdOutFormat);

    const QList<OutputTabWidget::Line> lines = tab.lines();
    QCOMPARE(lines.at(0).timestamp, 1234567890123);
    QCOMPARE(lines.at(1).timestamp, 0);
}

void tst_OutputTabWidget::formatTimestampShowsLocalTime()
{
    const QDateTime at(QDate(2026, 9, 28), QTime(13, 4, 5, 67));
    QCOMPARE(formatTimestamp(at.toMSecsSinceEpoch()), QStringLiteral("13:04:05.067"));
}

// The gutter is room beside the text, not text: switching it on changes the margin, never
// what Find, Copy or the filters see.
void tst_OutputTabWidget::timestampGutterAppearsOnlyWhenShown()
{
    OutputTabWidget tab;
    tab.appendLine("text", Utils::StdOutFormat, -1, true, 1234567890123);
    QVERIFY(!tab.showTimestamps());
    // The text area (viewport) moves right to make room for the gutter.
    const int textLeft = tab.viewport()->x();

    tab.setShowTimestamps(true);
    QVERIFY(tab.showTimestamps());
    QVERIFY(tab.viewport()->x() > textLeft);
    QCOMPARE(tab.lastLineText(), QStringLiteral("text"));

    tab.setShowTimestamps(false);
    QCOMPARE(tab.viewport()->x(), textLeft);
}

void tst_OutputTabWidget::lineLooksUpOneLineById()
{
    OutputTabWidget tab;
    const int first = tab.appendLine("first", Utils::StdOutFormat, -1, true, 111);
    const int second = tab.appendLine("second", Utils::StdErrFormat, -1, true, 222);

    const std::optional<OutputTabWidget::Line> found = tab.line(second);
    QVERIFY(found);
    QCOMPARE(found->text, QStringLiteral("second"));
    QCOMPARE(found->format, Utils::StdErrFormat);
    QCOMPARE(found->lineId, second);
    QCOMPARE(found->timestamp, 222);
    QCOMPARE(tab.line(first)->text, QStringLiteral("first"));
    QVERIFY(!tab.line(second + 1));

    // Dropped for room: gone.
    tab.setMaxCharacterCount(8);
    QVERIFY(!tab.line(first));
}

QTEST_MAIN(tst_OutputTabWidget)

#include "tst_outputtabwidget.moc"

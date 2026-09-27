#include "../src/outputtabwidget.h"

#include <coreplugin/find/ifindsupport.h>

#include <utils/aggregate.h>

#include <QSignalSpy>
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

QTEST_MAIN(tst_OutputTabWidget)

#include "tst_outputtabwidget.moc"

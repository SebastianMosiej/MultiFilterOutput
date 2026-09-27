#include "../src/outputtabwidget.h"

#include <QSignalSpy>
#include <QTextBlock>
#include <QtTest>

using namespace MultiFilterOutput;

class tst_OutputTabWidget : public QObject
{
    Q_OBJECT

private slots:
    void jumpLandsOnMirroredLineAfterAllTabTrims();
    void jumpToLineTrimmedFromAllTabFails();
    void clickMapsCorrectlyAfterPatternTabTrims();

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

QTEST_MAIN(tst_OutputTabWidget)

#include "tst_outputtabwidget.moc"

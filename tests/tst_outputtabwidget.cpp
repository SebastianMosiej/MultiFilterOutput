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

QTEST_MAIN(tst_OutputTabWidget)

#include "tst_outputtabwidget.moc"

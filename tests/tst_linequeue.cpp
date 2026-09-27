#include "../src/linequeue.h"

#include <QtTest>

#include <thread>

using namespace MultiFilterOutput;
using namespace std::chrono_literals;

class tst_LineQueue : public QObject
{
    Q_OBJECT

private slots:
    void linesReachSinkFromEventLoopInOrder();
    void oneSliceStopsAtTimeBudget();
    void clearDropsPendingLines();
    void overflowDropsOldestAndReportsItInOrder();
    void backlogWithinCapLosesNothing();
    void clearForgetsUnreportedDiscards();
};

// Enqueuing must return straight away -- the whole point is that the caller (a RunControl
// signal handler) never does the per-line work itself.
void tst_LineQueue::linesReachSinkFromEventLoopInOrder()
{
    QStringList received;
    LineQueue queue([&](const QString &line, Utils::OutputFormat) { received << line; });

    queue.enqueue("a", Utils::StdOutFormat);
    queue.enqueue("b", Utils::StdErrFormat);
    queue.enqueue("c", Utils::StdOutFormat);
    QVERIFY(received.isEmpty());
    QVERIFY(!queue.isEmpty());

    QTRY_COMPARE(received, (QStringList{"a", "b", "c"}));
    QVERIFY(queue.isEmpty());
}

// A slice gives the event loop back once its budget is spent, even with lines left over,
// and the remainder still arrives in later slices.
void tst_LineQueue::oneSliceStopsAtTimeBudget()
{
    int received = 0;
    LineQueue queue(
        [&](const QString &, Utils::OutputFormat) {
            std::this_thread::sleep_for(5ms);
            ++received;
        },
        10ms);
    for (int i = 0; i < 10; ++i)
        queue.enqueue(QString::number(i), Utils::StdOutFormat);

    const int processed = queue.processPending();
    QVERIFY2(processed >= 1, qPrintable(QString::number(processed)));
    QVERIFY2(processed < 10, qPrintable(QString::number(processed)));

    QTRY_COMPARE(received, 10);
}

// Used when a new run replaces the old one: lines still queued from the old run must not
// land in the new run's freshly reset tabs.
void tst_LineQueue::clearDropsPendingLines()
{
    QStringList received;
    LineQueue queue([&](const QString &line, Utils::OutputFormat) { received << line; });
    queue.enqueue("old", Utils::StdOutFormat);

    queue.clear();
    QVERIFY(queue.isEmpty());
    QTest::qWait(50);
    QVERIFY(received.isEmpty());
}

// An app outputting faster than the pane can ever filter must not grow Qt Creator's memory
// without bound: the oldest backlog goes, and the user is told -- in order, in its place.
void tst_LineQueue::overflowDropsOldestAndReportsItInOrder()
{
    QStringList received;
    LineQueue queue([&](const QString &line, Utils::OutputFormat) { received << line; });
    queue.setOverflowHandling(10, [&](int discarded) {
        received << QStringLiteral("[%1 discarded]").arg(discarded);
    });

    // The 4th line makes 12 characters > 10: the oldest go until at most half the cap (5)
    // is left -- aaa, bbb, ccc -- and eee then fits again (6).
    for (const char *line : {"aaa", "bbb", "ccc", "ddd", "eee"})
        queue.enqueue(line, Utils::StdOutFormat);

    QTRY_COMPARE(received, (QStringList{"[3 discarded]", "ddd", "eee"}));
}

void tst_LineQueue::backlogWithinCapLosesNothing()
{
    QStringList received;
    int discards = 0;
    LineQueue queue([&](const QString &line, Utils::OutputFormat) { received << line; });
    queue.setOverflowHandling(9, [&](int) { ++discards; });

    for (const char *line : {"aaa", "bbb", "ccc"})
        queue.enqueue(line, Utils::StdOutFormat);

    QTRY_COMPARE(received, (QStringList{"aaa", "bbb", "ccc"}));
    QCOMPARE(discards, 0);
}

// clear() starts a new run's output: a discard notice belonging to the old run must not
// show up in it.
void tst_LineQueue::clearForgetsUnreportedDiscards()
{
    QStringList received;
    LineQueue queue([&](const QString &line, Utils::OutputFormat) { received << line; });
    queue.setOverflowHandling(4, [&](int discarded) {
        received << QStringLiteral("[%1 discarded]").arg(discarded);
    });
    for (const char *line : {"aaa", "bbb"})
        queue.enqueue(line, Utils::StdOutFormat);

    queue.clear();
    queue.enqueue("new", Utils::StdOutFormat);

    QTRY_COMPARE(received, QStringList{"new"});
}

QTEST_GUILESS_MAIN(tst_LineQueue)

#include "tst_linequeue.moc"

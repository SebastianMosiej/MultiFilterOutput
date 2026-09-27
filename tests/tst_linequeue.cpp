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
    void floodIsDiscardedWhenEnabled();
    void floodIsKeptByDefault();
    void hopelessBacklogIsDiscardedWhenEnabled();
    void drainingBacklogIsKept();
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

// Output arriving faster than it is filtered: the backlog never shrinks. With 0 ms slices each
// slice handles exactly one line, so "5 slices' worth" is 5 lines. Slices are run by hand
// (no event loop), exactly as the queue's timer would.
static QStringList runFlood(LineQueue &queue, QStringList &received)
{
    for (int i = 0; i < 20; ++i)
        queue.enqueue(QStringLiteral("line %1").arg(i), Utils::StdOutFormat);
    for (int slice = 0; slice < 15; ++slice) {
        queue.processPending();
        queue.enqueue("more", Utils::StdOutFormat);
        queue.enqueue("more", Utils::StdOutFormat);
    }
    return received;
}

void tst_LineQueue::floodIsDiscardedWhenEnabled()
{
    QStringList received;
    LineQueue queue([&](const QString &line, Utils::OutputFormat) { received << line; }, 0ms);
    queue.setOverflowHandling(LineQueue::DefaultMaxQueuedCharacters, [&](int discarded) {
        received << QStringLiteral("[%1 discarded]").arg(discarded);
    });
    queue.setDiscardExcessiveOutput(true);

    runFlood(queue, received);

    const auto notice = std::find_if(received.cbegin(), received.cend(), [](const QString &l) {
        return l.endsWith(" discarded]");
    });
    QVERIFY2(notice != received.cend(), qPrintable(received.join(", ")));
    // The flood was detected on the 11th non-shrinking sample: 10 lines shown, then the notice.
    QCOMPARE(notice - received.cbegin(), 10);
}

// Application Output leaves output alone unless the user opts in; so does the pane.
void tst_LineQueue::floodIsKeptByDefault()
{
    QStringList received;
    LineQueue queue([&](const QString &line, Utils::OutputFormat) { received << line; }, 0ms);
    queue.setOverflowHandling(LineQueue::DefaultMaxQueuedCharacters, [&](int discarded) {
        received << QStringLiteral("[%1 discarded]").arg(discarded);
    });

    runFlood(queue, received);

    QCOMPARE(received.filter(" discarded]").size(), 0);
    QCOMPARE(received.size(), 15);
}

// A backlog that does shrink, but so slowly that working through it would take minutes: the
// "too slow" criterion. 10 ms slices each taking one 10 ms line: 7000 lines = 70 s.
void tst_LineQueue::hopelessBacklogIsDiscardedWhenEnabled()
{
    QStringList received;
    LineQueue queue(
        [&](const QString &line, Utils::OutputFormat) {
            std::this_thread::sleep_for(10ms);
            received << line;
        },
        10ms);
    queue.setOverflowHandling(LineQueue::DefaultMaxQueuedCharacters, [&](int discarded) {
        received << QStringLiteral("[%1 discarded]").arg(discarded);
    });
    queue.setDiscardExcessiveOutput(true);
    for (int i = 0; i < 7000; ++i)
        queue.enqueue(QStringLiteral("line %1").arg(i), Utils::StdOutFormat);

    for (int slice = 0; slice < 12 && !queue.isEmpty(); ++slice)
        queue.processPending();

    QVERIFY(queue.isEmpty());
    QVERIFY2(!received.filter(" discarded]").isEmpty(), qPrintable(received.join(", ")));
}

void tst_LineQueue::drainingBacklogIsKept()
{
    QStringList received;
    LineQueue queue([&](const QString &line, Utils::OutputFormat) { received << line; }, 0ms);
    queue.setOverflowHandling(LineQueue::DefaultMaxQueuedCharacters, [&](int discarded) {
        received << QStringLiteral("[%1 discarded]").arg(discarded);
    });
    queue.setDiscardExcessiveOutput(true);
    for (int i = 0; i < 100; ++i)
        queue.enqueue(QStringLiteral("line %1").arg(i), Utils::StdOutFormat);

    while (!queue.isEmpty())
        queue.processPending();

    QCOMPARE(received.size(), 100);
    QCOMPARE(received.filter(" discarded]").size(), 0);
}

QTEST_GUILESS_MAIN(tst_LineQueue)

#include "tst_linequeue.moc"

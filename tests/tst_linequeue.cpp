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

QTEST_GUILESS_MAIN(tst_LineQueue)

#include "tst_linequeue.moc"

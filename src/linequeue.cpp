#include "linequeue.h"

#include <QElapsedTimer>

#include <utility>

namespace MultiFilterOutput {

LineQueue::LineQueue(Sink sink, std::chrono::milliseconds budget, QObject *parent)
    : QObject(parent)
    , m_sink(std::move(sink))
    , m_budget(budget)
{
    // Interval 0: the next slice runs as soon as the event loop has handled whatever else is
    // waiting (paints, input), which is exactly the gap a burst of output must leave open.
    m_timer.setSingleShot(true);
    m_timer.setInterval(0);
    connect(&m_timer, &QTimer::timeout, this, &LineQueue::processPending);
}

void LineQueue::setOverflowHandling(qsizetype maxQueuedCharacters, DiscardSink onDiscarded)
{
    m_maxQueuedCharacters = maxQueuedCharacters;
    m_discardSink = std::move(onDiscarded);
}

void LineQueue::enqueue(const QString &line, Utils::OutputFormat format)
{
    m_queue.enqueue({line, format});
    m_queuedCharacters += line.size();
    if (m_queuedCharacters > m_maxQueuedCharacters) {
        // Oldest first, down to half the cap; the newest line always stays.
        while (m_queue.size() > 1 && m_queuedCharacters > m_maxQueuedCharacters / 2) {
            m_queuedCharacters -= m_queue.dequeue().first.size();
            ++m_discardedPending;
        }
    }
    if (!m_timer.isActive())
        m_timer.start();
}

void LineQueue::clear()
{
    m_queue.clear();
    m_queuedCharacters = 0;
    m_discardedPending = 0;
    m_timer.stop();
}

bool LineQueue::isEmpty() const
{
    return m_queue.isEmpty();
}

int LineQueue::processPending()
{
    QElapsedTimer elapsed;
    elapsed.start();

    int processed = 0;
    while (!m_queue.isEmpty()) {
        // Everything dropped came before whatever is still queued.
        if (m_discardedPending > 0) {
            if (m_discardSink)
                m_discardSink(std::exchange(m_discardedPending, 0));
            else
                m_discardedPending = 0;
        }
        const auto [line, format] = m_queue.dequeue();
        m_queuedCharacters -= line.size();
        m_sink(line, format);
        ++processed;
        if (elapsed.durationElapsed() >= m_budget)
            break;
    }

    if (!m_queue.isEmpty() && !m_timer.isActive())
        m_timer.start();
    return processed;
}

} // namespace MultiFilterOutput

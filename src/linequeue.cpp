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

void LineQueue::setDiscardExcessiveOutput(bool enabled)
{
    m_discardExcessiveOutput = enabled;
}

bool LineQueue::isHopelesslyBehind()
{
    const qsizetype backlog = m_queue.size();

    // Being flooded: the backlog hasn't shrunk for the last ten slices and is considerably
    // more than a slice's worth.
    if (!m_backlogHistory.isEmpty() && m_backlogHistory.last() > backlog)
        m_backlogHistory.clear();
    m_backlogHistory << backlog;
    if (m_backlogHistory.size() > 10 && backlog > 5 * m_lastSliceLines)
        return true;

    // Too slow: at the current rate, working through the backlog would take over a minute --
    // judged only once a reasonable amount of output has been shown.
    return m_slices >= 10 && m_lastSliceLines > 0
           && (backlog / m_lastSliceLines) * m_budget > std::chrono::minutes(1);
}

void LineQueue::reportDiscards()
{
    if (m_discardedPending == 0)
        return;
    if (m_discardSink)
        m_discardSink(std::exchange(m_discardedPending, 0));
    else
        m_discardedPending = 0;
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
    m_backlogHistory.clear();
    m_slices = 0;
    m_lastSliceLines = 0;
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

    if (m_discardExcessiveOutput && !m_queue.isEmpty() && isHopelesslyBehind()) {
        m_discardedPending += int(m_queue.size());
        m_queue.clear();
        m_queuedCharacters = 0;
        m_backlogHistory.clear();
    }

    int processed = 0;
    while (!m_queue.isEmpty()) {
        // Everything dropped came before whatever is still queued.
        reportDiscards();
        const auto [line, format] = m_queue.dequeue();
        m_queuedCharacters -= line.size();
        m_sink(line, format);
        ++processed;
        if (elapsed.durationElapsed() >= m_budget)
            break;
    }

    if (processed > 0) {
        m_lastSliceLines = processed;
        ++m_slices;
    }
    // Nothing left to put the notice in front of (the whole backlog was dropped): say so now.
    if (m_queue.isEmpty())
        reportDiscards();

    if (!m_queue.isEmpty() && !m_timer.isActive())
        m_timer.start();
    return processed;
}

} // namespace MultiFilterOutput

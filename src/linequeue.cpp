#include "linequeue.h"

#include <QElapsedTimer>

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

void LineQueue::enqueue(const QString &line, Utils::OutputFormat format)
{
    m_queue.enqueue({line, format});
    if (!m_timer.isActive())
        m_timer.start();
}

void LineQueue::clear()
{
    m_queue.clear();
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
        const auto [line, format] = m_queue.dequeue();
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

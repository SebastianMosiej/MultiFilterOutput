#pragma once

#include "multifilteroutput_global.h"

#include <utils/outputformat.h>

#include <QObject>
#include <QQueue>
#include <QString>
#include <QTimer>

#include <chrono>
#include <functional>
#include <utility>

namespace MultiFilterOutput {

// Buffers output lines and hands them to a sink from the event loop in time-limited slices,
// so a burst of output can't block the GUI thread for longer than one slice at a time --
// paints and input get handled in between. The same idea as Core::OutputWindow's queued
// output chunks, simplified: a fixed time budget, and nothing is ever discarded.
class MULTIFILTEROUTPUT_EXPORT LineQueue : public QObject
{
    Q_OBJECT

public:
    using Sink = std::function<void(const QString &line, Utils::OutputFormat format)>;
    // Called, in order, in place of lines dropped because the backlog overflowed.
    using DiscardSink = std::function<void(int discardedLines)>;

    // About 64 MB of queued text: far beyond anything a merely busy run leaves queued, so
    // only a runaway one -- output arriving faster than the pane can ever filter it -- hits
    // it. (A single line can be up to 64 KiB, see LineBuffer, so the cap is on characters,
    // not lines.)
    static constexpr qsizetype DefaultMaxQueuedCharacters = 32 * 1024 * 1024;

    explicit LineQueue(
        Sink sink,
        std::chrono::milliseconds budget = std::chrono::milliseconds(10),
        QObject *parent = nullptr);

    // Once more than maxQueuedCharacters are waiting, the oldest queued lines are dropped
    // -- down to half the cap, so a steady flood doesn't trip it on every line -- and
    // onDiscarded is called with how many, at the point in the order where they were, so the
    // user sees that output went missing instead of it vanishing silently. Keeps Qt
    // Creator's memory bounded when an app outputs faster than the pane can filter.
    void setOverflowHandling(qsizetype maxQueuedCharacters, DiscardSink onDiscarded);

    void enqueue(const QString &line, Utils::OutputFormat format);
    // Drops every line not yet handed to the sink.
    void clear();
    bool isEmpty() const;

    // Hands queued lines to the sink until the queue is empty or the time budget is used up
    // (always at least one line), and reschedules itself if lines remain. Returns how many
    // lines it processed. Driven by the internal timer; public so tests can run one slice.
    int processPending();

private:
    Sink m_sink;
    std::chrono::milliseconds m_budget;
    QQueue<std::pair<QString, Utils::OutputFormat>> m_queue;
    qsizetype m_queuedCharacters = 0;
    qsizetype m_maxQueuedCharacters = DefaultMaxQueuedCharacters;
    DiscardSink m_discardSink;
    // Lines dropped since the last report to m_discardSink; reported before the next line.
    int m_discardedPending = 0;
    QTimer m_timer;
};

} // namespace MultiFilterOutput

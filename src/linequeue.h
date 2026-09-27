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

    explicit LineQueue(
        Sink sink,
        std::chrono::milliseconds budget = std::chrono::milliseconds(10),
        QObject *parent = nullptr);

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
    QTimer m_timer;
};

} // namespace MultiFilterOutput

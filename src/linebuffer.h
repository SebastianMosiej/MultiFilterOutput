#pragma once

#include "multifilteroutput_global.h"

#include <utils/outputformat.h>

#include <QList>
#include <QMap>
#include <QString>
#include <QStringList>

namespace MultiFilterOutput {

// Splits a stream of output chunks into complete lines. Lines are what a terminal (and
// Application Output) would display: a lone "\r" returns to the start of the line, so only
// the text after the last one is kept -- see applyCarriageReturns() in the .cpp.
class MULTIFILTEROUTPUT_EXPORT LineBuffer
{
public:
    static constexpr qsizetype DefaultMaxLineLength = 64 * 1024;

    // No line is ever longer than maxLineLength: longer text -- a complete line, or output
    // that simply never contains a newline (progress bars, binary dumps) -- is emitted in
    // pieces of at most that length, so the pending partial line can't grow without bound.
    explicit LineBuffer(qsizetype maxLineLength = DefaultMaxLineLength);

    QStringList feed(const QString &chunk);
    // Returns the pending incomplete line (text fed since the last newline), if any, and
    // empties the buffer -- for when the stream has ended and no newline is coming.
    QString flush();

private:
    qsizetype m_maxLineLength;
    QString m_partial;
};

// Splits one run's output into complete lines, keeping each output format (stdout, stderr,
// Qt Creator's own messages, ...) in a LineBuffer of its own. A single shared buffer would
// glue an unterminated fragment of one stream onto the next chunk of another -- common in
// practice, since stdout is block-buffered when piped while stderr is not -- producing one
// line that belongs to neither stream and truncating the fragment's own line.
class MULTIFILTEROUTPUT_EXPORT OutputLineSplitter
{
public:
    struct Line
    {
        QString text;
        Utils::OutputFormat format;
    };

    QList<Line> feed(const QString &chunk, Utils::OutputFormat format);
    // Returns every format's pending incomplete line (see LineBuffer::flush()), in
    // Utils::OutputFormat order, and empties all buffers.
    QList<Line> flush();

private:
    QMap<Utils::OutputFormat, LineBuffer> m_buffers;
};

} // namespace MultiFilterOutput

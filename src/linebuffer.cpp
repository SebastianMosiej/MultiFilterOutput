#include "linebuffer.h"

#include <utility>

namespace MultiFilterOutput {

namespace {

// Where to cut a piece of at most maxLength UTF-16 units off the front of `text`: maxLength,
// unless that would separate a surrogate pair, in which case one unit earlier.
qsizetype cutLength(const QString &text, qsizetype maxLength)
{
    if (maxLength > 1 && text.at(maxLength - 1).isHighSurrogate())
        return maxLength - 1;
    return maxLength;
}

// Moves maxLength-sized pieces off the front of `text` into `out` for as long as `text` is
// longer than maxLength, leaving the (non-empty) remainder in `text`.
void takeCappedPieces(QString &text, qsizetype maxLength, QStringList &out)
{
    while (text.size() > maxLength) {
        const qsizetype n = cutLength(text, maxLength);
        out.append(text.left(n));
        text.remove(0, n);
    }
}

// Reduces a complete line to what a terminal -- and Application Output, see
// Utils::OutputFormatter::append() -- displays for it. Trailing "\r"s just end the line
// (Utils::normalizeNewlines() collapses "\r\r\n" to "\n"); any other "\r" returns to the
// start of the line, so the text after the last one replaces everything before it.
void applyCarriageReturns(QString &line)
{
    while (line.endsWith('\r'))
        line.chop(1);
    const qsizetype cr = line.lastIndexOf('\r');
    if (cr >= 0)
        line.remove(0, cr + 1);
}

// The same for the pending partial line, except that a trailing run of "\r"s is kept: it may
// be the first half of a "\r\n" arriving in the next chunk. This is also what keeps a
// progress bar (redrawn with "\r", no newline until it is done) from piling up.
void applyCarriageReturnsToPartial(QString &partial)
{
    qsizetype end = partial.size();
    while (end > 0 && partial.at(end - 1) == '\r')
        --end;
    if (end == 0)
        return;
    const qsizetype cr = partial.lastIndexOf('\r', end - 1);
    if (cr >= 0)
        partial.remove(0, cr + 1);
}

} // namespace

LineBuffer::LineBuffer(qsizetype maxLineLength)
    : m_maxLineLength(maxLineLength)
{}

QStringList LineBuffer::feed(const QString &chunk)
{
    QString combined = m_partial + chunk;
    combined.replace("\r\n", "\n");

    QStringList parts = combined.split('\n');
    // The last element is either "" (chunk ended on a newline) or a partial line.
    m_partial = parts.takeLast();

    QStringList lines;
    for (QString &part : parts) {
        applyCarriageReturns(part);
        takeCappedPieces(part, m_maxLineLength, lines);
        lines.append(part);
    }
    applyCarriageReturnsToPartial(m_partial);
    // Cap only the content, not a trailing run of "\r"s -- possibly the first half of a "\r\n"
    // split across chunks. Those stay pending as they are: counting them would force out
    // cap-length content (followed by a spurious empty line once the "\n" arrives), and a
    // piece could end in a "\r" that never went through applyCarriageReturns().
    qsizetype contentEnd = m_partial.size();
    while (contentEnd > 0 && m_partial.at(contentEnd - 1) == '\r')
        --contentEnd;
    QString content = m_partial.left(contentEnd);
    takeCappedPieces(content, m_maxLineLength, lines);
    m_partial = content + m_partial.mid(contentEnd);
    return lines;
}

QString LineBuffer::flush()
{
    // feed() only normalizes a "\r\n" it sees whole, so a chunk ending between the two
    // leaves the "\r" pending; with no "\n" coming it is line-ending residue, not content --
    // i.e. the pending line is now simply a complete one.
    applyCarriageReturns(m_partial);
    return std::exchange(m_partial, QString());
}

QList<OutputLineSplitter::Line> OutputLineSplitter::feed(
    const QString &chunk, Utils::OutputFormat format)
{
    QList<Line> result;
    for (const QString &line : m_buffers[format].feed(chunk))
        result.append({line, format});
    return result;
}

QList<OutputLineSplitter::Line> OutputLineSplitter::flush()
{
    QList<Line> result;
    for (auto it = m_buffers.begin(); it != m_buffers.end(); ++it) {
        const QString pending = it.value().flush();
        if (!pending.isEmpty())
            result.append({pending, it.key()});
    }
    return result;
}

} // namespace MultiFilterOutput

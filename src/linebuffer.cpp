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
        takeCappedPieces(part, m_maxLineLength, lines);
        lines.append(part);
    }
    // Only ever leaves a non-empty remainder behind, so a trailing "\r" -- possibly the first
    // half of a "\r\n" split across chunks -- always stays pending rather than being emitted.
    takeCappedPieces(m_partial, m_maxLineLength, lines);
    return lines;
}

QString LineBuffer::flush()
{
    // feed() only normalizes a "\r\n" it sees whole, so a chunk ending between the two
    // leaves the "\r" pending; with no "\n" coming it is line-ending residue, not content.
    if (m_partial.endsWith('\r'))
        m_partial.chop(1);
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

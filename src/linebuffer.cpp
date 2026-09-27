#include "linebuffer.h"

#include <utility>

namespace MultiFilterOutput {

QStringList LineBuffer::feed(const QString &chunk)
{
    QString combined = m_partial + chunk;
    combined.replace("\r\n", "\n");

    QStringList lines = combined.split('\n');
    // The last element is either "" (chunk ended on a newline) or a partial line.
    m_partial = lines.takeLast();
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

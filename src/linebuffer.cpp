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

} // namespace MultiFilterOutput

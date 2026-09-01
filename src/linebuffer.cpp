#include "linebuffer.h"

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

} // namespace MultiFilterOutput

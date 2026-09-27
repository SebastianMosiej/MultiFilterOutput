#pragma once

#include "multifilteroutput_global.h"

#include <QString>
#include <QStringList>

namespace MultiFilterOutput {

class MULTIFILTEROUTPUT_EXPORT LineBuffer
{
public:
    QStringList feed(const QString &chunk);
    // Returns the pending incomplete line (text fed since the last newline), if any, and
    // empties the buffer -- for when the stream has ended and no newline is coming.
    QString flush();

private:
    QString m_partial;
};

} // namespace MultiFilterOutput

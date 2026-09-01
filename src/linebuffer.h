#pragma once

#include "multifilteroutput_global.h"

#include <QString>
#include <QStringList>

namespace MultiFilterOutput {

class MULTIFILTEROUTPUT_EXPORT LineBuffer
{
public:
    QStringList feed(const QString &chunk);

private:
    QString m_partial;
};

} // namespace MultiFilterOutput

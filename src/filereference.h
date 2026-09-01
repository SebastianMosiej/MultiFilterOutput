#pragma once

#include "multifilteroutput_global.h"

#include <QList>
#include <QString>

namespace MultiFilterOutput {

struct MULTIFILTEROUTPUT_EXPORT FileReference
{
    int start = -1;
    int length = 0;
    QString target;
};

MULTIFILTEROUTPUT_EXPORT QList<FileReference> findFileReferences(const QString &line);

} // namespace MultiFilterOutput

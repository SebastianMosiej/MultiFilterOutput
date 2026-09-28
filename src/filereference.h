#pragma once

#include "multifilteroutput_global.h"

#include <QList>
#include <QString>

#include <optional>

namespace MultiFilterOutput {

// A file location mentioned in a line of output, for linkifying it (see
// MultiFilterLineParser).
struct MULTIFILTEROUTPUT_EXPORT FileReference
{
    // The linked text: line.mid(start, length) == target.
    int start = -1;
    int length = 0;
    QString target;
    // Where it points: a local path as written (relative, native separators and all) or a
    // qrc:/file: URL; 1-based line and column, -1 when not given.
    QString path;
    int line = -1;
    int column = -1;
};

// Every file location in `line`, left to right, in the formats of: QML (qrc:/file: URLs),
// Qt's Object:: warnings and Q_ASSERT/Q_ASSERT_X, GCC/Clang, GTest and sanitizers
// (path:line[:column]), MSVC (path(line[,column])), Qt Test ("Loc: [path(line)]") and Python
// tracebacks (File "path", line N). A bare path:line counts only with a file extension, so
// times like 12:00:01 stay plain text.
MULTIFILTEROUTPUT_EXPORT QList<FileReference> findFileReferences(const QString &line);

// What a link carries (Utils::OutputLineParser::LinkSpec's target), and back: path, line and
// column only.
MULTIFILTEROUTPUT_EXPORT QString fileLinkHref(const FileReference &reference);
MULTIFILTEROUTPUT_EXPORT std::optional<FileReference> fileReferenceFromHref(const QString &href);

} // namespace MultiFilterOutput

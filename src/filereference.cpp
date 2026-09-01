#include "filereference.h"

#include <QRegularExpression>

namespace MultiFilterOutput {

namespace {

const char QT_QML_URL_REGEXP[] = "(?:file|qrc):(?://)?/.+?";

QList<FileReference> collectMatches(
    const QString &line, const QRegularExpression &re, int captureGroup)
{
    QList<FileReference> result;
    QRegularExpressionMatchIterator it = re.globalMatch(line);
    while (it.hasNext()) {
        const QRegularExpressionMatch m = it.next();
        FileReference ref;
        ref.start = m.capturedStart(captureGroup);
        ref.length = m.capturedLength(captureGroup);
        ref.target = m.captured(captureGroup);
        result.append(ref);
    }
    return result;
}

} // namespace

QList<FileReference> findFileReferences(const QString &line)
{
    static const QRegularExpression qmlUrlRe(
        QString("(%1:\\d+(?::\\d+)?)\\b").arg(QT_QML_URL_REGEXP));
    static const QRegularExpression objectWarningRe("Object::.*in (.*:\\d+)");
    static const QRegularExpression assertRe("ASSERT: .* in file (.+, line \\d+)");
    static const QRegularExpression assertXRe("ASSERT failure in .*: \".*\", file (.+, line \\d+)");

    QList<FileReference> result;
    result += collectMatches(line, qmlUrlRe, 1);
    if (result.isEmpty())
        result += collectMatches(line, objectWarningRe, 1);
    if (result.isEmpty())
        result += collectMatches(line, assertXRe, 1);
    if (result.isEmpty())
        result += collectMatches(line, assertRe, 1);
    return result;
}

} // namespace MultiFilterOutput

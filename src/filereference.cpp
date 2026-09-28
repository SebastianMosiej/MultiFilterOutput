#include "filereference.h"

#include <QRegularExpression>

#include <algorithm>

namespace MultiFilterOutput {

namespace {

// One way of writing a file location: `re` finds it; its captures `path`, `line` and (if any)
// `column` say where it points, and the link covers from `linkFrom`'s start to `linkTo`'s end.
struct Detector
{
    QRegularExpression re;
    int path;
    int line;
    int column; // -1: none
    int linkFrom;
    int linkTo;
};

// A path as compilers and test frameworks print it: optional drive letter, no spaces or
// colons, and a file extension -- without one, "12:00:01" or "localhost:8080" would count.
// Not right after a word character, dot, slash or colon, so a URL's host or a path's tail
// can't start one ("http://example.com:8080").
const char PathPattern[]
    = R"re((?<![\w./\\:-])((?:[A-Za-z]:)?[^\s:"'()<>\[\]]+\.[A-Za-z][A-Za-z0-9_+]*))re";

const QList<Detector> &detectors()
{
    // Most specific first: a later detector's match overlapping an earlier one's is dropped.
    static const QList<Detector> list = [] {
        const QString path = QString::fromLatin1(PathPattern);
        return QList<Detector>{
            // QML: qrc:/main.qml:12:5, file:///home/u/main.qml:3
            {QRegularExpression(R"re(((?:file|qrc):(?://)?/.+?):(\d+)(?::(\d+))?\b)re"), 1, 2, 3, 0, 0},
            // Qt: "QObject::connect: ... in C:/dir/main.cpp:42"
            {QRegularExpression(R"re(Object::.*in (.*):(\d+))re"), 1, 2, -1, 1, 2},
            // Q_ASSERT_X: ASSERT failure in where: "what", file a.cpp, line 3
            {QRegularExpression(R"re(ASSERT failure in .*: ".*", file (.+), line (\d+))re"), 1, 2, -1, 1, 2},
            // Q_ASSERT: ASSERT: "cond" in file a.cpp, line 3
            {QRegularExpression(R"re(ASSERT: .* in file (.+), line (\d+))re"), 1, 2, -1, 1, 2},
            // Qt Test: "   Loc: [C:\Program Files\x\tst_a.cpp(34)]" -- spaces allowed here.
            {QRegularExpression(R"re(Loc: \[((.+)\((\d+)\))\])re"), 2, 3, -1, 1, 1},
            // Python: File "/home/u/app.py", line 7
            {QRegularExpression(R"re(File "([^"]+)", line (\d+))re"), 1, 2, -1, 0, 0},
            // MSVC (and GTest on Windows): main.cpp(12) / main.cpp(12,5)
            {QRegularExpression(path + R"re(\((\d+)(?:,(\d+))?\))re"), 1, 2, 3, 1, 0},
            // GCC/Clang, GTest, sanitizers: main.cpp:12 / main.cpp:12:5
            {QRegularExpression(path + R"re(:(\d+)(?::(\d+))?)re"), 1, 2, 3, 1, 0},
        };
    }();
    return list;
}

bool overlaps(const FileReference &ref, const QList<FileReference> &accepted)
{
    return std::any_of(accepted.cbegin(), accepted.cend(), [&](const FileReference &other) {
        return ref.start < other.start + other.length && other.start < ref.start + ref.length;
    });
}

} // namespace

QList<FileReference> findFileReferences(const QString &line)
{
    QList<FileReference> result;
    for (const Detector &detector : detectors()) {
        QRegularExpressionMatchIterator it = detector.re.globalMatch(line);
        while (it.hasNext()) {
            const QRegularExpressionMatch m = it.next();
            FileReference ref;
            ref.start = int(m.capturedStart(detector.linkFrom));
            // The link ends where the last thing it covers does -- a column, if one was given.
            qsizetype end = m.capturedEnd(detector.linkTo);
            if (detector.linkTo != 0 && detector.column >= 0 && m.hasCaptured(detector.column))
                end = std::max(end, m.capturedEnd(detector.column));
            ref.length = int(end - ref.start);
            ref.target = line.mid(ref.start, ref.length);
            ref.path = m.captured(detector.path);
            ref.line = m.captured(detector.line).toInt();
            if (detector.column >= 0 && m.hasCaptured(detector.column))
                ref.column = m.captured(detector.column).toInt();
            if (!overlaps(ref, result))
                result.append(ref);
        }
    }
    std::sort(result.begin(), result.end(), [](const FileReference &a, const FileReference &b) {
        return a.start < b.start;
    });
    return result;
}

// Tab-separated: no path in output a link could come from holds one.
QString fileLinkHref(const FileReference &reference)
{
    return reference.path + u'\t' + QString::number(reference.line) + u'\t'
           + QString::number(reference.column);
}

std::optional<FileReference> fileReferenceFromHref(const QString &href)
{
    const QStringList parts = href.split(u'\t');
    if (parts.size() != 3 || parts.at(0).isEmpty())
        return std::nullopt;
    bool lineOk = false;
    bool columnOk = false;
    FileReference ref;
    ref.path = parts.at(0);
    ref.line = parts.at(1).toInt(&lineOk);
    ref.column = parts.at(2).toInt(&columnOk);
    if (!lineOk || !columnOk)
        return std::nullopt;
    return ref;
}

} // namespace MultiFilterOutput

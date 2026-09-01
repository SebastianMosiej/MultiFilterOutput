#include "multifilterlineparser.h"

#include "filereference.h"

#include <coreplugin/editormanager/editormanager.h>

#include <projectexplorer/project.h>

#include <utils/link.h>

#include <QRegularExpression>
#include <QUrl>

namespace MultiFilterOutput::Internal {

MultiFilterLineParser::MultiFilterLineParser(ProjectExplorer::Project *project)
{
    if (project) {
        m_finder.setProjectDirectory(project->projectDirectory());
        m_finder.setProjectFiles(project->files(ProjectExplorer::Project::SourceFiles));
    }
}

Utils::OutputLineParser::Result MultiFilterLineParser::handleLine(
    const QString &text, Utils::OutputFormat format)
{
    Q_UNUSED(format)
    const QList<FileReference> refs = findFileReferences(text);
    if (refs.isEmpty())
        return Status::NotHandled;

    QList<Utils::OutputLineParser::LinkSpec> links;
    for (const FileReference &ref : refs)
        links.append({ref.start, ref.length, ref.target});
    return Result(Status::Done, links);
}

bool MultiFilterLineParser::handleLink(const QString &href)
{
    // href is a FileReference::target. It comes in one of four shapes:
    //   "qrc:/qt/qml/QuickView.qml:67:13"  (qrc/file URL, line and column)
    //   "file:///home/user/main.qml:10"    (qrc/file URL, line only)
    //   "/src/model.cpp, line 42"          (ASSERT-style, no scheme)
    //   "C:/dir/main.cpp:42"               (Object::-style, no scheme; note the drive-letter
    //                                        colon on Windows, which must not be mistaken for
    //                                        the line-number separator)
    // Matching is done with anchored regular expressions rather than a naive split(':') so
    // that a Windows drive letter's colon is never confused with the line/column separator.
    static const QRegularExpression schemeLineColumn("^((?:file|qrc):(?://)?/.+?):(\\d+):(\\d+)$");
    static const QRegularExpression schemeLine("^((?:file|qrc):(?://)?/.+?):(\\d+)$");
    static const QRegularExpression pathCommaLine("^(.+), line (\\d+)$");
    static const QRegularExpression pathLine("^(.+):(\\d+)$");

    const auto openAt = [this](const QUrl &fileUrl, int line, int column) {
        const Utils::FilePath filePath = Utils::chooseFileFromList(m_finder.findFile(fileUrl));
        if (filePath.isEmpty())
            return false;
        Core::EditorManager::openEditorAt({filePath, line, column});
        return true;
    };

    QRegularExpressionMatch match = schemeLineColumn.match(href);
    if (match.hasMatch()) {
        return openAt(
            QUrl(match.captured(1)), match.captured(2).toInt(), match.captured(3).toInt() - 1);
    }

    match = schemeLine.match(href);
    if (match.hasMatch())
        return openAt(QUrl(match.captured(1)), match.captured(2).toInt(), -1);

    match = pathCommaLine.match(href);
    if (match.hasMatch()) {
        return openAt(QUrl::fromLocalFile(match.captured(1)), match.captured(2).toInt(), -1);
    }

    match = pathLine.match(href);
    if (match.hasMatch()) {
        return openAt(QUrl::fromLocalFile(match.captured(1)), match.captured(2).toInt(), -1);
    }

    return false;
}

} // namespace MultiFilterOutput::Internal

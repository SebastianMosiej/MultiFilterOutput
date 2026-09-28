#include "multifilterlineparser.h"

#include "filereference.h"

#include <coreplugin/editormanager/editormanager.h>

#include <projectexplorer/project.h>

#include <utils/link.h>

#include <QDir>
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

    // The link carries where it points already parsed (see fileLinkHref()), so handleLink()
    // needn't re-read the many ways it can be written -- nor mistake a Windows drive letter's
    // colon for a line separator.
    QList<Utils::OutputLineParser::LinkSpec> links;
    for (const FileReference &ref : refs)
        links.append({ref.start, ref.length, fileLinkHref(ref)});
    return Result(Status::Done, links);
}

bool MultiFilterLineParser::handleLink(const QString &href)
{
    const std::optional<FileReference> ref = fileReferenceFromHref(href);
    if (!ref)
        return false;
    // qrc: and file: URLs as they are; anything else is a local path, possibly relative or
    // with native separators -- FileInProjectFinder resolves it against the project.
    const bool isUrl = ref->path.startsWith("qrc:") || ref->path.startsWith("file:");
    const QUrl fileUrl = isUrl ? QUrl(ref->path)
                               : QUrl::fromLocalFile(QDir::fromNativeSeparators(ref->path));
    const Utils::FilePath filePath = Utils::chooseFileFromList(m_finder.findFile(fileUrl));
    if (filePath.isEmpty())
        return false;
    // Output counts columns from 1, the editor from 0.
    Core::EditorManager::openEditorAt(
        {filePath, ref->line, ref->column > 0 ? ref->column - 1 : -1});
    return true;
}

} // namespace MultiFilterOutput::Internal

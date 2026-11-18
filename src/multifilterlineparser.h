#pragma once

#include <utils/fileinprojectfinder.h>
#include <utils/outputformatter.h>

#include <QPointer>

namespace ProjectExplorer {
class Project;
}

namespace MultiFilterOutput::Internal {

// Turns MultiFilterOutput::FileReference matches into clickable links and resolves
// qrc:/file: targets (and the plain "path:line"/"path, line N" forms produced by the
// Object::/ASSERT patterns) via Utils::FileInProjectFinder, then opens them in an editor.
class MultiFilterLineParser final : public Utils::OutputLineParser
{
public:
    explicit MultiFilterLineParser(ProjectExplorer::Project *project);

    Utils::OutputLineParser::Result handleLine(
        const QString &text, Utils::OutputFormat format) override;
    bool handleLink(const QString &href) override;

private:
    Utils::FileInProjectFinder m_finder;
    // The project's file list is only needed when a link is clicked, and every tab gets a
    // parser of its own on each tab rebuild: fetched on the first handleLink(), not here.
    QPointer<ProjectExplorer::Project> m_project;
    bool m_projectFilesSet = false;
};

} // namespace MultiFilterOutput::Internal

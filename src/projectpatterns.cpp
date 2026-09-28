#include "projectpatterns.h"

#include "multifilteroutputtr.h"
#include "patternexchange.h"

#include <projectexplorer/project.h>
#include <projectexplorer/projectmanager.h>

#include <utils/filesystemwatcher.h>

namespace MultiFilterOutput {

namespace {

bool samePatterns(const QList<Pattern> &a, const QList<Pattern> &b)
{
    if (a.size() != b.size())
        return false;
    for (qsizetype i = 0; i < a.size(); ++i) {
        if (a.at(i).toMap() != b.at(i).toMap())
            return false;
    }
    return true;
}

} // namespace

ProjectPatterns::ProjectPatterns(QObject *parent)
    : QObject(parent)
    , m_watcher(new Utils::FileSystemWatcher(this))
{
    connect(m_watcher, &Utils::FileSystemWatcher::fileChanged, this, &ProjectPatterns::handleChange);
    connect(
        m_watcher, &Utils::FileSystemWatcher::directoryChanged, this, &ProjectPatterns::handleChange);
}

ProjectPatterns::~ProjectPatterns() = default;

Utils::FilePath ProjectPatterns::patternFile(const Utils::FilePath &projectDirectory)
{
    return projectDirectory.pathAppended(".qtcreator/multifilteroutput-patterns.json");
}

void ProjectPatterns::addProject(const Utils::FilePath &projectDirectory)
{
    Entry &entry = m_projects[projectDirectory];
    if (++entry.users > 1)
        return;
    watch(projectDirectory);
    reload(projectDirectory);
}

void ProjectPatterns::removeProject(const Utils::FilePath &projectDirectory)
{
    const auto it = m_projects.find(projectDirectory);
    if (it == m_projects.end() || --it->users > 0)
        return;
    const bool hadPatterns = !it->patterns.isEmpty();
    unwatch(projectDirectory);
    m_projects.erase(it);
    if (hadPatterns)
        emit patternsChanged(projectDirectory);
}

bool ProjectPatterns::hasProject(const Utils::FilePath &projectDirectory) const
{
    return m_projects.contains(projectDirectory);
}

QList<Pattern> ProjectPatterns::patterns(const Utils::FilePath &projectDirectory) const
{
    return m_projects.value(projectDirectory).patterns;
}

void ProjectPatterns::reload(const Utils::FilePath &projectDirectory)
{
    const auto it = m_projects.find(projectDirectory);
    if (it == m_projects.end())
        return;

    const Utils::FilePath file = patternFile(projectDirectory);
    QList<Pattern> patterns;
    QStringList problems;
    if (file.exists()) {
        const Utils::Result<QByteArray> contents = file.fileContents();
        if (!contents) {
            report(projectDirectory, {contents.error()});
            return;
        }
        // Seeded with the file's path: an entry without an id gets the same one every time.
        const ParsedPatterns parsed = parsePatterns(*contents, file.path());
        if (!parsed.error.isEmpty()) {
            report(projectDirectory, {parsed.error});
            return;
        }
        problems = parsed.warnings;
        patterns = parsed.patterns;
    }
    report(projectDirectory, problems);
    if (samePatterns(patterns, it->patterns))
        return;
    it->patterns = patterns;
    emit patternsChanged(projectDirectory);
}

Utils::Result<> ProjectPatterns::share(
    const Utils::FilePath &projectDirectory, const QList<Pattern> &patterns)
{
    const Utils::FilePath file = patternFile(projectDirectory);
    QList<Pattern> existing;
    if (file.exists()) {
        const Utils::Result<QByteArray> contents = file.fileContents();
        if (!contents)
            return Utils::ResultError(contents.error());
        const ParsedPatterns parsed = parsePatterns(*contents);
        if (!parsed.error.isEmpty()) {
            return Utils::ResultError(
                Tr::tr("\"%1\" could not be read, so it was left alone: %2")
                    .arg(file.toUserOutput(), parsed.error));
        }
        existing = parsed.patterns;
    }
    const QList<Pattern> updated
        = applyImport(existing, planImport(existing, patterns), ImportConflicts::Replace);
    if (const Utils::Result<> dir = file.parentDir().ensureWritableDir(); !dir)
        return dir;
    if (const Utils::Result<qint64> written = file.writeFileContents(exportPatterns(updated));
        !written) {
        return Utils::ResultError(written.error());
    }
    // Not waiting for the watcher: callers expect to see what they just shared. The file (and
    // .qtcreator) may be new, so watch what exists now.
    if (hasProject(projectDirectory)) {
        unwatch(projectDirectory);
        watch(projectDirectory);
    }
    reload(projectDirectory);
    return Utils::ResultOk;
}

void ProjectPatterns::report(const Utils::FilePath &projectDirectory, const QStringList &problems)
{
    const auto it = m_projects.find(projectDirectory);
    if (it == m_projects.end() || it->reportedProblems == problems)
        return;
    it->reportedProblems = problems;
    if (!problems.isEmpty())
        emit problemsFound(patternFile(projectDirectory), problems);
}

void ProjectPatterns::shutdown()
{
    m_projects.clear();
    delete m_watcher;
    m_watcher = nullptr;
}

// The file itself when it exists, and the directory it would appear in (or, before there is
// a .qtcreator, the project directory) -- so a file created, replaced or deleted is noticed
// too: editors and git often write a new file rather than change the old one.
void ProjectPatterns::watch(const Utils::FilePath &projectDirectory)
{
    if (!m_watcher) // after shutdown()
        return;
    const Utils::FilePath file = patternFile(projectDirectory);
    if (file.exists() && !m_watcher->watchesFile(file))
        m_watcher->addFile(file, Utils::FileSystemWatcher::WatchAllChanges);
    const Utils::FilePath dir = file.parentDir().exists() ? file.parentDir() : projectDirectory;
    if (!m_watcher->watchesDirectory(dir))
        m_watcher->addDirectory(dir, Utils::FileSystemWatcher::WatchAllChanges);
}

void ProjectPatterns::unwatch(const Utils::FilePath &projectDirectory)
{
    if (!m_watcher)
        return;
    const Utils::FilePath file = patternFile(projectDirectory);
    for (const Utils::FilePath &path : {file, file.parentDir(), projectDirectory}) {
        if (m_watcher->watchesFile(path))
            m_watcher->removeFile(path);
        if (m_watcher->watchesDirectory(path))
            m_watcher->removeDirectory(path);
    }
}

void ProjectPatterns::handleChange(const Utils::FilePath &path)
{
    for (auto it = m_projects.cbegin(); it != m_projects.cend(); ++it) {
        const Utils::FilePath projectDirectory = it.key();
        const Utils::FilePath file = patternFile(projectDirectory);
        if (path != file && path != file.parentDir() && path != projectDirectory)
            continue;
        // What exists may have changed (.qtcreator or the file appeared, or went away):
        // follow it.
        unwatch(projectDirectory);
        watch(projectDirectory);
        reload(projectDirectory);
        return; // reload() may emit, and a slot may add or remove projects: stop iterating
    }
}

ProjectPatterns &projectPatterns()
{
    static ProjectPatterns instance;
    return instance;
}

QList<Pattern> combinedPatterns(const QList<Pattern> &library, const QList<Pattern> &project)
{
    QList<Pattern> result = library;
    for (const Pattern &shared : project) {
        const auto same = std::find_if(result.begin(), result.end(), [&](const Pattern &p) {
            return p.id == shared.id;
        });
        if (same != result.end())
            *same = shared;
        else
            result.append(shared);
    }
    return result;
}

void attachProjectPatternsToProjectManager()
{
    using namespace ProjectExplorer;
    for (Project *project : ProjectManager::projects())
        projectPatterns().addProject(project->projectDirectory());
    QObject::connect(
        ProjectManager::instance(), &ProjectManager::projectAdded, &projectPatterns(), [](Project *p) {
            projectPatterns().addProject(p->projectDirectory());
        });
    QObject::connect(
        ProjectManager::instance(),
        &ProjectManager::projectRemoved,
        &projectPatterns(),
        [](Project *p) { projectPatterns().removeProject(p->projectDirectory()); });
}

} // namespace MultiFilterOutput

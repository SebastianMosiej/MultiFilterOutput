#pragma once

#include "multifilteroutput_global.h"
#include "pattern.h"

#include <utils/filepath.h>
#include <utils/result.h>

#include <QHash>
#include <QList>
#include <QObject>

namespace Utils {
class FileSystemWatcher;
}

namespace MultiFilterOutput {

// Patterns shared through a project rather than kept in one user's Pattern Library: the file
// <project directory>/.qtcreator/multifilteroutput-patterns.json, in the library's export format
// (see patternexchange.h), so a team can commit, review and version its filters. Read when the
// project opens and again whenever the file changes on disk (a pull, a checkout, an edit).
// Keyed by project directory; FilteredOutputPane and FilterSetAspect offer these patterns next
// to the library's (see combinedPatterns()).
class MULTIFILTEROUTPUT_EXPORT ProjectPatterns : public QObject
{
    Q_OBJECT

public:
    explicit ProjectPatterns(QObject *parent = nullptr);
    ~ProjectPatterns() override;

    static Utils::FilePath patternFile(const Utils::FilePath &projectDirectory);

    // Starts (or stops) following a project's file. Counted: a directory added twice (two
    // projects in one directory) needs removing twice.
    void addProject(const Utils::FilePath &projectDirectory);
    void removeProject(const Utils::FilePath &projectDirectory);
    bool hasProject(const Utils::FilePath &projectDirectory) const;

    // Empty for a directory not added, or without a file.
    QList<Pattern> patterns(const Utils::FilePath &projectDirectory) const;

    // Reads the file again -- which happens by itself when it changes on disk. A file that
    // can't be read keeps the patterns read last (a merge conflict mid-rebase shouldn't make a
    // team's filters vanish) and reports why.
    void reload(const Utils::FilePath &projectDirectory);

    // Adds these patterns to the project's file, replacing any with the same id, and creates
    // the file (and .qtcreator) if needed. Refuses to overwrite a file it can't read.
    Utils::Result<> share(const Utils::FilePath &projectDirectory, const QList<Pattern> &patterns);

signals:
    void patternsChanged(const Utils::FilePath &projectDirectory);
    void problemsFound(const Utils::FilePath &patternFile, const QStringList &problems);

private:
    struct Entry
    {
        int users = 0;
        QList<Pattern> patterns;
    };
    void watch(const Utils::FilePath &projectDirectory);
    void unwatch(const Utils::FilePath &projectDirectory);
    void handleChange(const Utils::FilePath &path);

    QHash<Utils::FilePath, Entry> m_projects;
    Utils::FileSystemWatcher *m_watcher = nullptr;
};

MULTIFILTEROUTPUT_EXPORT ProjectPatterns &projectPatterns();

// What a project's run configurations can choose from: the library's patterns, with a project
// pattern taking the place of a library one of the same id (the shared definition wins), then
// the project's other patterns in file order.
MULTIFILTEROUTPUT_EXPORT QList<Pattern> combinedPatterns(
    const QList<Pattern> &library, const QList<Pattern> &project);

// Follows ProjectExplorer's open projects (plugin start-up only; not in unit tests).
void attachProjectPatternsToProjectManager();

} // namespace MultiFilterOutput

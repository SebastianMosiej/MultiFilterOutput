#pragma once

#include "linebuffer.h"
#include "multifilteroutput_global.h"

#include <utils/outputformat.h>

#include <QHash>
#include <QObject>

namespace ProjectExplorer {
class RunControl;
}

namespace MultiFilterOutput {

class MULTIFILTEROUTPUT_EXPORT RunTracker : public QObject
{
    Q_OBJECT

public:
    explicit RunTracker(QObject *parent = nullptr);

    // The RunControl currently reporting isOutputVisible() == true, or nullptr.
    ProjectExplorer::RunControl *activeRunControl() const;

signals:
    // Emitted whenever activeRunControl() changes (including to/from nullptr).
    void activeRunChanged(ProjectExplorer::RunControl *runControl);
    // Emitted for each complete line from the currently-active run only.
    void activeRunLineReceived(const QString &line, Utils::OutputFormat format);

private:
    void trackRunControl(ProjectExplorer::RunControl *runControl);
    void untrackRunControl(ProjectExplorer::RunControl *runControl);
    void updateActiveRunControl();

    QHash<ProjectExplorer::RunControl *, LineBuffer> m_buffers;
    ProjectExplorer::RunControl *m_active = nullptr;
};

} // namespace MultiFilterOutput

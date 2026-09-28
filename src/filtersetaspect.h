#pragma once

#include "multifilteroutput_global.h"
#include "pattern.h"

#include <utils/filepath.h>

#include <utils/aspects.h>

namespace ProjectExplorer {
class BuildConfiguration;
}

namespace MultiFilterOutput {

class MULTIFILTEROUTPUT_EXPORT FilterSetAspect final : public Utils::BaseAspect
{
    Q_OBJECT

public:
    explicit FilterSetAspect(ProjectExplorer::BuildConfiguration *bc);

    QStringList enabledPatternIds() const;
    void setEnabledPatternIds(const QStringList &ids);
    // Enables or disables one pattern, keeping the rest of the selection -- what toggling it
    // from the MultiFilterOutput pane does to the run configuration. Like
    // setEnabledPatternIds(), emits changed() when the selection actually changes, so an open
    // Run Settings page shows it.
    void setPatternEnabled(const QString &id, bool enabled);
    // The same for several patterns at once (one changed() at most).
    void setPatternsEnabled(const QStringList &ids, bool enabled);
    // ... and for every pattern in the library tagged `tag` (see Pattern::tags).
    void setGroupEnabled(const QString &tag, bool enabled);

    // The project whose shared patterns (see ProjectPatterns) are offered next to the
    // library's -- the run configuration's project; set from the constructor.
    void setProjectDirectory(const Utils::FilePath &projectDirectory);
    // What this run configuration can choose from: see combinedPatterns().
    QList<Pattern> availablePatterns() const;

    QVariant variantValue() const override;

    void fromMap(const Utils::Store &map) override;
    void toMap(Utils::Store &map) const override;

private:
    QStringList m_enabledPatternIds;
    Utils::FilePath m_projectDirectory;
};

} // namespace MultiFilterOutput

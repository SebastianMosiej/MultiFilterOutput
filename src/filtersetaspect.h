#pragma once

#include "multifilteroutput_global.h"

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

    QVariant variantValue() const override;

    void fromMap(const Utils::Store &map) override;
    void toMap(Utils::Store &map) const override;

private:
    QStringList m_enabledPatternIds;
};

} // namespace MultiFilterOutput

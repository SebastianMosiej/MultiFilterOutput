#include "filtersetaspect.h"

#include "multifilteroutputconstants.h"
#include "multifilteroutputtr.h"
#include "pattern.h"
#include "patternlibrary.h"

#include <projectexplorer/buildconfiguration.h>

#include <utils/layoutbuilder.h>

#include <QCheckBox>
#include <QVBoxLayout>
#include <QWidget>

namespace MultiFilterOutput {

FilterSetAspect::FilterSetAspect(ProjectExplorer::BuildConfiguration *bc)
    : Utils::BaseAspect(bc ? bc : nullptr)
{
    setId(Constants::FILTER_SET_ASPECT_ID);
    setDisplayName(Tr::tr("Output Filters"));
    setSettingsKey(Constants::FILTER_SET_ASPECT_ID);
}

QStringList FilterSetAspect::enabledPatternIds() const
{
    return m_enabledPatternIds;
}

void FilterSetAspect::setEnabledPatternIds(const QStringList &ids)
{
    m_enabledPatternIds = ids;
}

QVariant FilterSetAspect::variantValue() const
{
    return m_enabledPatternIds;
}

void FilterSetAspect::fromMap(const Utils::Store &map)
{
    m_enabledPatternIds = map.value(Constants::FILTER_SET_ASPECT_ID).toStringList();
}

void FilterSetAspect::toMap(Utils::Store &map) const
{
    map.insert(Constants::FILTER_SET_ASPECT_ID, m_enabledPatternIds);
}

void FilterSetAspect::addToLayoutImpl(Layouting::Layout &parent)
{
    auto *container = new QWidget;
    auto *layout = new QVBoxLayout(container);
    for (const Pattern &pattern : patternLibrary().patterns()) {
        auto *checkBox = new QCheckBox(pattern.name, container);
        checkBox->setChecked(m_enabledPatternIds.contains(pattern.id));
        connect(checkBox, &QCheckBox::toggled, this, [this, id = pattern.id](bool checked) {
            if (checked && !m_enabledPatternIds.contains(id))
                m_enabledPatternIds.append(id);
            else if (!checked)
                m_enabledPatternIds.removeAll(id);
        });
        layout->addWidget(checkBox);
    }
    parent.addItem(container);
}

} // namespace MultiFilterOutput

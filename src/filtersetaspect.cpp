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

FilterSetAspect::FilterSetAspect(ProjectExplorer::BuildConfiguration *)
{
    // Deliberately do NOT pass the BuildConfiguration to Utils::BaseAspect(): that would
    // self-register this aspect into the BuildConfiguration's own (non-owning) aspect
    // list, in addition to the RunConfiguration's list that
    // RunConfiguration::registerAspect()'s factory machinery already registers us into
    // with ownership. The BuildConfiguration then outlives the RunConfiguration/aspect,
    // leaving a dangling entry that crashes the next time it serializes its aspects (e.g.
    // ProjectExplorer::Project::saveSettings(), triggered by switching modes). See
    // ProjectExplorer::CustomParsersAspect for the same reference pattern upstream.
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

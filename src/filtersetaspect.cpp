#include "filtersetaspect.h"

#include "multifilteroutputconstants.h"
#include "multifilteroutputtr.h"
#include "pattern.h"
#include "patternlibrary.h"

#include <projectexplorer/buildconfiguration.h>

#include <utils/detailswidget.h>

#include <QCheckBox>
#include <QPointer>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>

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

    // Seeds a brand-new run configuration with every pattern marked Pattern::isDefault.
    // RunConfigurationFactory::create() (used for genuinely new run configurations) never
    // calls fromMap() on its aspects -- only RunConfigurationFactory::restore() (loading an
    // already-saved one) does -- so this constructor-time seed only ever affects a run
    // configuration that has never been saved before; fromMap() unconditionally overwrites it
    // for anything restored from disk.
    for (const Pattern &pattern : patternLibrary().patterns()) {
        if (pattern.isDefault)
            m_enabledPatternIds.append(pattern.id);
    }

    // Forget patterns as they are deleted, so their ids don't linger in this run
    // configuration's selection -- and on disk -- forever. Only actual deletions count (see
    // PatternLibrary::patternsRemoved()); an id the library merely doesn't know is kept.
    // patternsRemoved() precedes the patternsChanged() for the same change, so the config
    // widget's checkbox rebuild below already sees the pruned selection.
    connect(&patternLibrary(), &PatternLibrary::patternsRemoved, this, [this](const QStringList &ids) {
        for (const QString &id : ids)
            m_enabledPatternIds.removeAll(id);
    });

    // The Run Settings page renders each RunConfiguration aspect via
    // createConfigWidget(), which only returns non-null once a creator is set here --
    // addToLayoutImpl() is for the generic AspectContainer::layouter() path (e.g. Options
    // pages), which the Run Settings page does not use.
    setConfigWidgetCreator([this] {
        // Collapsed by default, like the other Details sections on this page (e.g. Custom
        // Output Parsers): the checkbox list only needs to be visible while actively
        // choosing filters, and the summary line covers the common "what's active" check.
        auto *details = new Utils::DetailsWidget;
        details->setState(Utils::DetailsWidget::Collapsed);

        auto *container = new QWidget;
        new QVBoxLayout(container);
        details->setWidget(container);

        auto updateSummary = [this, guard = QPointer<Utils::DetailsWidget>(details)] {
            if (!guard)
                return;
            const QList<Pattern> patterns = patternLibrary().patterns();
            const int active = std::count_if(
                patterns.cbegin(), patterns.cend(), [this](const Pattern &p) {
                    return m_enabledPatternIds.contains(p.id);
                });
            guard->setSummaryText(
                patterns.isEmpty() ? Tr::tr("No patterns defined")
                                    : Tr::tr("%1 of %2 filters active")
                                          .arg(active)
                                          .arg(patterns.size()));
        };

        // Rebuilds the checkbox list from the current library contents. Needed because
        // createConfigWidget() is called (and its result cached) once per RunConfiguration,
        // so without this the list would freeze at whatever the library held the first time
        // this run configuration's settings page was opened -- e.g. staying empty forever
        // if it was viewed before any pattern existed. Guarded by a QPointer since the
        // container can be destroyed (e.g. switching run configurations) before this aspect
        // is; connect() uses `this` as context so the reverse teardown order disconnects
        // cleanly too.
        auto rebuild = [this, guard = QPointer<QWidget>(container), updateSummary] {
            if (!guard)
                return;
            auto *layout = static_cast<QVBoxLayout *>(guard->layout());
            while (QLayoutItem *child = layout->takeAt(0)) {
                delete child->widget();
                delete child;
            }
            for (const Pattern &pattern : patternLibrary().patterns()) {
                auto *checkBox = new QCheckBox(pattern.name);
                checkBox->setChecked(m_enabledPatternIds.contains(pattern.id));
                connect(
                    checkBox,
                    &QCheckBox::toggled,
                    this,
                    [this, id = pattern.id, updateSummary](bool checked) {
                        if (checked && !m_enabledPatternIds.contains(id))
                            m_enabledPatternIds.append(id);
                        else if (!checked)
                            m_enabledPatternIds.removeAll(id);
                        updateSummary();
                    });
                layout->addWidget(checkBox);
            }
            updateSummary();
        };
        rebuild();
        connect(&patternLibrary(), &PatternLibrary::patternsChanged, this, rebuild);
        // ...and when the selection is changed from outside these checkboxes (from the
        // MultiFilterOutput pane, see setPatternEnabled()). The checkboxes' own toggles edit
        // m_enabledPatternIds directly, without changed(), so a checkbox is never deleted from
        // inside its own signal.
        connect(this, &Utils::BaseAspect::changed, this, rebuild);

        return details;
    });
}

QStringList FilterSetAspect::enabledPatternIds() const
{
    return m_enabledPatternIds;
}

void FilterSetAspect::setEnabledPatternIds(const QStringList &ids)
{
    if (ids == m_enabledPatternIds)
        return;
    m_enabledPatternIds = ids;
    emit changed();
}

void FilterSetAspect::setPatternEnabled(const QString &id, bool enabled)
{
    setPatternsEnabled({id}, enabled);
}

void FilterSetAspect::setPatternsEnabled(const QStringList &ids, bool enabled)
{
    QStringList enabledIds = m_enabledPatternIds;
    for (const QString &id : ids) {
        if (enabled && !enabledIds.contains(id))
            enabledIds.append(id);
        else if (!enabled)
            enabledIds.removeAll(id);
    }
    setEnabledPatternIds(enabledIds);
}

void FilterSetAspect::setGroupEnabled(const QString &tag, bool enabled)
{
    setPatternsEnabled(patternIdsWithTag(patternLibrary().patterns(), tag), enabled);
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

} // namespace MultiFilterOutput

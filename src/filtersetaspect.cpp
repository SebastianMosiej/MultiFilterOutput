#include "filtersetaspect.h"

#include "multifilteroutputconstants.h"
#include "multifilteroutputtr.h"
#include "pattern.h"
#include "patternlibrary.h"
#include "projectpatterns.h"

#include <projectexplorer/buildconfiguration.h>
#include <projectexplorer/project.h>

#include <utils/detailswidget.h>

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPointer>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>

namespace MultiFilterOutput {

FilterSetAspect::FilterSetAspect(ProjectExplorer::BuildConfiguration *bc)
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
    // for anything restored from disk. The project's shared patterns count too.
    if (bc && bc->project())
        m_projectDirectory = bc->project()->projectDirectory();
    for (const Pattern &pattern : availablePatterns()) {
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

        // The overrides sit above the pattern list, outside what rebuild() below replaces.
        auto *outer = new QWidget;
        auto *outerLayout = new QVBoxLayout(outer);
        outerLayout->setContentsMargins(0, 0, 0, 0);
        auto *overrides = new QFormLayout;
        const auto addOverride = [this, overrides](
                                     const QString &label,
                                     const QString &tooltip,
                                     const QString &onText,
                                     const QString &offText,
                                     Utils::TriState (FilterSetAspect::*get)() const,
                                     void (FilterSetAspect::*set)(Utils::TriState)) {
            // Item order follows Utils::TriState's values: Enabled, Disabled, Default.
            auto *combo = new QComboBox;
            combo->addItems({onText, offText, Tr::tr("Default (as in Preferences)")});
            combo->setToolTip(tooltip);
            combo->setCurrentIndex((this->*get)().toInt());
            connect(combo, &QComboBox::activated, this, [this, set](int index) {
                (this->*set)(Utils::TriState::fromInt(index));
            });
            connect(this, &Utils::BaseAspect::changed, combo, [this, combo, get] {
                const QSignalBlocker blocker(combo);
                combo->setCurrentIndex((this->*get)().toInt());
            });
            overrides->addRow(label, combo);
        };
        addOverride(
            Tr::tr("Show the pane on run start:"),
            Tr::tr("Whether the MultiFilterOutput pane pops up when a run of this run "
                   "configuration first prints something (Preferences > MultiFilterOutput)."),
            Tr::tr("Yes"),
            Tr::tr("No"),
            &FilterSetAspect::focusOnRunStart,
            &FilterSetAspect::setFocusOnRunStart);
        addOverride(
            Tr::tr("Unmatched tab:"),
            Tr::tr("Whether runs of this run configuration get the Unmatched tab "
                   "(Preferences > MultiFilterOutput)."),
            Tr::tr("Show"),
            Tr::tr("Hide"),
            &FilterSetAspect::showUnmatchedTab,
            &FilterSetAspect::setShowUnmatchedTab);
        outerLayout->addLayout(overrides);

        auto *container = new QWidget;
        new QVBoxLayout(container);
        outerLayout->addWidget(container);
        details->setWidget(outer);

        auto updateSummary = [this, guard = QPointer<Utils::DetailsWidget>(details)] {
            if (!guard)
                return;
            const QList<Pattern> patterns = availablePatterns();
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
            const QList<Pattern> patterns = availablePatterns();
            const QList<Pattern> shared = projectPatterns().patterns(m_projectDirectory);

            // One tri-state checkbox per group (see Pattern::tags): checked when all of its
            // patterns are active, partially when some are. A click activates the whole group
            // -- or deactivates it when all were active.
            QList<QPair<QString, QPointer<QCheckBox>>> groupBoxes;
            if (const QStringList tags = patternTags(patterns); !tags.isEmpty()) {
                auto *groupsRow = new QWidget;
                auto *groupsLayout = new QHBoxLayout(groupsRow);
                groupsLayout->setContentsMargins(0, 0, 0, 0);
                groupsLayout->addWidget(new QLabel(Tr::tr("Groups:")));
                for (const QString &tag : tags) {
                    const int size = int(patternIdsWithTag(patterns, tag).size());
                    auto *groupBox = new QCheckBox(Tr::tr("%1 (%2)").arg(tag).arg(size));
                    groupBox->setToolTip(Tr::tr("Activate or deactivate every pattern tagged "
                                                "\"%1\".")
                                             .arg(tag));
                    groupsLayout->addWidget(groupBox);
                    groupBoxes.append({tag, groupBox});
                }
                groupsLayout->addStretch();
                layout->addWidget(groupsRow);
            }
            const auto updateGroups = [this, groupBoxes] {
                const QList<Pattern> patterns = availablePatterns();
                for (const auto &[tag, groupBox] : groupBoxes) {
                    if (!groupBox)
                        continue;
                    const QStringList ids = patternIdsWithTag(patterns, tag);
                    const auto active = std::count_if(ids.cbegin(), ids.cend(), [this](const QString &id) {
                        return m_enabledPatternIds.contains(id);
                    });
                    const QSignalBlocker blocker(groupBox);
                    groupBox->setCheckState(
                        active == 0             ? Qt::Unchecked
                        : active == ids.size() ? Qt::Checked
                                                : Qt::PartiallyChecked);
                }
            };
            for (const auto &[tag, groupBox] : groupBoxes) {
                connect(groupBox, &QCheckBox::clicked, this, [this, tag, groupBox] {
                    // The click has already moved the box on; what it showed before decides.
                    // The changed() that setGroupEnabled() emits rebuilds this list -- deleting
                    // the box -- so not from inside its own signal.
                    const bool enable = groupBox->checkState() != Qt::Unchecked;
                    QMetaObject::invokeMethod(
                        this, [this, tag, enable] { setGroupEnabled(tag, enable); },
                        Qt::QueuedConnection);
                });
            }

            for (const Pattern &pattern : patterns) {
                const bool isShared = std::any_of(
                    shared.cbegin(), shared.cend(), [&](const Pattern &p) { return p.id == pattern.id; });
                auto *checkBox = new QCheckBox(
                    isShared ? Tr::tr("%1 (project)").arg(pattern.name) : pattern.name);
                if (isShared) {
                    checkBox->setToolTip(Tr::tr("Shared through the project, in %1.")
                                             .arg(ProjectPatterns::patternFile(m_projectDirectory)
                                                      .toUserOutput()));
                }
                checkBox->setChecked(m_enabledPatternIds.contains(pattern.id));
                connect(
                    checkBox,
                    &QCheckBox::toggled,
                    this,
                    [this, id = pattern.id, updateSummary, updateGroups](bool checked) {
                        if (checked && !m_enabledPatternIds.contains(id))
                            m_enabledPatternIds.append(id);
                        else if (!checked)
                            m_enabledPatternIds.removeAll(id);
                        updateSummary();
                        updateGroups();
                    });
                layout->addWidget(checkBox);
            }
            updateSummary();
            updateGroups();
        };
        rebuild();
        connect(&patternLibrary(), &PatternLibrary::patternsChanged, this, rebuild);
        connect(
            &projectPatterns(),
            &ProjectPatterns::patternsChanged,
            this,
            [this, rebuild](const Utils::FilePath &projectDirectory) {
                if (projectDirectory == m_projectDirectory)
                    rebuild();
            });
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
    setPatternsEnabled(patternIdsWithTag(availablePatterns(), tag), enabled);
}

Utils::TriState FilterSetAspect::focusOnRunStart() const
{
    return m_focusOnRunStart;
}

void FilterSetAspect::setFocusOnRunStart(Utils::TriState value)
{
    if (value == m_focusOnRunStart)
        return;
    m_focusOnRunStart = value;
    emit changed();
}

Utils::TriState FilterSetAspect::showUnmatchedTab() const
{
    return m_showUnmatchedTab;
}

void FilterSetAspect::setShowUnmatchedTab(Utils::TriState value)
{
    if (value == m_showUnmatchedTab)
        return;
    m_showUnmatchedTab = value;
    emit changed();
}

RunFilterSettings runFilterSettings(const QVariant &aspectValue)
{
    // Before the overrides, the value was just the id list.
    if (aspectValue.typeId() != QMetaType::QVariantMap)
        return {aspectValue.toStringList()};
    const QVariantMap map = aspectValue.toMap();
    return {
        map.value("patterns").toStringList(),
        Utils::TriState::fromVariant(map.value("focusOnRunStart", Utils::TriState::Default.toVariant())),
        Utils::TriState::fromVariant(map.value("showUnmatchedTab", Utils::TriState::Default.toVariant())),
    };
}

bool resolvedSetting(Utils::TriState override, bool global)
{
    if (override == Utils::TriState::Enabled)
        return true;
    if (override == Utils::TriState::Disabled)
        return false;
    return global;
}

void FilterSetAspect::setProjectDirectory(const Utils::FilePath &projectDirectory)
{
    m_projectDirectory = projectDirectory;
}

QList<Pattern> FilterSetAspect::availablePatterns() const
{
    return combinedPatterns(
        patternLibrary().patterns(), projectPatterns().patterns(m_projectDirectory));
}

QVariant FilterSetAspect::variantValue() const
{
    return QVariantMap{
        {"patterns", m_enabledPatternIds},
        {"focusOnRunStart", m_focusOnRunStart.toVariant()},
        {"showUnmatchedTab", m_showUnmatchedTab.toVariant()},
    };
}

void FilterSetAspect::fromMap(const Utils::Store &map)
{
    m_enabledPatternIds = map.value(Constants::FILTER_SET_ASPECT_ID).toStringList();
    m_focusOnRunStart = Utils::TriState::fromVariant(
        map.value(Constants::RUN_FOCUS_ON_RUN_START_KEY, Utils::TriState::Default.toVariant()));
    m_showUnmatchedTab = Utils::TriState::fromVariant(
        map.value(Constants::RUN_SHOW_UNMATCHED_TAB_KEY, Utils::TriState::Default.toVariant()));
}

void FilterSetAspect::toMap(Utils::Store &map) const
{
    map.insert(Constants::FILTER_SET_ASPECT_ID, m_enabledPatternIds);
    map.insert(Constants::RUN_FOCUS_ON_RUN_START_KEY, m_focusOnRunStart.toVariant());
    map.insert(Constants::RUN_SHOW_UNMATCHED_TAB_KEY, m_showUnmatchedTab.toVariant());
}

} // namespace MultiFilterOutput

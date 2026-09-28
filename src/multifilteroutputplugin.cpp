#include "multifilteroutputplugin.h"

#include "filteredoutputpane.h"
#include "filtersetaspect.h"
#include "multifilteroutputconstants.h"
#include "multifilteroutputtr.h"
#include "outputpanesettings.h"
#include "patternlibrary.h"
#include "patternlibrarysettingspage.h"
#include "projectpatterns.h"
#include "starterpatterns.h"
#include "runtracker.h"

#include <coreplugin/dialogs/ioptionspage.h>
#include <coreplugin/icore.h>
#include <coreplugin/messagemanager.h>

#include <projectexplorer/runconfiguration.h>

#include <utils/infobar.h>

namespace MultiFilterOutput::Internal {

MultiFilterOutputPlugin::~MultiFilterOutputPlugin()
{
    delete m_outputPane;
    delete m_runTracker;
    delete m_settingsPage;
    // Its file watcher must go while Qt is still up; the store itself is a function-local
    // static, destroyed only after QApplication.
    projectPatterns().shutdown();
}

void MultiFilterOutputPlugin::initialize()
{
    Core::IOptionsPage::registerCategory(
        Constants::OPTIONS_CATEGORY_ID,
        Tr::tr("MultiFilterOutput"),
        ":/multifilteroutput/images/MultiFilterOutput_24.png");

    m_settingsPage = new PatternLibrarySettingsPage;
    // General Messages rather than a debug log: these need the user to look at the Pattern
    // Library, and the log is not somewhere they would ever see them.
    for (const QString &problem : patternLibrary().load(Core::ICore::settings()))
        Core::MessageManager::writeSilently(Tr::tr("MultiFilterOutput: %1").arg(problem));
    outputPaneSettings().load();
    offerStarterPatterns();

    ProjectExplorer::RunConfiguration::registerAspect<FilterSetAspect>();

    connect(
        &projectPatterns(),
        &ProjectPatterns::problemsFound,
        this,
        [](const Utils::FilePath &file, const QStringList &problems) {
            for (const QString &problem : problems) {
                Core::MessageManager::writeSilently(
                    Tr::tr("MultiFilterOutput: %1: %2").arg(file.toUserOutput(), problem));
            }
        });
    attachProjectPatternsToProjectManager();

    m_runTracker = new RunTracker;
    m_runTracker->attachToProjectExplorer();
    m_outputPane = new FilteredOutputPane(m_runTracker);
}

// A fresh install has an empty Pattern Library and so nothing to filter by: offer the starter
// set once, in Qt Creator's info bar -- not forced on anyone, and never again once added or
// dismissed for good.
void MultiFilterOutputPlugin::offerStarterPatterns()
{
    const Utils::Id id("MultiFilterOutput.StarterPatterns");
    Utils::InfoBar *infoBar = Core::ICore::infoBar();
    if (!patternLibrary().patterns().isEmpty() || !infoBar->canInfoBeAdded(id))
        return;
    Utils::InfoBarEntry info(
        id,
        Tr::tr("MultiFilterOutput's Pattern Library is empty. Add starter patterns for errors, "
               "warnings, QML messages, assertions and Qt Test failures? Turn them on for a "
               "run configuration in Run Settings > Output Filters (group \"starter\")."),
        Utils::InfoBarEntry::GlobalSuppression::Enabled);
    info.addCustomButton(
        Tr::tr("Add Starter Patterns"),
        [] { patternLibrary().setPatterns(withStarterPatterns(patternLibrary().patterns())); },
        {},
        Utils::InfoBarEntry::ButtonAction::SuppressPersistently);
    infoBar->addInfo(info);
}

ExtensionSystem::IPlugin::ShutdownFlag MultiFilterOutputPlugin::aboutToShutdown()
{
    // Nothing to save: PatternLibrary writes every change -- and load()'s own repairs and
    // migrations -- as it happens. An unconditional save here used to stamp the current
    // settings format over one written by a newer plugin version on every shutdown.
    return SynchronousShutdown;
}

} // namespace MultiFilterOutput::Internal

#include "multifilteroutputplugin.moc"

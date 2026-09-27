#include "multifilteroutputplugin.h"

#include "filteredoutputpane.h"
#include "filtersetaspect.h"
#include "multifilteroutputconstants.h"
#include "multifilteroutputtr.h"
#include "outputpanesettings.h"
#include "patternlibrary.h"
#include "patternlibrarysettingspage.h"
#include "runtracker.h"

#include <coreplugin/dialogs/ioptionspage.h>
#include <coreplugin/icore.h>
#include <coreplugin/messagemanager.h>

#include <projectexplorer/runconfiguration.h>

namespace MultiFilterOutput::Internal {

MultiFilterOutputPlugin::~MultiFilterOutputPlugin()
{
    delete m_outputPane;
    delete m_runTracker;
    delete m_settingsPage;
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

    ProjectExplorer::RunConfiguration::registerAspect<FilterSetAspect>();

    m_runTracker = new RunTracker;
    m_runTracker->attachToProjectExplorer();
    m_outputPane = new FilteredOutputPane(m_runTracker);
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

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
    patternLibrary().load(Core::ICore::settings());
    outputPaneSettings().load();

    ProjectExplorer::RunConfiguration::registerAspect<FilterSetAspect>();

    m_runTracker = new RunTracker;
    m_outputPane = new FilteredOutputPane(m_runTracker);
}

ExtensionSystem::IPlugin::ShutdownFlag MultiFilterOutputPlugin::aboutToShutdown()
{
    // Every change made through PatternLibrary's mutators is already on disk; this is for the
    // repairs load() itself may have made (see validatePatterns()), which aren't a mutation.
    patternLibrary().save();
    return SynchronousShutdown;
}

} // namespace MultiFilterOutput::Internal

#include "multifilteroutputplugin.moc"

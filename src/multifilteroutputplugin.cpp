#include "multifilteroutputplugin.h"

#include "filteredoutputpane.h"
#include "filtersetaspect.h"
#include "multifilteroutputconstants.h"
#include "multifilteroutputtr.h"
#include "patternlibrary.h"
#include "patternlibrarysettingspage.h"
#include "runtracker.h"

#include <coreplugin/dialogs/ioptionspage.h>

#include <projectexplorer/runconfiguration.h>

namespace MultiFilterOutput::Internal {

MultiFilterOutputPlugin::~MultiFilterOutputPlugin() = default;

void MultiFilterOutputPlugin::initialize()
{
    Core::IOptionsPage::registerCategory(
        Constants::OPTIONS_CATEGORY_ID, Tr::tr("MultiFilterOutput"), {});

    static PatternLibrarySettingsPage settingsPage;
    patternLibrary().load();

    ProjectExplorer::RunConfiguration::registerAspect<FilterSetAspect>();

    static RunTracker runTracker;
    static FilteredOutputPane outputPane(&runTracker);
}

ExtensionSystem::IPlugin::ShutdownFlag MultiFilterOutputPlugin::aboutToShutdown()
{
    patternLibrary().save();
    return SynchronousShutdown;
}

} // namespace MultiFilterOutput::Internal

#include "multifilteroutputplugin.moc"

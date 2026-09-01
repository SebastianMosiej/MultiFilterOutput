#include "multifilteroutputplugin.h"

namespace MultiFilterOutput::Internal {

MultiFilterOutputPlugin::~MultiFilterOutputPlugin() = default;

void MultiFilterOutputPlugin::initialize()
{
    // Later tasks register the Pattern Library options page, FilterSetAspect,
    // RunTracker and FilteredOutputPane here.
}

ExtensionSystem::IPlugin::ShutdownFlag MultiFilterOutputPlugin::aboutToShutdown()
{
    return SynchronousShutdown;
}

} // namespace MultiFilterOutput::Internal

#include "multifilteroutputplugin.moc"

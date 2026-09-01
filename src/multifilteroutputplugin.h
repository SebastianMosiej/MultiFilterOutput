#pragma once

#include <extensionsystem/iplugin.h>

namespace MultiFilterOutput {
class RunTracker;
}

namespace MultiFilterOutput::Internal {

class PatternLibrarySettingsPage;
class FilteredOutputPane;

class MultiFilterOutputPlugin final : public ExtensionSystem::IPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QtCreatorPlugin" FILE "MultiFilterOutput.json")

public:
    MultiFilterOutputPlugin() = default;
    ~MultiFilterOutputPlugin() final;

    void initialize() final;
    ShutdownFlag aboutToShutdown() final;

private:
    // Owned here -- NOT function-local statics -- so they are destroyed from
    // ~MultiFilterOutputPlugin(), which ExtensionSystem::PluginManager calls at a
    // controlled point in its dependency-ordered shutdown, while Core/ProjectExplorer are
    // still loaded. A function-local static instead gets destroyed by the C++ runtime at
    // DLL-unload/process-exit time, whose ordering relative to other plugins' teardown is
    // not guaranteed -- IOutputPane and IOptionsPage both deregister from a live manager
    // object in Core.dll on destruction, which crashed (access violation in
    // IOutputPane::~IOutputPane(), Core.dll) when that manager was already gone.
    PatternLibrarySettingsPage *m_settingsPage = nullptr;
    RunTracker *m_runTracker = nullptr;
    FilteredOutputPane *m_outputPane = nullptr;
};

} // namespace MultiFilterOutput::Internal

#pragma once

#include <extensionsystem/iplugin.h>

namespace MultiFilterOutput::Internal {

class MultiFilterOutputPlugin final : public ExtensionSystem::IPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QtCreatorPlugin" FILE "MultiFilterOutput.json")

public:
    MultiFilterOutputPlugin() = default;
    ~MultiFilterOutputPlugin() final;

    void initialize() final;
    ShutdownFlag aboutToShutdown() final;
};

} // namespace MultiFilterOutput::Internal

#pragma once

#include <vector>
#include <memory>
#include "RprHybrid/Base/Library.h"
#include "Renderer/FrRenderer.h"

struct Plugin
{
    PFNCREATERENDERERINSTANCE CreateRendererInstance;
    PFNDELETERENDERERINSTANCE DeleteRendererInstance;
};

class PluginManager
{
public:
    PluginManager();

    ~PluginManager();

    int RegisterPlugin(const char* path);

    //void EnumeratePlugins(const std::string& path);

    int GetPluginCount() const;

    bool GetPlugin(int index, Plugin* out_plugin);

private:
    std::vector<std::tuple<std::shared_ptr<Library>, Plugin>> m_plugins;
};
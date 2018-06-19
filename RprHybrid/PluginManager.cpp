#ifdef WIN32
#define NOMINMAX
#include <Windows.h>
#include <tchar.h>
#include <strsafe.h>
#else
// Linux
#endif
#include "PluginManager.h"

PluginManager::PluginManager()
{

}

PluginManager::~PluginManager()
{
    m_plugins.clear();
}

int PluginManager::RegisterPlugin(const char* path)
{
    std::shared_ptr<Library> library(new Library());
    if (library->LoadFile(path))
    {
        Plugin plugin;
        plugin.CreateRendererInstance = reinterpret_cast<PFNCREATERENDERERINSTANCE>(library->GetEntryPoint("CreateRendererInstance"));
        plugin.DeleteRendererInstance = reinterpret_cast<PFNDELETERENDERERINSTANCE>(library->GetEntryPoint("DeleteRendererInstance"));

        if (plugin.CreateRendererInstance && plugin.DeleteRendererInstance)
        {
            m_plugins.push_back(std::make_tuple(library, plugin));
            return (int)m_plugins.size() - 1;
        }
    }

    return -1;
}
/*
void PluginManager::EnumeratePlugins(const std::string& path)
{
    std::list<std::string> pluginsInDirectory;
#ifdef WIN32
    TCHAR searchPattern[MAX_PATH] = { '\0' };
    StringCchCopy(searchPattern, MAX_PATH, path.c_str());
    StringCchCat(searchPattern, MAX_PATH, TEXT("\\*.dll"));

    WIN32_FIND_DATA ffd;
    HANDLE hFind = FindFirstFile(searchPattern, &ffd);
    if (hFind == INVALID_HANDLE_VALUE)
        return;

    do
    {
        if (!(ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
            pluginsInDirectory.push_back(ffd.cFileName);
    } while (FindNextFile(hFind, &ffd));

#else
    // Linux implementation
#endif

    // Try to load the plugins found in the directory path.
    for (std::string filename : pluginsInDirectory)
    {
        std::shared_ptr<Library> library(new Library());
        if (library->LoadFile(filename.c_str()))
        {
            Plugin plugin;
            plugin.GetPluginIdentifier = static_cast<PFNGETPLUGINIDENTIFIER>(library->GetEntryPoint("GetPluginIdentifier"));
            plugin.CreateRendererInstance = static_cast<PFNCREATERENDERERINSTANCE>(library->GetEntryPoint("CreateRendererInstance"));
            plugin.DeleteRendererInstance = static_cast<PFNDELETERENDERERINSTANCE>(library->GetEntryPoint("DeleteRendererInstance"));

            if (plugin.GetPluginIdentifier && plugin.CreateRendererInstance && plugin.DeleteRendererInstance)
            {
                m_plugins.push_back(std::make_tuple(library, plugin));
            }
        }
    }
}*/

int PluginManager::GetPluginCount() const
{
    return (int)m_plugins.size();
}

bool PluginManager::GetPlugin(int index, Plugin* out_plugin)
{
    if (index < 0 || index >= m_plugins.size())
        return false;

    auto itr = m_plugins.begin();
    std::advance(itr, index);
    if (itr != m_plugins.end())
    {
        *out_plugin = std::get<1>(*itr);
        return true;
    }

    return false;
}

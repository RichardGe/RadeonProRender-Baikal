#include <RprHybrid/PluginManager.h>

FrRendererEncalps::FrRendererEncalps(rpr_int pluginID, PluginManager* pluginManager)
{
	m_pluginID = pluginID;
	m_pluginManager = pluginManager;

	Plugin plugin;
	if (pluginManager->GetPlugin(m_pluginID, &plugin))
	{
		// Try to create an instance of the plugin.
		m_FrRenderer = plugin.CreateRendererInstance();
	}
	else
	{
		m_FrRenderer = NULL;
	}

}

FrRendererEncalps::~FrRendererEncalps()
{
	if ( m_FrRenderer )
	{
		Plugin plugin;
		if (m_pluginManager->GetPlugin(m_pluginID, &plugin))
		{
			// Try to create an instance of the plugin.
			plugin.DeleteRendererInstance(m_FrRenderer);
		}
	}
}
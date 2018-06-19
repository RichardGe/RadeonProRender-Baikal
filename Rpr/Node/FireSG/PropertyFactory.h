#pragma once

#include <string>
#include <map>
#include <cstdlib>

namespace FireSG
{
    // This class stores sets of property names and default values for various values of Types.
    // For each value of Types, AddProperty can be called one or more times to register property values.
    template <typename Type, typename Key, typename PropertySet>
    class PropertyFactory
    {
    public:

        template <typename T>
        void AddProperty(Type type, Key const& key, T&& defaultValue, bool mutableType=false)
        {
            PropertySet& properties = m_typeProperties[type];
            properties.template AddProperty<T>(key, std::forward<T>(defaultValue), mutableType);
        }

        PropertySet const& GetProperties(Type type) const
        {
            auto itr = m_typeProperties.find(type);

            if (itr != m_typeProperties.end())
            {
                return itr->second;
            }
            else
            {
                throw std::runtime_error("Type not found");
            }
        }

    private:
        std::map<Type, PropertySet> m_typeProperties;
    };
}

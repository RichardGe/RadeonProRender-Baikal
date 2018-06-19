#pragma once

#include <string>
#include <type_traits>
#include <typeindex>
#include <map>
#include <stdexcept>

#ifdef WIN32
#if (_MSC_VER <= 1800)
#define FR_NOEXCEPT
#else
#define FR_NOEXCEPT noexcept
#endif
#else
#define FR_NOEXCEPT noexcept
#endif

#if 1
inline
size_t rprHashCode( const char* __ptr )
{
     size_t __hash = 5381;
     while (unsigned char __c = static_cast<unsigned char>(*__ptr++))
       __hash = (__hash * 33) ^ __c;
     return __hash;
}
#define GET_TYPE_ID(T) rprHashCode( std::type_index(typeid(T)).name() )
#else
#define GET_TYPE_ID(T) std::type_index(typeid(T)).hash_code()
#endif

namespace FireSG
{
	class property_not_found_error : public std::exception
	{
	public:
		const char* what() const FR_NOEXCEPT override
		{
			return "Property not found";
		}
	};

	class property_key_exists_error : public std::exception
	{
	public:
		const char* what() const FR_NOEXCEPT override
		{
			return "Property key already exists";
		}
	};

	class property_type_error : public std::exception
	{
	public:
		const char* what() const FR_NOEXCEPT override 
		{
			return "Property type does not match";
		}
	};
	
	class IProperty
    {
    public:
        IProperty() : m_mutable(false) { }

        virtual ~IProperty() { }

        virtual IProperty* Clone() = 0;

        virtual bool IsDirty() const = 0;

        virtual void SetDirty() = 0;

        virtual void ClearDirty() = 0;

        virtual size_t GetTypeId() const = 0;

        virtual size_t GetTypeSize() const = 0;

        void SetMutable(bool value)
        {
            m_mutable = value;
        }

        bool IsMutable() const
        {
            return m_mutable;
        }

    private:
        bool m_mutable;
    };

    template <typename T>
    class Property : public IProperty
    {
    public:
        Property()
            : m_dirty(false)
            , m_typeId(GET_TYPE_ID(typename std::decay<T>::type))
        {
        }

        Property(T const& value)
            : m_data(value)
            , m_dirty(false)
            , m_typeId(GET_TYPE_ID(typename std::decay<T>::type))
        {
        }

        Property(T&& value)
            : m_data(std::move(value))
            , m_dirty(false)
            , m_typeId(GET_TYPE_ID(typename std::decay<T>::type))
        {
        }

        IProperty* Clone()
        {
            return new Property<T>(*this);
        }

        bool IsDirty() const
        {
            return m_dirty;
        }

        void SetDirty()
        {
            m_dirty = true;
        }

        void ClearDirty()
        {
            m_dirty = false;
        }

        size_t GetTypeId() const
        {
            return m_typeId;
        }

        size_t GetTypeSize() const
        {
            return sizeof(T);
        }

        T& Get()
        {
            return m_data;
        }

        void Set(const T& value)
        {
            m_data = value;
            m_dirty = true;
        }

        void Set(T&& value)
        {
            m_data = std::move(value);
            m_dirty = true;
        }

    private:
        Property(const Property& rhs)
        {
            m_data = rhs.m_data;
            m_dirty = rhs.m_dirty;
            m_typeId = rhs.m_typeId;
        }

        T m_data;
        bool m_dirty;
        size_t m_typeId;
    };

    template <typename Key>
    class PropertySet
    {
    public:
        PropertySet()
        {

        }

        PropertySet(PropertySet const& rhs)
        {
            for (auto& kv : rhs.m_properties)
                m_properties[kv.first] = kv.second->Clone();
        }

        ~PropertySet()
        {
            for (auto& kv : m_properties)
                delete kv.second;
        }

        template <typename T>
        void AddProperty(Key const& key, T&& defaultValue, bool mutableType = false)
        {
            auto itr = m_properties.find(key);
            if (itr == m_properties.cend())
            {
                IProperty* prop = new Property<typename std::decay<T>::type>(std::forward<T>(defaultValue));
                prop->SetMutable(mutableType);
                m_properties[key] = prop;
            }
            else
            {
                throw property_key_exists_error();
            }
        }

        template <typename T>
        T& GetProperty(Key const& key)
        {
            auto itr = m_properties.find(key);
            if (itr != m_properties.cend())
            {
                if (itr->second->GetTypeId() != GET_TYPE_ID(T))
                    throw std::bad_typeid();

                Property<T>* property = static_cast<Property<T>*>(itr->second);
                return property->Get();
            }
            else
            {
                throw property_not_found_error();
            }
        }

        template <typename T>
        void SetProperty(Key const& key, T&& value)
        {
	    using PropType = typename std::decay<T>::type;

            auto itr = m_properties.find(key);
            if (itr != m_properties.cend())
            {
                if (itr->second->GetTypeId() != GET_TYPE_ID(PropType))
                {
                    if (itr->second->IsMutable())
                    {
                        delete itr->second;
                        m_properties.erase(itr);
                        m_properties[key] = new Property<PropType>(std::forward<T>(value));
						m_properties[key]->SetMutable(true);
                        return;
                    }
                    else
                    {
                        throw property_type_error();
                    }
                }

                Property<PropType>* property = static_cast<Property<PropType>*>(itr->second);
                property->Set(std::forward<T>(value));
            }
            else
            {
                throw property_not_found_error();
            }
        }

        size_t GetNumProperties() const
        {
            return m_properties.size();
        }

        Key GetPropertyKey(size_t index) const
        {
            auto itr = m_properties.cbegin();
            std::advance(itr, index);
            return itr->first;
        }

        size_t GetTypeId(Key const& key) const
        {
            auto itr = m_properties.find(key);
            if (itr != m_properties.cend())
            {
                IProperty* property = itr->second;
                return property->GetTypeId();
            }
            else
            {
                throw property_not_found_error();
            }
        }

        size_t GetPropertyTypeSize(Key const& key)
        {
            auto itr = m_properties.find(key);
            if (itr != m_properties.cend())
            {
                IProperty* property = itr->second;
                return property->GetTypeSize();
            }
            else
            {
                throw property_not_found_error();
            }
        }

    private:
        std::map<Key, IProperty*> m_properties;
    };

}

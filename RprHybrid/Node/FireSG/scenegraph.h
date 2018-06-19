#pragma once

#include <string>
#include <list>
#include <vector>
#include <unordered_map>
#include <array>
#include <memory>
#include <map>
#include <set>
#include <stdexcept>
#include <algorithm>
#include <functional>
#include <RprHybrid/Base/FrException.h>

namespace FireSG
{
    // Helper classes for passing additional arguments to a Node's PropertyChanged event callback.
    // TODO: This should probably be moved to a different source file.
    class ListChangedArgs
    {
    public:
        enum class Op
        {
            ItemAdded,
            ItemRemoved,
        };

        ListChangedArgs(Op op, void* item)
            : m_op(op)
            , m_item(item)
        {}

        Op Operation() const { return m_op; }

        void* Item() const { return m_item; }

    private:
        Op m_op;
        void* m_item;
    };

    // Generic class representing a scene graph node.
    template <typename Type, typename Key, typename PropertySet>
    class Node
    {
        typedef std::function<void(Node<Type, Key, PropertySet>*, Key const&, void*)> PropertyChangedCallback;
        typedef std::function<void(Node<Type, Key, PropertySet>*)> DestructorCallback;

    public:
        // Constructors.
        Node(Type type, PropertySet const& propertySet, PropertyChangedCallback const& propertyChangedCallback)
            : m_type(type)
            , m_propertySet(propertySet)
            , m_propertyChangedCallback(propertyChangedCallback)
        {
        }

        // Accessor method for retrieving a property value by named string.
        // If property name does not exist, std::runtime_error exception will be thrown.
        // If template type does not match existing stored type, std::bad_typeid exception will be thrown (by any struct).
        template <typename T>
        T& GetProperty(Key const& key)
        {
            return m_propertySet.template GetProperty<T>(key);
        }

        // Set method for storing a value in a property specified by named string.
        // If property name does not exist, std::runtime_error exception will be thrown.
        // If template type does not match existing stored type, std::bad_typeid exception will be thrown.
        // If property is successfully set, SceneGraph::NotifyAll is called.
        template <typename T>
        void SetProperty(Key const& key, T&& value, void* args = nullptr)
        {
			m_propertySet.template SetProperty<T>(key, std::forward<T>(value));
			PropertyChanged(key,args);
        }

        // Adds custom new properties to Node.
        template <typename T>
        void AddProperty(Key const& key, T&& defaultValue, bool mutableType = false)
        {
            m_propertySet.template AddProperty<T>(key, std::forward<T>(defaultValue), mutableType);
        }

        // Signals a change to the specified property.
        void PropertyChanged(Key const& key, void* args = nullptr)
        {
            m_propertyChangedCallback(this, key, args);
        }

        // Returns the number of properties assigned to the Node.
        size_t GetNumProperties() const
        {
            return m_propertySet.GetNumProperties();
        }

        // Returns the name of the property at the specified index.
        Key GetPropertyKey(size_t index) const
        {
            return m_propertySet.GetPropertyKey(index);
        }

        // Returns the type for the Node instance.
        Type GetType() const
        {
            return m_type;
        }

        // Returns the property type.
        size_t GetPropertyType(Key const& key)
        {
            return m_propertySet.GetTypeId(key);
        }

        // Returns the size in bytes of the specified property.
        size_t GetPropertyTypeSize(Key const& key)
        {
            return m_propertySet.GetPropertyTypeSize(key);
        }

    private:
        // Constructors.
        Node(Node const& rhs) = delete;
        Node(Node&& rhs) = delete;

        // Assignment operators
        Node& operator = (Node const& rhs) = delete;
        Node& operator = (Node&& rhs) = delete;

        // Type assigned to Node.
        Type m_type;

        // Set of properties assigned to the Node type.
        PropertySet m_propertySet;

        // Callback function when a Node property changes.
        PropertyChangedCallback m_propertyChangedCallback;
    };

    // Events emitted by SceneGraph class.
    enum class SceneGraphEvents
    {
        OnNodeCreated,
        OnNodeDeleted,
        OnPropertyChanged,
    };

    // FireRender scene graph class.
    template <typename Type, typename Key, typename PropertySet, typename PropertyFactory>
    class SceneGraph
    {
        friend class Node<Type, Key, PropertySet>;

        // Callback class for notifying observers of supported events.
        // This implementation isn't ideal since two function signatures are required and
        // more could be required in the future.  This should be generalized into separate
        // templated callback classes or something else.
        struct Callback
        {
            std::function<void(Node<Type, Key, PropertySet>*)> func0;
            std::function<void(Node<Type, Key, PropertySet>*, Key const&, void*)> func1;
            std::set<Type> filter;

            // Called for SceneGraphEvents::OnNodeCreated and SceneGraphEvents::OnNodeDeleted.
            void operator()(Node<Type, Key, PropertySet>* sender)
            {
                if (filter.empty() || filter.find(sender->GetType()) != filter.end())
                {
                    func0(sender);
                }
            }

            // Called for SceneGraphEvents::OnPropertyChanged.  The propertyName is added so observers
            // easily know which Node property was modified.  Else each observer would need to iterate over
            // all the properites and check for dirty bits or changed values.
            void operator()(Node<Type, Key, PropertySet>* sender, Key const& propertyKey, void* args)
            {
                if (filter.empty() || filter.find(sender->GetType()) != filter.end())
                {
                    func1(sender, propertyKey, args);
                }
            }
        };

    public:
        // Constructor.
        SceneGraph()
            : m_callbacksEnabled(true)
        {
        }

		~SceneGraph()
        {
			//debug break: inform developer that memory is not totally released
			#ifdef _DEBUG
				#ifdef _WIN32
					if( m_nodes.size() != 0  )  {__debugbreak(); }
				#endif
			#endif
        }

        // Returns the SceneGraph's PropertyFactory instance.
        PropertyFactory& GetPropertyFactory()
        {
            return m_propertyFactory;
        }

        // Creates and returns a new Node from the specified type.
        Node<Type, Key, PropertySet>* CreateNode(Type type, std::function<void(Node<Type, Key, PropertySet>*)> setupPropertiesFunc = nullptr)
        {
            const PropertySet& nodeTypeProperties = m_propertyFactory.GetProperties(type);

			auto node = new Node<Type, Key, PropertySet>(type, nodeTypeProperties,
				[&](Node<Type, Key, PropertySet>* node, Key const& key, void* args)
			{
				this->NotifyAll(node, SceneGraphEvents::OnPropertyChanged, key, args);
			});

			{
				auto node_pair = std::make_pair(node, std::unique_ptr<Node<Type, Key, PropertySet>>(node));
				m_nodes.insert(std::move(node_pair));
			}
            if (setupPropertiesFunc)
            {
                DisableCallbacks();
                setupPropertiesFunc(node);
                EnableCallbacks();
            }
            NotifyAll(node, SceneGraphEvents::OnNodeCreated);
            return node;
        }

		//delete node from memory, without checking that other nodes are referencing this node.
		//call that with care.
		void DeleteNode(Node<Type, Key, PropertySet>* node)
		{
			auto itr = m_nodes.find(node);
			if (itr != m_nodes.end())
			{
				NotifyAll(itr->second.get(), SceneGraphEvents::OnNodeDeleted);
				m_nodes.erase(itr);
			}

		}

        // Registers a callback for specified SceneGraphEvents.  Filter is optional.
        // First parameter must be SceneGraphEvents::OnNodeCreated or SceneGraphEvents::OnNodeDeleted
        // else an exception will be thrown.
        void RegisterCallback(SceneGraphEvents e, std::function<void(Node<Type, Key, PropertySet>*)> const& func, std::set<Type> const& filter = {})
        {
            Callback cb;
            cb.func0 = func;
            cb.filter = filter;

            switch (e)
            {
            case SceneGraphEvents::OnNodeCreated:
                m_cbOnNodeCreated.push_back(cb);
                break;

            case SceneGraphEvents::OnNodeDeleted:
                m_cbOnNodeDeleted.push_back(cb);
                break;

            default:
                throw std::runtime_error("Invalid SceneGraphEvent for specified callback signature");
            }
        }

        // Registers a callback for specified SceneGraphEvents (must be OnPropertyChanged).  Filter is optional.
        // Throws an exception is first parameter is anything but SceneGraphEvents::OnPropertyChanged.
        void RegisterCallback(SceneGraphEvents e, std::function<void(Node<Type, Key, PropertySet>*, Key const&, void*)> const& func, std::set<Type> const& filter = {})
        {
            Callback cb;
            cb.func1 = func;
            cb.filter = filter;

            switch (e)
            {
            case SceneGraphEvents::OnPropertyChanged:
                m_cbOnNodePropertyChanged.push_back(cb);
                break;

            default:
                throw std::runtime_error("Invalid SceneGraphEvent for specified callback signature");
            }
        }

        // Disables callbacks for SceneGraphEvents.
        void DisableCallbacks()
        {
            m_callbacksEnabled = false;
        }

        // Enables callbacks for SceneGraphEvents.
        void EnableCallbacks()
        {
            m_callbacksEnabled = true;
        }

        void ClearCallbacks()
        {
            m_cbOnNodeCreated.clear();
            m_cbOnNodeDeleted.clear();
            m_cbOnNodePropertyChanged.clear();
        }

    private:
        // Notifies all observers subscribing to specified SceneGraphEvents enum value.
        void NotifyAll(Node<Type, Key, PropertySet>* sender, SceneGraphEvents e, Key const& propertyKey = Key(), void* args = nullptr)
        {
            if (!m_callbacksEnabled)
                return;

            switch (e)
            {
            case SceneGraphEvents::OnNodeCreated:
                for (auto c : m_cbOnNodeCreated)
                {
                    c(sender);
                }
                break;

            case SceneGraphEvents::OnNodeDeleted:
                for (auto c : m_cbOnNodeDeleted)
                {
                    c(sender);
                }
                break;

            case SceneGraphEvents::OnPropertyChanged:
            {
                FrException copiedException;
                bool oneCallbackNoExceptionsThrown = false;

                for (auto c : m_cbOnNodePropertyChanged)
                {
                    try
                    {
                        c(sender, propertyKey, args);
                        oneCallbackNoExceptionsThrown = true;
                    }
                    catch (FrException& e)
                    {
                        copiedException = e;
                    }
                }

                if (m_cbOnNodePropertyChanged.size() > 0 && !oneCallbackNoExceptionsThrown)
                    throw copiedException;

                break;
            }
            }
        }

        // Map of Node instances added to SceneGraph.
        std::unordered_map<Node<Type, Key, PropertySet>*, std::unique_ptr<Node<Type, Key, PropertySet>>> m_nodes;

        // List of callbacks for each supported event type.
        std::vector<Callback> m_cbOnNodeCreated;
        std::vector<Callback> m_cbOnNodeDeleted;
        std::vector<Callback> m_cbOnNodePropertyChanged;

        // PropertyFactory for NodeTypes enum.
        PropertyFactory m_propertyFactory;

        // Indicates whether callbacks should be emitted when SceneGraphEvents occur.
        bool m_callbacksEnabled;
    };
}

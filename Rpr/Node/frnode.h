#pragma once

#include <stdint.h>
#include "FireSG/PropertySet.h"
#include "FireSG/PropertyFactory.h"
#include "FireSG/SceneGraph.h"
#include "Rpr/RadeonProRender.h"

// Additional property keys for internal use only.
#define FR_SCENEGRAPH                   0xFFFFFFFF
#define FR_CONTEXT_PLUGIN_LIST          0xFFFFFFFE
#define FR_CONTEXT_IMAGE_CACHE          0xFFFFFFFD
#define FR_CONTEXT_AOV_LIST             0xFFFFFFFC
#define FR_NODE_CONTEXT                 0xFFFFFFFB
//#define FR_LIGHT_SCENE                  0xFFFFFFFA  // richard ,  I remove this parameter. it makes no sense because 1 same light can be attached to several scenes
#define FR_MATERIAL_AUTORELEASE_POOL    0xFFFFFFF8
#define FR_MATERIAL_STANDARD_MAP        0xFFFFFFF7
#define FR_MATERIAL_NODE_IS_STD         0xFFFFFFF6
#define FR_NODE_GRAPH_MATERIAL_PARENT   0xFFFFFFF5
#define FR_CONTEXT_POST_EFFECT_LIST     0xFFFFFFF4
#define FR_CUSTOM_PROPERTY_KEYS_START   0xFFFFF000

// Supported FireRender Node types.
enum class NodeTypes
{
    Context,
    Scene,
    Camera,
    Image,
    FrameBuffer,
    Mesh,
    Instance,
    PointLight,
    DirectionalLight,
    SpotLight,
    EnvironmentLight,
    SkyLight,
	IESLight,
    MaterialSystem,
    Material,
    StandardMaterial,
    PostEffect,
	Composite,
	Buffer,
	HeteroVolume,
	LUT,
};


enum class StandardMaterialComponent
{
	Refraction,
	Diffuse,
	Glossy,
	Clearcoat,
    Transparency,
	Diffuse2Refraction,
	Glossy2Diffuse,
	Clearcoat2Glossy,
	Transparent2Clearcoat
};




// Forward declarations.
class FrPropertyFactory;

// Typedef implementation of Node.
typedef FireSG::Node<NodeTypes, uint32_t, FireSG::PropertySet<uint32_t>> FrNode;

using StandardMaterial = std::map<StandardMaterialComponent, rpr_material_node>;

// Typedef implementation of SceneGraph.
typedef FireSG::SceneGraph<NodeTypes, uint32_t, FireSG::PropertySet<uint32_t>, FrPropertyFactory> FrSceneGraph;

// Other typedefs.
using FrShapeList = std::set<FrNode*>;
using FrLightList = std::set<FrNode*>;
using FrHeteroVolumeList = std::set<FrNode*>;
using FrAOVList = std::vector<FrNode*>;
using FrAutoreleasePool = std::set<FrNode*>;
using FrPostEffectList = std::list<FrNode*>;
using FrAABB = fr_float[6];

// Implementation of FireSG::PropertyFactory.  Initializes properties for all supported NodeTypes.
class FrPropertyFactory : public FireSG::PropertyFactory<NodeTypes, uint32_t, FireSG::PropertySet<uint32_t>>
{
public:
    FrPropertyFactory();

private:
    void InitializeNodeProperties();
};

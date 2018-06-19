#include "../Base/Common.h"
#include "Math/matrix.h"
#include "FrNode.h"
#include "RprHybrid/RadeonProRender_GL.h"
#include <cfloat>
#include <BaikalRendererDLL/TahoeDefaultParam.h>
#include <unordered_map>

class FrRenderer;
class FrRendererEncalps;
class FrImageCache;

using namespace RadeonRays;

FrPropertyFactory::FrPropertyFactory()
{
    InitializeNodeProperties();
}

void FrPropertyFactory::InitializeNodeProperties()
{
    // Identity rpr_matrix used by several node types below.
    const rpr_matrix identity(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 );

    // Context.
    AddProperty<rpr_creation_flags>(NodeTypes::Context, RPR_CONTEXT_CREATION_FLAGS, 0);
    AddProperty<std::string>(NodeTypes::Context, RPR_CONTEXT_CACHE_PATH, "");
    AddProperty<rpr_uint>(NodeTypes::Context, RPR_CONTEXT_RENDER_STATUS, 0);
    AddProperty<rpr_uint>(NodeTypes::Context, RPR_CONTEXT_DEVICE_COUNT, 0);
    AddProperty<rpr_uint>(NodeTypes::Context, RPR_CONTEXT_PARAMETER_COUNT, RPR_CONTEXT_MAX - RPR_CONTEXT_AA_CELL_SIZE ); 
    AddProperty<std::map<rpr_int, std::shared_ptr<FrRendererEncalps>>>(NodeTypes::Context, FR_CONTEXT_PLUGIN_LIST, std::map<rpr_int, std::shared_ptr<FrRendererEncalps>>());
    AddProperty<std::shared_ptr<FrRendererEncalps>>(NodeTypes::Context, RPR_CONTEXT_ACTIVE_PLUGIN, nullptr);
    AddProperty<std::shared_ptr<FrImageCache>>(NodeTypes::Context, FR_CONTEXT_IMAGE_CACHE, nullptr);
    AddProperty<FrNode*>(NodeTypes::Context, RPR_CONTEXT_SCENE, nullptr);

    AddProperty(NodeTypes::Context, FR_CONTEXT_POST_EFFECT_LIST, FrPostEffectList());

    FrAOVList aovList(RPR_AOV_MAX);
    std::fill(aovList.begin(), aovList.end(), nullptr);
    AddProperty(NodeTypes::Context, FR_CONTEXT_AOV_LIST, aovList);

    AddProperty< std::shared_ptr<FrSceneGraph> >(NodeTypes::Context, FR_SCENEGRAPH, nullptr);
    AddProperty<rpr_uint>(NodeTypes::Context, RPR_CONTEXT_AA_CELL_SIZE, TAHOE_CONTEXT_DEFAULT_AA_CELL_SIZE);
    AddProperty<rpr_uint>(NodeTypes::Context, RPR_CONTEXT_ITERATIONS, TAHOE_CONTEXT_DEFAULT_AA_SAMPLES);
    AddProperty<rpr_aa_filter>(NodeTypes::Context, RPR_CONTEXT_IMAGE_FILTER_TYPE, TAHOE_CONTEXT_DEFAULT_IMAGE_FILTER_TYPE);
    AddProperty<rpr_float>(NodeTypes::Context, RPR_CONTEXT_IMAGE_FILTER_BOX_RADIUS, TAHOE_CONTEXT_DEFAULT_IMAGE_FILTER_BOX_RADIUS);
    AddProperty<rpr_float>(NodeTypes::Context, RPR_CONTEXT_IMAGE_FILTER_GAUSSIAN_RADIUS, TAHOE_CONTEXT_DEFAULT_IMAGE_FILTER_GAUSSIAN_RADIUS);
    AddProperty<rpr_float>(NodeTypes::Context, RPR_CONTEXT_IMAGE_FILTER_TRIANGLE_RADIUS, TAHOE_CONTEXT_DEFAULT_IMAGE_FILTER_TRIANGLE_RADIUS);
    AddProperty<rpr_float>(NodeTypes::Context, RPR_CONTEXT_IMAGE_FILTER_MITCHELL_RADIUS, TAHOE_CONTEXT_DEFAULT_IMAGE_FILTER_MITCHELL_RADIUS);
    AddProperty<rpr_float>(NodeTypes::Context, RPR_CONTEXT_IMAGE_FILTER_LANCZOS_RADIUS, TAHOE_CONTEXT_DEFAULT_IMAGE_FILTER_LANCZOS_RADIUS);
    AddProperty<rpr_float>(NodeTypes::Context, RPR_CONTEXT_IMAGE_FILTER_BLACKMANHARRIS_RADIUS, TAHOE_CONTEXT_DEFAULT_IMAGE_FILTER_BLACKMANHARRIS_RADIUS);
    AddProperty<rpr_tonemapping_operator>(NodeTypes::Context, RPR_CONTEXT_TONE_MAPPING_TYPE, TAHOE_CONTEXT_DEFAULT_TONE_MAPPING_TYPE);
    AddProperty<rpr_float>(NodeTypes::Context, RPR_CONTEXT_TONE_MAPPING_LINEAR_SCALE, TAHOE_CONTEXT_DEFAULT_TONE_MAPPING_LINEAR_SCALE);
    AddProperty<rpr_float>(NodeTypes::Context, RPR_CONTEXT_TONE_MAPPING_PHOTO_LINEAR_SENSITIVITY, TAHOE_CONTEXT_DEFAULT_TONE_MAPPING_PHOTO_LINEAR_SENSITIVITY);
    AddProperty<rpr_float>(NodeTypes::Context, RPR_CONTEXT_TONE_MAPPING_PHOTO_LINEAR_EXPOSURE, TAHOE_CONTEXT_DEFAULT_TONE_MAPPING_PHOTO_LINEAR_EXPOSURE);
    AddProperty<rpr_float>(NodeTypes::Context, RPR_CONTEXT_TONE_MAPPING_PHOTO_LINEAR_FSTOP, TAHOE_CONTEXT_DEFAULT_TONE_MAPPING_PHOTO_LINEAR_FSTOP);
	AddProperty<rpr_float>(NodeTypes::Context, RPR_CONTEXT_TONE_MAPPING_EXPONENTIAL_INTENSITY, TAHOE_CONTEXT_DEFAULT_TONE_MAPPING_EXPONENTIAL_INTENSITY);
    AddProperty<rpr_float>(NodeTypes::Context, RPR_CONTEXT_TONE_MAPPING_REINHARD02_PRE_SCALE, TAHOE_CONTEXT_DEFAULT_TONE_MAPPING_REINHARD02_PRE_SCALE);
    AddProperty<rpr_float>(NodeTypes::Context, RPR_CONTEXT_TONE_MAPPING_REINHARD02_POST_SCALE, TAHOE_CONTEXT_DEFAULT_TONE_MAPPING_REINHARD02_POST_SCALE);
    AddProperty<rpr_float>(NodeTypes::Context, RPR_CONTEXT_TONE_MAPPING_REINHARD02_BURN, TAHOE_CONTEXT_DEFAULT_TONE_MAPPING_REINHARD02_BURN);
    AddProperty<rpr_uint>(NodeTypes::Context, RPR_CONTEXT_MAX_RECURSION, TAHOE_CONTEXT_DEFAULT_MAX_RECURSION);
    AddProperty<rpr_float>(NodeTypes::Context, RPR_CONTEXT_RAY_CAST_EPISLON, TAHOE_CONTEXT_DEFAULT_RAY_CAST_EPISLON);
	AddProperty<rpr_float>(NodeTypes::Context, RPR_CONTEXT_AO_RAY_LENGTH, TAHOE_CONTEXT_DEFAULT_AO_RAY_LENGTH);
    AddProperty<rpr_float>(NodeTypes::Context, RPR_CONTEXT_RADIANCE_CLAMP, TAHOE_CONTEXT_DEFAULT_RADIANCE_CLAMP);
    AddProperty<rpr_uint>(NodeTypes::Context, RPR_CONTEXT_X_FLIP, TAHOE_CONTEXT_DEFAULT_X_FLIP);
    AddProperty<rpr_uint>(NodeTypes::Context, RPR_CONTEXT_Y_FLIP, TAHOE_CONTEXT_DEFAULT_Y_FLIP);
	AddProperty<rpr_uint>(NodeTypes::Context, RPR_CONTEXT_TRANSPARENT_BACKGROUND,TAHOE_CONTEXT_DEFAULT_TRANSPARENTBACKGROUND);
	AddProperty<rpr_uint>(NodeTypes::Context, RPR_CONTEXT_PREVIEW, TAHOE_CONTEXT_DEFAULT_PREVIEW);
	AddProperty<rpr_uint>(NodeTypes::Context, RPR_CONTEXT_FRAMECOUNT, TAHOE_CONTEXT_DEFAULT_FRAMECOUNT);
	AddProperty<rpr_uint>(NodeTypes::Context, RPR_CONTEXT_OOC_TEXTURE_CACHE, TAHOE_CONTEXT_DEFAULT_OOC_TEXTURE_CACHE);
    AddProperty<rpr_float>(NodeTypes::Context, RPR_CONTEXT_TEXTURE_GAMMA, TAHOE_CONTEXT_DEFAULT_TEXTURE_GAMMA);
	AddProperty<rpr_float>(NodeTypes::Context, RPR_CONTEXT_DISPLAY_GAMMA, TAHOE_CONTEXT_DEFAULT_DISPLAY_GAMMA);  
	AddProperty<rpr_float4>(NodeTypes::Context, RPR_CONTEXT_CLIPPING_PLANE, rpr_float4(TAHOE_CONTEXT_DEFAULT_CLIPPING_PLANE) );
	AddProperty<rpr_uint>(NodeTypes::Context, RPR_CONTEXT_MATERIAL_STACK_SIZE, TAHOE_CONTEXT_DEFAULT_STACK_SIZE);
    AddProperty<rpr_float>(NodeTypes::Context, RPR_CONTEXT_PDF_THRESHOLD, TAHOE_CONTEXT_DEFAULT_PDF_THRESHOLD);
    AddProperty<rpr_render_mode>(NodeTypes::Context, RPR_CONTEXT_RENDER_MODE, RPR_RENDER_MODE_GLOBAL_ILLUMINATION);
    AddProperty<rpr_float>(NodeTypes::Context, RPR_CONTEXT_ROUGHNESS_CAP, TAHOE_CONTEXT_DEFAULT_ROUGHNESS_CAP);
	AddProperty(NodeTypes::Context, RPR_OBJECT_NAME, std::string());
	AddProperty(NodeTypes::Context, RPR_CONTEXT_GPU0_NAME, std::string());
	AddProperty(NodeTypes::Context, RPR_CONTEXT_GPU1_NAME, std::string());
	AddProperty(NodeTypes::Context, RPR_CONTEXT_GPU2_NAME, std::string());
	AddProperty(NodeTypes::Context, RPR_CONTEXT_GPU3_NAME, std::string());
	AddProperty(NodeTypes::Context, RPR_CONTEXT_GPU4_NAME, std::string());
	AddProperty(NodeTypes::Context, RPR_CONTEXT_GPU5_NAME, std::string());
	AddProperty(NodeTypes::Context, RPR_CONTEXT_GPU6_NAME, std::string());
	AddProperty(NodeTypes::Context, RPR_CONTEXT_GPU7_NAME, std::string());
	AddProperty(NodeTypes::Context, RPR_CONTEXT_CPU_NAME, std::string());
	AddProperty<rpr_uint>(NodeTypes::Context, RPR_CONTEXT_TEXTURE_COMPRESSION, 0);
	AddProperty<rpr_uint>(NodeTypes::Context, RPR_CONTEXT_CPU_THREAD_LIMIT, 128);
	AddProperty(NodeTypes::Context, RPR_CONTEXT_LAST_ERROR_MESSAGE, std::string());
	AddProperty<rpr_uint>(NodeTypes::Context, RPR_CONTEXT_MAX_DEPTH_DIFFUSE, TAHOE_CONTEXT_DEFAULT_MAXDIFFUSEDEPTH);
	AddProperty<rpr_uint>(NodeTypes::Context, RPR_CONTEXT_MAX_DEPTH_SHADOW, TAHOE_CONTEXT_DEFAULT_MAXSHADOWDEPTH);
	AddProperty<rpr_uint>(NodeTypes::Context, RPR_CONTEXT_MAX_DEPTH_GLOSSY, TAHOE_CONTEXT_DEFAULT_MAXGLOSSYDEPTH);
	AddProperty<rpr_uint>(NodeTypes::Context, RPR_CONTEXT_MAX_DEPTH_REFRACTION, TAHOE_CONTEXT_DEFAULT_MAXREFRACTIONDEPTH);
	AddProperty<rpr_uint>(NodeTypes::Context, RPR_CONTEXT_MAX_DEPTH_GLOSSY_REFRACTION, TAHOE_CONTEXT_DEFAULT_MAXGLOSSYREFRACTIONDEPTH);
	AddProperty<std::string>(NodeTypes::Context, RPR_CONTEXT_OOC_CACHE_PATH, std::string(""));
	AddProperty<rpr_uint>(NodeTypes::Context, RPR_CONTEXT_RENDER_LAYER_MASK, 0xFFffFFff);
	AddProperty<rpr_uint>(NodeTypes::Context, RPR_CONTEXT_SINGLE_LEVEL_BVH_ENABLED, 0);
	AddProperty<void*>(NodeTypes::Context, RPR_CONTEXT_CREATEPROP_COMPILE_CALLBACK, nullptr);
	AddProperty<void*>(NodeTypes::Context, RPR_CONTEXT_CREATEPROP_COMPILE_USER_DATA, nullptr);



	AddProperty<rpr_uint>(NodeTypes::Context, RPR_CONTEXT_RANDOM_SEED, 0U);


	



    // Scene.
    AddProperty<FrNode*>(NodeTypes::Scene, FR_NODE_CONTEXT, nullptr);
    AddProperty<FrShapeList>(NodeTypes::Scene, RPR_SCENE_SHAPE_LIST, FrShapeList());
    AddProperty<FrLightList>(NodeTypes::Scene, RPR_SCENE_LIGHT_LIST, FrLightList());
	AddProperty<FrHeteroVolumeList>(NodeTypes::Scene, RPR_SCENE_HETEROVOLUME_LIST, FrHeteroVolumeList());
    AddProperty<FrNode*>(NodeTypes::Scene, RPR_SCENE_CAMERA, nullptr);
    AddProperty<FrNode*>(NodeTypes::Scene, RPR_SCENE_BACKGROUND_IMAGE, nullptr);
    AddProperty<FrNode*>(NodeTypes::Scene, RPR_SCENE_ENVIRONMENT_OVERRIDE_REFLECTION, nullptr);
    AddProperty<FrNode*>(NodeTypes::Scene, RPR_SCENE_ENVIRONMENT_OVERRIDE_REFRACTION, nullptr);
    AddProperty<FrNode*>(NodeTypes::Scene, RPR_SCENE_ENVIRONMENT_OVERRIDE_TRANSPARENCY, nullptr);
    AddProperty<FrNode*>(NodeTypes::Scene, RPR_SCENE_ENVIRONMENT_OVERRIDE_BACKGROUND, nullptr);
	AddProperty(NodeTypes::Scene, RPR_OBJECT_NAME, std::string());

    // Camera.
    AddProperty<FrNode*>(NodeTypes::Camera, FR_NODE_CONTEXT, nullptr);
    AddProperty<rpr_float>(NodeTypes::Camera, RPR_CAMERA_FOCAL_LENGTH, 35.0f);
    AddProperty<rpr_float>(NodeTypes::Camera, RPR_CAMERA_FSTOP, FLT_MAX);
    AddProperty<rpr_uint>(NodeTypes::Camera, RPR_CAMERA_APERTURE_BLADES, 0);
    AddProperty<rpr_float>(NodeTypes::Camera, RPR_CAMERA_EXPOSURE, 0.0f);
    AddProperty<rpr_float>(NodeTypes::Camera, RPR_CAMERA_RESPONSE, 0.0f);
    AddProperty<rpr_float3>(NodeTypes::Camera, RPR_CAMERA_POSITION, { TAHOE_CAMERA_DEFAULT_POSITION });
    AddProperty<rpr_float3>(NodeTypes::Camera, RPR_CAMERA_LOOKAT, { TAHOE_CAMERA_DEFAULT_LOOKAT });
    AddProperty<rpr_float3>(NodeTypes::Camera, RPR_CAMERA_UP, { TAHOE_CAMERA_DEFAULT_UP });
    AddProperty<rpr_float2>(NodeTypes::Camera, RPR_CAMERA_SENSOR_SIZE, { 36.0f, 24.0f });
    AddProperty<rpr_float>(NodeTypes::Camera, RPR_CAMERA_FOCUS_DISTANCE, 1.0f);
    AddProperty<rpr_camera_mode>(NodeTypes::Camera, RPR_CAMERA_MODE, RPR_CAMERA_MODE_PERSPECTIVE);
    AddProperty<rpr_float>(NodeTypes::Camera, RPR_CAMERA_ORTHO_WIDTH, 1.0f);
    AddProperty<rpr_float>(NodeTypes::Camera, RPR_CAMERA_ORTHO_HEIGHT, 1.0f);
		AddProperty<rpr_float>(NodeTypes::Camera, RPR_CAMERA_NEAR_PLANE, TAHOE_CAMERA_DEFAULT_NEAR_PLANE);
		AddProperty<rpr_float>(NodeTypes::Camera, RPR_CAMERA_FAR_PLANE, TAHOE_CAMERA_DEFAULT_FAR_PLANE);
    AddProperty(NodeTypes::Camera, RPR_CAMERA_TRANSFORM, identity);
	AddProperty(NodeTypes::Camera, RPR_OBJECT_NAME, std::string());
	AddProperty<rpr_float>(NodeTypes::Camera, RPR_CAMERA_FOCAL_TILT, { 0.0f });
	AddProperty<rpr_float>(NodeTypes::Camera, RPR_CAMERA_IPD,  TAHOE_CAMERA_DEFAULT_IPD );
	AddProperty<rpr_float2>(NodeTypes::Camera, RPR_CAMERA_LENS_SHIFT, { 0.0f, 0.0f });
	AddProperty<rpr_float2>(NodeTypes::Camera, RPR_CAMERA_TILT_CORRECTION, { 0.0f, 0.0f });
	AddProperty<rpr_float4>(NodeTypes::Camera, RPR_CAMERA_LINEAR_MOTION, { 0.0f, 0.0f, 0.0f, 0.0f });
	AddProperty<rpr_float4>(NodeTypes::Camera, RPR_CAMERA_ANGULAR_MOTION, { 0.0f, 1.0f, 0.0f, 0.0f });


    // Image.
    AddProperty<FrNode*>(NodeTypes::Image, FR_NODE_CONTEXT, nullptr);
    AddProperty<rpr_image_format>(NodeTypes::Image, RPR_IMAGE_FORMAT, { 4, RPR_COMPONENT_TYPE_UINT8 });
    AddProperty<rpr_image_desc>(NodeTypes::Image, RPR_IMAGE_DESC, { 0, 0, 0, 0, 0 });
    AddProperty<std::shared_ptr<unsigned char>>(NodeTypes::Image, RPR_IMAGE_DATA, std::shared_ptr<unsigned char>());
	AddProperty<size_t>(NodeTypes::Image, RPR_IMAGE_DATA_SIZEBYTE, 0);
	AddProperty(NodeTypes::Image, RPR_OBJECT_NAME, std::string());
    AddProperty<FrNode*>(NodeTypes::Image, FR_NODE_GRAPH_MATERIAL_PARENT, nullptr);
	AddProperty<rpr_uint>(NodeTypes::Image, RPR_IMAGE_WRAP, RPR_IMAGE_WRAP_TYPE_REPEAT);
	AddProperty<rpr_uint>(NodeTypes::Image, RPR_IMAGE_FILTER, RPR_IMAGE_FILTER_TYPE_LINEAR);
	AddProperty<rpr_float>(NodeTypes::Image, RPR_IMAGE_GAMMA, TAHOE_IMAGE_DEFAULT_IMAGE_GAMMA );
	AddProperty<rpr_bool>(NodeTypes::Image, RPR_IMAGE_MIPMAP_ENABLED, TAHOE_IMAGE_DEFAULT_IMAGE_MIPMAPENABLED );

    // Buffer.
    AddProperty<FrNode*>(NodeTypes::Buffer, FR_NODE_CONTEXT, nullptr);
    AddProperty<rpr_buffer_desc>(NodeTypes::Buffer, RPR_BUFFER_DESC, { 0, 0 });
    AddProperty<std::shared_ptr<unsigned char>>(NodeTypes::Buffer, RPR_BUFFER_DATA, std::shared_ptr<unsigned char>());
	AddProperty(NodeTypes::Buffer, RPR_OBJECT_NAME, std::string());
    AddProperty<FrNode*>(NodeTypes::Buffer, FR_NODE_GRAPH_MATERIAL_PARENT, nullptr);

    // Framebuffer.
    AddProperty<FrNode*>(NodeTypes::FrameBuffer, FR_NODE_CONTEXT, nullptr);
    AddProperty<rpr_framebuffer_format>(NodeTypes::FrameBuffer, RPR_FRAMEBUFFER_FORMAT, { 4, RPR_COMPONENT_TYPE_UINT8 });
    AddProperty<rpr_framebuffer_desc>(NodeTypes::FrameBuffer, RPR_FRAMEBUFFER_DESC, { 0, 0 });
    AddProperty<rpr_GLenum>(NodeTypes::FrameBuffer, RPR_FRAMEBUFFER_GL_TARGET, 0);
    AddProperty<rpr_GLint>(NodeTypes::FrameBuffer, RPR_FRAMEBUFFER_GL_MIPLEVEL, 0);
    AddProperty<rpr_GLuint>(NodeTypes::FrameBuffer, RPR_FRAMEBUFFER_GL_TEXTURE, 0);
	AddProperty(NodeTypes::FrameBuffer, RPR_OBJECT_NAME, std::string());


    // Mesh.
    AddProperty<FrNode*>(NodeTypes::Mesh, FR_NODE_CONTEXT, nullptr);
    AddProperty<FrNode*>(NodeTypes::Mesh, RPR_SHAPE_MATERIAL, nullptr);
	AddProperty<std::unordered_map<rpr_int, FrNode*>>(NodeTypes::Mesh, RPR_SHAPE_MATERIALS_PER_FACE, std::unordered_map<rpr_int, FrNode*>());
    AddProperty<FrNode*>(NodeTypes::Mesh, RPR_SHAPE_VOLUME_MATERIAL, nullptr);
	AddProperty<FrNode*>(NodeTypes::Mesh, RPR_SHAPE_HETERO_VOLUME, nullptr);
    AddProperty(NodeTypes::Mesh, RPR_SHAPE_TRANSFORM, identity);
    AddProperty<rpr_bool>(NodeTypes::Mesh, RPR_SHAPE_VISIBILITY_FLAG, RPR_TRUE, true);
    AddProperty<rpr_bool>(NodeTypes::Mesh, RPR_SHAPE_SHADOW_FLAG, RPR_TRUE);
	AddProperty<rpr_bool>(NodeTypes::Mesh, RPR_SHAPE_VISIBILITY_PRIMARY_ONLY_FLAG, RPR_TRUE);
	AddProperty<rpr_bool>(NodeTypes::Mesh, RPR_SHAPE_VISIBILITY_IN_SPECULAR_FLAG, RPR_TRUE);
	AddProperty<rpr_bool>(NodeTypes::Mesh, RPR_SHAPE_SHADOW_CATCHER_FLAG, RPR_FALSE);
    AddProperty<rpr_float4>(NodeTypes::Mesh, RPR_SHAPE_ANGULAR_MOTION, { 0.0f, 1.0f, 0.0f, 0.0f });
    AddProperty<rpr_float4>(NodeTypes::Mesh, RPR_SHAPE_LINEAR_MOTION, { 0.0f, 0.0f, 0.0f, 0.0f });
		AddProperty<rpr_float4>(NodeTypes::Mesh, RPR_SHAPE_SCALE_MOTION, { 0.0f, 0.0f, 0.0f, 0.0f });
    AddProperty<rpr_uint>(NodeTypes::Mesh, RPR_SHAPE_SUBDIVISION_FACTOR, TAHOE_SHAPE_DEFAULT_SUBDIVISION_FACTOR);
	AddProperty<rpr_float>(NodeTypes::Mesh, RPR_SHAPE_SUBDIVISION_CREASEWEIGHT, 0.0f);
	AddProperty<rpr_uint>(NodeTypes::Mesh, RPR_SHAPE_SUBDIVISION_BOUNDARYINTEROP, RPR_SUBDIV_BOUNDARY_INTERFOP_TYPE_EDGE_ONLY);
    AddProperty<rpr_float2>(NodeTypes::Mesh, RPR_SHAPE_DISPLACEMENT_SCALE, { TAHOE_SHAPE_DEFAULT_DISPLACEMENT_SCALE_MIN, TAHOE_SHAPE_DEFAULT_DISPLACEMENT_SCALE_MAX });
	AddProperty<rpr_uint>(NodeTypes::Mesh, RPR_SHAPE_OBJECT_GROUP_ID, 0 );
	AddProperty<FrNode*>(NodeTypes::Mesh, RPR_SHAPE_DISPLACEMENT_MATERIAL, nullptr);
    AddProperty<rpr_float*>(NodeTypes::Mesh, RPR_MESH_VERTEX_ARRAY, nullptr);
    AddProperty<rpr_float*>(NodeTypes::Mesh, RPR_MESH_NORMAL_ARRAY, nullptr);
    AddProperty<rpr_float*>(NodeTypes::Mesh, RPR_MESH_UV_ARRAY, nullptr);
	AddProperty<rpr_float*>(NodeTypes::Mesh, RPR_MESH_UV2_ARRAY, nullptr);
    AddProperty<rpr_int*>(NodeTypes::Mesh, RPR_MESH_VERTEX_INDEX_ARRAY, nullptr);
    AddProperty<rpr_int*>(NodeTypes::Mesh, RPR_MESH_NORMAL_INDEX_ARRAY, nullptr);
    AddProperty<rpr_int*>(NodeTypes::Mesh, RPR_MESH_UV_INDEX_ARRAY, nullptr);
	AddProperty<rpr_int*>(NodeTypes::Mesh, RPR_MESH_UV2_INDEX_ARRAY, nullptr);
    AddProperty<rpr_int*>(NodeTypes::Mesh, RPR_MESH_NUM_FACE_VERTICES_ARRAY, nullptr);
    AddProperty<size_t>(NodeTypes::Mesh, RPR_MESH_VERTEX_STRIDE, 0);
    AddProperty<size_t>(NodeTypes::Mesh, RPR_MESH_NORMAL_STRIDE, 0);
    AddProperty<size_t>(NodeTypes::Mesh, RPR_MESH_UV_STRIDE, 0);
	AddProperty<size_t>(NodeTypes::Mesh, RPR_MESH_UV2_STRIDE, 0);
    AddProperty<size_t>(NodeTypes::Mesh, RPR_MESH_VERTEX_INDEX_STRIDE, 0);
    AddProperty<size_t>(NodeTypes::Mesh, RPR_MESH_NORMAL_INDEX_STRIDE, 0);
    AddProperty<size_t>(NodeTypes::Mesh, RPR_MESH_UV_INDEX_STRIDE, 0);
	AddProperty<size_t>(NodeTypes::Mesh, RPR_MESH_UV2_INDEX_STRIDE, 0);
    AddProperty<size_t>(NodeTypes::Mesh, RPR_MESH_VERTEX_COUNT, 0);
    AddProperty<size_t>(NodeTypes::Mesh, RPR_MESH_NORMAL_COUNT, 0);
    AddProperty<size_t>(NodeTypes::Mesh, RPR_MESH_UV_COUNT, 0);
	AddProperty<size_t>(NodeTypes::Mesh, RPR_MESH_UV2_COUNT, 0);
    AddProperty<size_t>(NodeTypes::Mesh, RPR_MESH_POLYGON_COUNT, 0U);
	AddProperty<rpr_uint>(NodeTypes::Mesh, RPR_SHAPE_TYPE, RPR_SHAPE_TYPE_MESH);
	AddProperty<rpr_uint>(NodeTypes::Mesh, RPR_MESH_UV_DIM, TAHOE_SHAPE_DEFAULT_UVDIM);
	AddProperty(NodeTypes::Mesh, RPR_OBJECT_NAME, std::string());
	AddProperty<rpr_uint>(NodeTypes::Mesh, RPR_SHAPE_LAYER_MASK, 0xFFffFFff);

    // Instance.
    AddProperty<FrNode*>(NodeTypes::Instance, FR_NODE_CONTEXT, nullptr);
    AddProperty<FrNode*>(NodeTypes::Instance, RPR_INSTANCE_PARENT_SHAPE, nullptr);
    AddProperty<FrNode*>(NodeTypes::Instance, RPR_SHAPE_MATERIAL, nullptr);
    AddProperty<FrNode*>(NodeTypes::Instance, RPR_SHAPE_VOLUME_MATERIAL, nullptr);
	AddProperty<FrNode*>(NodeTypes::Instance, RPR_SHAPE_HETERO_VOLUME, nullptr);
    AddProperty(NodeTypes::Instance, RPR_SHAPE_TRANSFORM, identity);
    AddProperty<rpr_bool>(NodeTypes::Instance, RPR_SHAPE_VISIBILITY_FLAG, RPR_TRUE, true);
    AddProperty<rpr_bool>(NodeTypes::Instance, RPR_SHAPE_SHADOW_FLAG, RPR_TRUE);
	AddProperty<rpr_bool>(NodeTypes::Instance, RPR_SHAPE_VISIBILITY_PRIMARY_ONLY_FLAG, RPR_TRUE);
	AddProperty<rpr_bool>(NodeTypes::Instance, RPR_SHAPE_VISIBILITY_IN_SPECULAR_FLAG, RPR_TRUE);
	AddProperty<rpr_bool>(NodeTypes::Instance, RPR_SHAPE_SHADOW_CATCHER_FLAG, RPR_FALSE);
    AddProperty<rpr_float4>(NodeTypes::Instance, RPR_SHAPE_ANGULAR_MOTION, { 0.0f, 1.0f, 0.0f, 0.0f });
    AddProperty<rpr_float4>(NodeTypes::Instance, RPR_SHAPE_LINEAR_MOTION, { 0.0f, 0.0f, 0.0f, 0.0f });
		AddProperty<rpr_float4>(NodeTypes::Instance, RPR_SHAPE_SCALE_MOTION, { 0.0f, 0.0f, 0.0f, 0.0f });
    AddProperty<rpr_uint>(NodeTypes::Instance, RPR_SHAPE_SUBDIVISION_FACTOR, TAHOE_SHAPE_DEFAULT_SUBDIVISION_FACTOR);
	AddProperty<rpr_uint>(NodeTypes::Instance, RPR_SHAPE_OBJECT_GROUP_ID, 0 );
	AddProperty<rpr_float>(NodeTypes::Instance, RPR_SHAPE_SUBDIVISION_CREASEWEIGHT, 0.0f);
	AddProperty<rpr_uint>(NodeTypes::Instance, RPR_SHAPE_SUBDIVISION_BOUNDARYINTEROP, RPR_SUBDIV_BOUNDARY_INTERFOP_TYPE_EDGE_ONLY);
    AddProperty<rpr_float2>(NodeTypes::Instance, RPR_SHAPE_DISPLACEMENT_SCALE, { TAHOE_SHAPE_DEFAULT_DISPLACEMENT_SCALE_MIN, TAHOE_SHAPE_DEFAULT_DISPLACEMENT_SCALE_MAX } );
	AddProperty<FrNode*>(NodeTypes::Instance, RPR_SHAPE_DISPLACEMENT_MATERIAL, nullptr);
	AddProperty<rpr_uint>(NodeTypes::Instance, RPR_SHAPE_TYPE, RPR_SHAPE_TYPE_INSTANCE);
	AddProperty(NodeTypes::Instance, RPR_OBJECT_NAME, std::string());
	AddProperty<rpr_uint>(NodeTypes::Instance, RPR_SHAPE_LAYER_MASK, 0xFFffFFff);

    // PointLight.
    AddProperty<FrNode*>(NodeTypes::PointLight, FR_NODE_CONTEXT, nullptr);
    AddProperty(NodeTypes::PointLight, RPR_LIGHT_TRANSFORM, identity);
	AddProperty<rpr_uint>(NodeTypes::PointLight, RPR_LIGHT_GROUP_ID, -1);
    AddProperty<rpr_float3>(NodeTypes::PointLight, RPR_POINT_LIGHT_RADIANT_POWER, { 0.0f, 0.0f, 0.0f });
	AddProperty<rpr_uint>(NodeTypes::PointLight, RPR_LIGHT_TYPE, RPR_LIGHT_TYPE_POINT);
	AddProperty(NodeTypes::PointLight, RPR_OBJECT_NAME, std::string());


    // DirectionalLight.
    AddProperty<FrNode*>(NodeTypes::DirectionalLight, FR_NODE_CONTEXT, nullptr);
    AddProperty(NodeTypes::DirectionalLight, RPR_LIGHT_TRANSFORM, identity);
	AddProperty<rpr_uint>(NodeTypes::DirectionalLight, RPR_LIGHT_GROUP_ID, -1);
    AddProperty<rpr_float3>(NodeTypes::DirectionalLight, RPR_DIRECTIONAL_LIGHT_RADIANT_POWER, { 0.0f, 0.0f, 0.0f });
	AddProperty<rpr_float>(NodeTypes::DirectionalLight, RPR_DIRECTIONAL_LIGHT_SHADOW_SOFTNESS,  TAHOE_DIRECTIONNALLIGHT_DEFAULT_ANGLE / ((float)M_PI/2.0f) );
	AddProperty<rpr_uint>(NodeTypes::DirectionalLight, RPR_LIGHT_TYPE, RPR_LIGHT_TYPE_DIRECTIONAL);
	AddProperty(NodeTypes::DirectionalLight, RPR_OBJECT_NAME, std::string());


    // SpotLight.
    AddProperty<FrNode*>(NodeTypes::SpotLight, FR_NODE_CONTEXT, nullptr);
    AddProperty(NodeTypes::SpotLight, RPR_LIGHT_TRANSFORM, identity);
	AddProperty<rpr_uint>(NodeTypes::SpotLight, RPR_LIGHT_GROUP_ID, -1);
    AddProperty<rpr_float3>(NodeTypes::SpotLight, RPR_SPOT_LIGHT_RADIANT_POWER, { 0.0f, 0.0f, 0.0f });
    AddProperty<rpr_float2>(NodeTypes::SpotLight, RPR_SPOT_LIGHT_CONE_SHAPE, { 0.0f, 0.0f });
	AddProperty<rpr_uint>(NodeTypes::SpotLight, RPR_LIGHT_TYPE, RPR_LIGHT_TYPE_SPOT);
	AddProperty(NodeTypes::SpotLight, RPR_OBJECT_NAME, std::string());


    // EnvironmentLight.
    AddProperty<FrNode*>(NodeTypes::EnvironmentLight, FR_NODE_CONTEXT, nullptr);
    AddProperty(NodeTypes::EnvironmentLight, RPR_LIGHT_TRANSFORM, identity);
	AddProperty<rpr_uint>(NodeTypes::EnvironmentLight, RPR_LIGHT_GROUP_ID, -1);
    AddProperty<FrNode*>(NodeTypes::EnvironmentLight, RPR_ENVIRONMENT_LIGHT_IMAGE, nullptr);
    AddProperty<rpr_float>(NodeTypes::EnvironmentLight, RPR_ENVIRONMENT_LIGHT_INTENSITY_SCALE, 1.0f);
	AddProperty<rpr_uint>(NodeTypes::EnvironmentLight, RPR_LIGHT_TYPE, RPR_LIGHT_TYPE_ENVIRONMENT);
	AddProperty(NodeTypes::EnvironmentLight, RPR_OBJECT_NAME, std::string());
	AddProperty<FrShapeList>(NodeTypes::EnvironmentLight, RPR_ENVIRONMENT_LIGHT_PORTAL_LIST, FrShapeList());


    // SkyLight.
    AddProperty<FrNode*>(NodeTypes::SkyLight, FR_NODE_CONTEXT, nullptr);
    AddProperty(NodeTypes::SkyLight, RPR_LIGHT_TRANSFORM, identity);
	AddProperty<rpr_uint>(NodeTypes::SkyLight, RPR_LIGHT_GROUP_ID, -1);
    AddProperty<rpr_float>(NodeTypes::SkyLight, RPR_SKY_LIGHT_TURBIDITY, 0.0f);
    AddProperty<rpr_float>(NodeTypes::SkyLight, RPR_SKY_LIGHT_ALBEDO, 0.0f);
    AddProperty<rpr_float>(NodeTypes::SkyLight, RPR_SKY_LIGHT_SCALE, 1.0f);
	AddProperty<rpr_float3>(NodeTypes::SkyLight, RPR_SKY_LIGHT_DIRECTION, rpr_float3(TAHOE_SKYLIGHT_DEFAULT_DIRECTION));
	AddProperty<rpr_uint>(NodeTypes::SkyLight, RPR_LIGHT_TYPE, RPR_LIGHT_TYPE_SKY);
	AddProperty(NodeTypes::SkyLight, RPR_OBJECT_NAME, std::string());
	AddProperty<FrShapeList>(NodeTypes::SkyLight, RPR_SKY_LIGHT_PORTAL_LIST, FrShapeList());


	// IES
	AddProperty<FrNode*>(NodeTypes::IESLight, FR_NODE_CONTEXT, nullptr);
	AddProperty(NodeTypes::IESLight, RPR_LIGHT_TRANSFORM, identity);
	AddProperty<rpr_uint>(NodeTypes::IESLight, RPR_LIGHT_GROUP_ID, -1);
	AddProperty<rpr_float3>(NodeTypes::IESLight, RPR_IES_LIGHT_RADIANT_POWER, rpr_float3());
	AddProperty(NodeTypes::IESLight, RPR_IES_LIGHT_IMAGE_DESC, rpr_ies_image_desc{0,0,nullptr,nullptr});
	AddProperty<rpr_uint>(NodeTypes::IESLight, RPR_LIGHT_TYPE, RPR_LIGHT_TYPE_IES);
	AddProperty(NodeTypes::IESLight, RPR_OBJECT_NAME, std::string());


    // Material system.
    AddProperty<FrNode*>(NodeTypes::MaterialSystem, FR_NODE_CONTEXT, nullptr);
    AddProperty<rpr_material_system_type>(NodeTypes::MaterialSystem, RPR_MATERIAL_NODE_SYSTEM, 0);
	AddProperty(NodeTypes::MaterialSystem, RPR_OBJECT_NAME, std::string());

    // Material.
    // Most of these properties are mutable since they must be able to take on different data types.
    // Example: color could be a constant float4 or be the result of a previous calculation.
    AddProperty<FrNode*>(NodeTypes::Material, FR_NODE_CONTEXT, nullptr);
    AddProperty<FrNode*>(NodeTypes::Material, RPR_MATERIAL_NODE_SYSTEM, nullptr);
    AddProperty<rpr_material_node_type>(NodeTypes::Material, RPR_MATERIAL_NODE_TYPE, 0);
	AddProperty<FrAutoreleasePool>(NodeTypes::Material, FR_MATERIAL_AUTORELEASE_POOL, FrAutoreleasePool{});
	AddProperty(NodeTypes::Material, RPR_OBJECT_NAME, std::string());
    AddProperty<bool>(NodeTypes::Material, FR_MATERIAL_NODE_IS_STD, false);
    AddProperty<FrNode*>(NodeTypes::Material, FR_NODE_GRAPH_MATERIAL_PARENT, nullptr);

    // Post effect
    AddProperty<FrNode*>(NodeTypes::PostEffect, FR_NODE_CONTEXT, nullptr);
    AddProperty<rpr_post_effect_type>(NodeTypes::PostEffect, RPR_POST_EFFECT_TYPE, 0U);
    AddProperty(NodeTypes::PostEffect, RPR_OBJECT_NAME, std::string());

	// Composite
    AddProperty<FrNode*>(NodeTypes::Composite, FR_NODE_CONTEXT, nullptr);
    AddProperty<rpr_composite_type>(NodeTypes::Composite, RPR_COMPOSITE_TYPE, 0U);
    AddProperty(NodeTypes::Composite, RPR_OBJECT_NAME, std::string());

	// LUT
    AddProperty<FrNode*>(NodeTypes::LUT, FR_NODE_CONTEXT, nullptr);
    AddProperty(NodeTypes::LUT, RPR_OBJECT_NAME, std::string());
	AddProperty(NodeTypes::LUT, RPR_LUT_FILENAME, std::string());
	AddProperty(NodeTypes::LUT, RPR_LUT_DATA, std::string());

	// Hetero Volume.
    AddProperty<FrNode*>(NodeTypes::HeteroVolume, FR_NODE_CONTEXT, nullptr);
	AddProperty(NodeTypes::HeteroVolume, RPR_OBJECT_NAME, std::string());
	AddProperty<size_t>(NodeTypes::HeteroVolume, RPR_HETEROVOLUME_SIZE_X, 0);
	AddProperty<size_t>(NodeTypes::HeteroVolume, RPR_HETEROVOLUME_SIZE_Y, 0);
	AddProperty<size_t>(NodeTypes::HeteroVolume, RPR_HETEROVOLUME_SIZE_Z, 0);
	AddProperty<std::shared_ptr<unsigned char>>(NodeTypes::HeteroVolume, RPR_HETEROVOLUME_DATA, std::shared_ptr<unsigned char>());
	AddProperty<size_t>(NodeTypes::HeteroVolume, RPR_HETEROVOLUME_DATA_SIZEBYTE, 0);
	AddProperty(NodeTypes::HeteroVolume, RPR_HETEROVOLUME_TRANSFORM, identity);
	AddProperty<rpr_float3>(NodeTypes::HeteroVolume, RPR_HETEROVOLUME_EMISSION, { 0.0f, 0.0f, 0.0f });
	AddProperty<rpr_float3>(NodeTypes::HeteroVolume, RPR_HETEROVOLUME_ALBEDO, { 1.0f, 1.0f, 1.0f });
	AddProperty<rpr_hetero_volume_filter>(NodeTypes::HeteroVolume, RPR_HETEROVOLUME_FILTER, RPR_HETEROVOLUME_FILTER_NEAREST);
	AddProperty<std::shared_ptr<unsigned char>>(NodeTypes::HeteroVolume, RPR_HETEROVOLUME_INDICES, std::shared_ptr<unsigned char>());
	AddProperty<size_t>(NodeTypes::HeteroVolume, RPR_HETEROVOLUME_INDICES_NUMBER, 0);
	AddProperty<rpr_hetero_volume_indices_topology>(NodeTypes::HeteroVolume, RPR_HETEROVOLUME_INDICES_TOPOLOGY, RPR_HETEROVOLUME_INDICES_TOPOLOGY_I_U64);

}

/*****************************************************************************\
*
*  Module Name    FireRender.cpp
*  Project        FireRender Engine
*
*  Description    Fire Render interface implementation source file
*
*  Copyright 2011 - 2013 Advanced Micro Devices, Inc. (unpublished)
*
*  All rights reserved.  This notice is intended as a precaution against
*  inadvertent publication and does not imply publication or any waiver
*  of confidentiality.  The year included in the foregoing notice is the
*  year of creation of the work.
*
\*****************************************************************************/

#if defined(_APPLE_)
#include "stddef.h"
#else
#include "cstddef"
#endif

#include <cassert>
#include <map>
#include <unordered_map>

#include <Rpr/RadeonProRender.h>
#include <Rpr/RadeonProRender_GL.h>
#include <Rpr/RadeonProRender_CL.h>

//#include "Image/stbi.h"
//#include "Image/hdrloader.h"

#include "Base/Common.h"
#include "Base/FrException.h"
#include "Base/FrTrace.h"

#include "Math/float3.h"
#include "Math/matrix.h"

#include "Cache/FrImageCache.h"
#include "Cache/OiioImageCache.h"
#include "Cache/SimpleImageCache.h"

#include "Node/FrNode.h"

#include "Renderer/FrRenderer.h"

#include "Base/MeshArrays.h"

#include "PluginManager.h"

#ifdef RPR_FOR_BAIKAL
#include <BaikalRendererDLL/TahoeDefaultParam.h>
#include "BaikalRendererDLL/MaterialNode.h"
#else
#include <Tahoe/TahoeDefaultParam.h>
#endif

#include "Rpr/Node/ListChangedArgs_Scene.h"


#define SET_STATUS(status_ptr, value) if(status_ptr)*(status_ptr)=(value)
#define API_PRINT s_logger.printLog
#define TRACE__FUNCTION_OPEN {  s_logger.Trace__FunctionOpen(__FUNCTION__); }
#define TRACE__FUNCTION_FAILED(frNode) {  s_logger.Trace__FunctionFailed(frNode,__FUNCTION__,e.GetErrorCode()); }

//trick for developpers : if you open the RadeonProRender64.dll in binary, and search "This DLL is version ", you will see the version inside the DLL file.
//could be useful for debugging.
#define STRINGIFY__RPR_CPP____(x) #x
#define TOSTRING__RPR_CPP____(x) STRINGIFY__RPR_CPP____(x)
#define VERSION_STRING__RPR_CPP____ "This DLL is version " TOSTRING__RPR_CPP____(RPR_API_VERSION)

#ifndef RPR_FOR_BAIKAL
using namespace RadeonProRender;
#else
using namespace RadeonRays;
#endif

static std::map<uint32_t, ParameterDesc> ContextParameterDescriptions = {
	{ RPR_CONTEXT_AA_CELL_SIZE, { "aacellsize", "Numbers of cells for stratified sampling", RPR_PARAMETER_TYPE_UINT } },
	{ RPR_CONTEXT_ITERATIONS, { "aasamples", "Numbers of samples per pixel", RPR_PARAMETER_TYPE_UINT } },
	{ RPR_CONTEXT_IMAGE_FILTER_TYPE, { "imagefilter.type", "Image filter to use", RPR_PARAMETER_TYPE_UINT } },
	{ RPR_CONTEXT_IMAGE_FILTER_BOX_RADIUS, { "imagefilter.box.radius", "Image filter to use", RPR_PARAMETER_TYPE_FLOAT } },
	{ RPR_CONTEXT_IMAGE_FILTER_GAUSSIAN_RADIUS, { "imagefilter.gaussian.radius", "Filter radius", RPR_PARAMETER_TYPE_FLOAT } },
	{ RPR_CONTEXT_IMAGE_FILTER_TRIANGLE_RADIUS, { "imagefilter.triangle.radius", "Filter radius", RPR_PARAMETER_TYPE_FLOAT } },
	{ RPR_CONTEXT_IMAGE_FILTER_MITCHELL_RADIUS, { "imagefilter.mitchell.radius", "Filter radius", RPR_PARAMETER_TYPE_FLOAT } },
	{ RPR_CONTEXT_IMAGE_FILTER_LANCZOS_RADIUS, { "imagefilter.lanczos.radius", "Filter radius", RPR_PARAMETER_TYPE_FLOAT } },
	{ RPR_CONTEXT_IMAGE_FILTER_BLACKMANHARRIS_RADIUS, { "imagefilter.blackmanharris.radius", "Filter radius", RPR_PARAMETER_TYPE_FLOAT } },
	{ RPR_CONTEXT_TONE_MAPPING_TYPE, { "tonemapping.type", "Tonemapping operator", RPR_PARAMETER_TYPE_UINT } },
	{ RPR_CONTEXT_TONE_MAPPING_LINEAR_SCALE, { "tonemapping.linear.scale", "Linear scale", RPR_PARAMETER_TYPE_FLOAT } },
	{ RPR_CONTEXT_TONE_MAPPING_PHOTO_LINEAR_SENSITIVITY, { "tonemapping.photolinear.sensitivity", "Linear sensitivity", RPR_PARAMETER_TYPE_FLOAT } },
	{ RPR_CONTEXT_TONE_MAPPING_PHOTO_LINEAR_EXPOSURE, { "tonemapping.photolinear.exposure", "Photolinear exposure", RPR_PARAMETER_TYPE_FLOAT } },
	{ RPR_CONTEXT_TONE_MAPPING_PHOTO_LINEAR_FSTOP, { "tonemapping.photolinear.fstop", "Photolinear fstop", RPR_PARAMETER_TYPE_FLOAT } },
	{ RPR_CONTEXT_TONE_MAPPING_EXPONENTIAL_INTENSITY, { "tonemapping.exponential.intensity", "Exponential Tonemapping intensity", RPR_PARAMETER_TYPE_FLOAT } },
	{ RPR_CONTEXT_TONE_MAPPING_REINHARD02_PRE_SCALE, { "tonemapping.reinhard02.prescale", "Reinhard prescale", RPR_PARAMETER_TYPE_FLOAT } },
	{ RPR_CONTEXT_TONE_MAPPING_REINHARD02_POST_SCALE, { "tonemapping.reinhard02.postscale", "Reinhard postscale", RPR_PARAMETER_TYPE_FLOAT } },
	{ RPR_CONTEXT_TONE_MAPPING_REINHARD02_BURN, { "tonemapping.reinhard02.burn", "Reinhard burn", RPR_PARAMETER_TYPE_FLOAT } },
	{ RPR_CONTEXT_MAX_RECURSION, { "maxRecursion", "Ray trace depth", RPR_PARAMETER_TYPE_UINT } },
	{ RPR_CONTEXT_RAY_CAST_EPISLON, { "raycastepsilon", "Ray epsilon", RPR_PARAMETER_TYPE_FLOAT } },
	{ RPR_CONTEXT_AO_RAY_LENGTH, { "aoraylength", "AO ray Length", RPR_PARAMETER_TYPE_FLOAT } },
	{ RPR_CONTEXT_RADIANCE_CLAMP, { "radianceclamp", "Max radiance value", RPR_PARAMETER_TYPE_FLOAT } },
	{ RPR_CONTEXT_X_FLIP, { "xflip", "Flip framebuffer output along X axis", RPR_PARAMETER_TYPE_UINT } },
	{ RPR_CONTEXT_Y_FLIP, { "yflip", "Flip framebuffer output along Y axis", RPR_PARAMETER_TYPE_UINT } },
	{ RPR_CONTEXT_TRANSPARENT_BACKGROUND, { "transparentbackground", "0 or 1. Enable the transparent background", RPR_PARAMETER_TYPE_UINT } },
	{ RPR_CONTEXT_PREVIEW, { "preview", "optimization for preview render mode", RPR_PARAMETER_TYPE_UINT } },
	{ RPR_CONTEXT_FRAMECOUNT, { "framecount", "framecount", RPR_PARAMETER_TYPE_UINT } },
	{ RPR_CONTEXT_OOC_TEXTURE_CACHE, { "ooctexcache", "OOC texture cache", RPR_PARAMETER_TYPE_UINT } },
	{ RPR_CONTEXT_TEXTURE_GAMMA, { "texturegamma", "Texture gamma", RPR_PARAMETER_TYPE_FLOAT } },
	{ RPR_CONTEXT_DISPLAY_GAMMA, { "displaygamma", "Display gamma", RPR_PARAMETER_TYPE_FLOAT } },
	{ RPR_CONTEXT_CLIPPING_PLANE, { "clippingplane", "Clipping Plane", RPR_PARAMETER_TYPE_FLOAT4 } },
	{ RPR_CONTEXT_MATERIAL_STACK_SIZE, { "stacksize", "Maximum number of nodes that a material graph can support. Constant value.", RPR_PARAMETER_TYPE_UINT } },
	{ RPR_CONTEXT_PDF_THRESHOLD, { "pdfThreshold", "pdf Threshold", RPR_PARAMETER_TYPE_FLOAT } },
	{ RPR_CONTEXT_RENDER_MODE, { "stage", "render mode - Deprecated : use AOV instead", RPR_PARAMETER_TYPE_UINT } },
	{ RPR_CONTEXT_RENDER_MODE, { "rendermode", "render mode - Deprecated : use AOV instead", RPR_PARAMETER_TYPE_UINT } },
	{ RPR_CONTEXT_ROUGHNESS_CAP, { "roughnessCap", "roughness Cap", RPR_PARAMETER_TYPE_FLOAT } },
	{ RPR_CONTEXT_GPU0_NAME, { "gpu0name", "Name of the GPU index 0 in context. Constant value.", RPR_PARAMETER_TYPE_STRING } },
	{ RPR_CONTEXT_GPU1_NAME, { "gpu1name", "Name of the GPU index 1 in context. Constant value.", RPR_PARAMETER_TYPE_STRING } },
	{ RPR_CONTEXT_GPU2_NAME, { "gpu2name", "Name of the GPU index 2 in context. Constant value.", RPR_PARAMETER_TYPE_STRING } },
	{ RPR_CONTEXT_GPU3_NAME, { "gpu3name", "Name of the GPU index 3 in context. Constant value.", RPR_PARAMETER_TYPE_STRING } },
	{ RPR_CONTEXT_GPU4_NAME, { "gpu4name", "Name of the GPU index 4 in context. Constant value.", RPR_PARAMETER_TYPE_STRING } },
	{ RPR_CONTEXT_GPU5_NAME, { "gpu5name", "Name of the GPU index 5 in context. Constant value.", RPR_PARAMETER_TYPE_STRING } },
	{ RPR_CONTEXT_GPU6_NAME, { "gpu6name", "Name of the GPU index 6 in context. Constant value.", RPR_PARAMETER_TYPE_STRING } },
	{ RPR_CONTEXT_GPU7_NAME, { "gpu7name", "Name of the GPU index 7 in context. Constant value.", RPR_PARAMETER_TYPE_STRING } },
	{ RPR_CONTEXT_CPU_NAME, { "cpuname", "Name of the CPU in context. Constant value.", RPR_PARAMETER_TYPE_STRING } },
	{ RPR_CONTEXT_TEXTURE_COMPRESSION, { "texturecompression", "enable texture compression", RPR_PARAMETER_TYPE_UINT } },
	{ RPR_CONTEXT_CPU_THREAD_LIMIT, { "cputhreadlimit", "limit number of CPU threads. Constant value.", RPR_PARAMETER_TYPE_UINT } },
	{ RPR_CONTEXT_API_VERSION, { "RPR_API_VERSION", "corresponds to RPR_API_VERSION. Constant value.", RPR_PARAMETER_TYPE_UINT } },
	{ RPR_CONTEXT_LAST_ERROR_MESSAGE, { "lasterror", "last error message as string.", RPR_PARAMETER_TYPE_STRING } },
	{ RPR_CONTEXT_MAX_DEPTH_DIFFUSE, { "maxdepth.diffuse", "Ray trace depth", RPR_PARAMETER_TYPE_UINT } },
	{ RPR_CONTEXT_MAX_DEPTH_SHADOW, { "maxdepth.shadow", "shadow depth", RPR_PARAMETER_TYPE_UINT } },
	{ RPR_CONTEXT_MAX_DEPTH_GLOSSY, { "maxdepth.glossy", "Ray trace depth", RPR_PARAMETER_TYPE_UINT } },
	{ RPR_CONTEXT_MAX_DEPTH_REFRACTION, { "maxdepth.refraction", "Ray trace depth", RPR_PARAMETER_TYPE_UINT } },
	{ RPR_CONTEXT_MAX_DEPTH_GLOSSY_REFRACTION, { "maxdepth.refraction.glossy", "Ray trace depth", RPR_PARAMETER_TYPE_UINT } },
	{ RPR_CONTEXT_OOC_CACHE_PATH, { "ooccachepath", "change the out-of-core directory path", RPR_PARAMETER_TYPE_STRING } },
	{ RPR_CONTEXT_RENDER_LAYER_MASK, { "renderlayermask", "unsingned int 32 render layer mask for shape", RPR_PARAMETER_TYPE_UINT } },
	{ RPR_CONTEXT_SINGLE_LEVEL_BVH_ENABLED, { "singlelevelbvh", "0 or 1 to disable/enable single BVH", RPR_PARAMETER_TYPE_UINT } },


	#ifdef RPR_FOR_BAIKAL
	{ RPR_CONTEXT_RANDOM_SEED, { "randseed", "0 or 1 to disable/enable single BVH", RPR_PARAMETER_TYPE_UINT } },
	#endif

};

static std::map<std::string, uint32_t> ContextParameterNamesToKeys = {
	{ "aacellsize", RPR_CONTEXT_AA_CELL_SIZE },
	{ "aasamples", RPR_CONTEXT_ITERATIONS },
	{ "imagefilter.type", RPR_CONTEXT_IMAGE_FILTER_TYPE },
	{ "imagefilter.box.radius", RPR_CONTEXT_IMAGE_FILTER_BOX_RADIUS },
	{ "imagefilter.gaussian.radius", RPR_CONTEXT_IMAGE_FILTER_GAUSSIAN_RADIUS },
	{ "imagefilter.triangle.radius", RPR_CONTEXT_IMAGE_FILTER_TRIANGLE_RADIUS },
	{ "imagefilter.mitchell.radius", RPR_CONTEXT_IMAGE_FILTER_MITCHELL_RADIUS },
	{ "imagefilter.lanczos.radius", RPR_CONTEXT_IMAGE_FILTER_LANCZOS_RADIUS },
	{ "imagefilter.blackmanharris.radius", RPR_CONTEXT_IMAGE_FILTER_BLACKMANHARRIS_RADIUS },
	{ "tonemapping.type", RPR_CONTEXT_TONE_MAPPING_TYPE },
	{ "tonemapping.linear.scale", RPR_CONTEXT_TONE_MAPPING_LINEAR_SCALE },
	{ "tonemapping.photolinear.sensitivity", RPR_CONTEXT_TONE_MAPPING_PHOTO_LINEAR_SENSITIVITY },
	{ "tonemapping.photolinear.exposure", RPR_CONTEXT_TONE_MAPPING_PHOTO_LINEAR_EXPOSURE },
	{ "tonemapping.photolinear.fstop", RPR_CONTEXT_TONE_MAPPING_PHOTO_LINEAR_FSTOP },
	{ "tonemapping.exponential.intensity", RPR_CONTEXT_TONE_MAPPING_EXPONENTIAL_INTENSITY },
	{ "tonemapping.reinhard02.prescale", RPR_CONTEXT_TONE_MAPPING_REINHARD02_PRE_SCALE },
	{ "tonemapping.reinhard02.postscale", RPR_CONTEXT_TONE_MAPPING_REINHARD02_POST_SCALE },
	{ "tonemapping.reinhard02.burn", RPR_CONTEXT_TONE_MAPPING_REINHARD02_BURN },
	{ "maxrecursion", RPR_CONTEXT_MAX_RECURSION },
	{ "raycastepsilon", RPR_CONTEXT_RAY_CAST_EPISLON },
	{ "aoraylength", RPR_CONTEXT_AO_RAY_LENGTH },
	{ "radianceclamp", RPR_CONTEXT_RADIANCE_CLAMP },
	{ "xflip", RPR_CONTEXT_X_FLIP },
	{ "yflip", RPR_CONTEXT_Y_FLIP },
	{ "transparentbackground", RPR_CONTEXT_TRANSPARENT_BACKGROUND },
	{ "preview", RPR_CONTEXT_PREVIEW },
	{ "framecount", RPR_CONTEXT_FRAMECOUNT },
	{ "ooctexcache", RPR_CONTEXT_OOC_TEXTURE_CACHE },
	{ "texturegamma", RPR_CONTEXT_TEXTURE_GAMMA, },
	{ "displaygamma", RPR_CONTEXT_DISPLAY_GAMMA, },
	{ "clippingplane", RPR_CONTEXT_CLIPPING_PLANE, },
	{ "stacksize", RPR_CONTEXT_MATERIAL_STACK_SIZE, },
	{ "pdfthreshold", RPR_CONTEXT_PDF_THRESHOLD },
	{ "stage", RPR_CONTEXT_RENDER_MODE },
	{ "rendermode", RPR_CONTEXT_RENDER_MODE },
	{ "roughnesscap", RPR_CONTEXT_ROUGHNESS_CAP },
	{ "gpu0name", RPR_CONTEXT_GPU0_NAME, },
	{ "gpu1name", RPR_CONTEXT_GPU1_NAME, },
	{ "gpu2name", RPR_CONTEXT_GPU2_NAME, },
	{ "gpu3name", RPR_CONTEXT_GPU3_NAME, },
	{ "gpu4name", RPR_CONTEXT_GPU4_NAME, },
	{ "gpu5name", RPR_CONTEXT_GPU5_NAME, },
	{ "gpu6name", RPR_CONTEXT_GPU6_NAME, },
	{ "gpu7name", RPR_CONTEXT_GPU7_NAME, },
	{ "cpuname", RPR_CONTEXT_CPU_NAME, },
	{ "texturecompression", RPR_CONTEXT_TEXTURE_COMPRESSION },
	{ "cputhreadlimit", RPR_CONTEXT_CPU_THREAD_LIMIT, },
	{ "RPR_API_VERSION", RPR_CONTEXT_API_VERSION, },
	{ "lasterror", RPR_CONTEXT_LAST_ERROR_MESSAGE, },
	{ "maxdepth.diffuse", RPR_CONTEXT_MAX_DEPTH_DIFFUSE },
	{ "maxdepth.shadow", RPR_CONTEXT_MAX_DEPTH_SHADOW },
	{ "maxdepth.glossy", RPR_CONTEXT_MAX_DEPTH_GLOSSY },
	{ "maxdepth.refraction", RPR_CONTEXT_MAX_DEPTH_REFRACTION },
	{ "maxdepth.refraction.glossy", RPR_CONTEXT_MAX_DEPTH_GLOSSY_REFRACTION },
	{ "ooccachepath", RPR_CONTEXT_OOC_CACHE_PATH },
	{ "renderlayermask", RPR_CONTEXT_RENDER_LAYER_MASK },
	{ "singlelevelbvh", RPR_CONTEXT_SINGLE_LEVEL_BVH_ENABLED },

	#ifdef RPR_FOR_BAIKAL
	{ "randseed"  ,   RPR_CONTEXT_RANDOM_SEED  }
	#endif

};

static std::map<std::string, uint32_t> PostEffectParameterNamesToKeys = {
    { "colortemp", RPR_POST_EFFECT_WHITE_BALANCE_COLOR_TEMPERATURE },
    { "colorspace", RPR_POST_EFFECT_WHITE_BALANCE_COLOR_SPACE },

	{ "exposure", RPR_POST_EFFECT_SIMPLE_TONEMAP_EXPOSURE },
	{ "contrast", RPR_POST_EFFECT_SIMPLE_TONEMAP_CONTRAST },
	{ "tonemap", RPR_POST_EFFECT_SIMPLE_TONEMAP_ENABLE_TONEMAP },
};

static std::map<std::string, uint32_t> CompositeParameterNamesToKeys = {
    { "framebuffer.input", RPR_COMPOSITE_FRAMEBUFFER_INPUT_FB },
	{ "lut.lut", RPR_COMPOSITE_LUT_INPUT_LUT },
	{ "lut.color", RPR_COMPOSITE_LUT_INPUT_COLOR },
	{ "normalize.color", RPR_COMPOSITE_NORMALIZE_INPUT_COLOR },
	{ "normalize.shadowcatcher", RPR_COMPOSITE_NORMALIZE_INPUT_SHADOWCATCHER },
	{ "constant.input", RPR_COMPOSITE_CONSTANT_INPUT_VALUE },
	{ "lerp.color0", RPR_COMPOSITE_LERP_VALUE_INPUT_COLOR0 },
	{ "lerp.color1", RPR_COMPOSITE_LERP_VALUE_INPUT_COLOR1 },
	{ "lerp.weight", RPR_COMPOSITE_LERP_VALUE_INPUT_WEIGHT },
	{ "arithmetic.color0", RPR_COMPOSITE_ARITHMETIC_INPUT_COLOR0 },
	{ "arithmetic.color1", RPR_COMPOSITE_ARITHMETIC_INPUT_COLOR1 },
	{ "arithmetic.op", RPR_COMPOSITE_ARITHMETIC_INPUT_OP },
	{ "gammacorrection.color", RPR_COMPOSITE_GAMMA_CORRECTION_INPUT_COLOR },
};

static std::map<uint32_t, std::string> MaterialNodeInputStrings = {
	{ RPR_MATERIAL_INPUT_COLOR, "color" },
	{ RPR_MATERIAL_INPUT_COLOR0, "color0" },
	{ RPR_MATERIAL_INPUT_COLOR1, "color1" },
	{ RPR_MATERIAL_INPUT_COLOR2, "color2" },
	{ RPR_MATERIAL_INPUT_COLOR3, "color3" },
	{ RPR_MATERIAL_INPUT_NORMAL, "normal" },
	{ RPR_MATERIAL_INPUT_UV, "uv" },
	{ RPR_MATERIAL_INPUT_DATA, "data" },
	{ RPR_MATERIAL_INPUT_ROUGHNESS, "roughness" },
	{ RPR_MATERIAL_INPUT_IOR, "ior" },
	{ RPR_MATERIAL_INPUT_ROUGHNESS_X, "roughness_x" },
	{ RPR_MATERIAL_INPUT_ROUGHNESS_Y, "roughness_y" },
	{ RPR_MATERIAL_INPUT_ROTATION, "rotation" },
	{ RPR_MATERIAL_INPUT_WEIGHT, "weight" },
	{ RPR_MATERIAL_INPUT_OP, "op" },
	{ RPR_MATERIAL_INPUT_INVEC, "invec" },
	{ RPR_MATERIAL_INPUT_UV_SCALE, "uv_scale" },
	{ RPR_MATERIAL_INPUT_VALUE, "value" },
	{ RPR_MATERIAL_INPUT_REFLECTANCE, "reflectance" },
	{ RPR_MATERIAL_INPUT_SCALE, "bumpscale" }, //  note : the name is badly chosen here it should be  "scale"  instead of  "bumpscale" . but we can't change it for retro-compatibility - avoid to use it in next new features
    { RPR_MATERIAL_INPUT_SCATTERING, "sigmas" },
    { RPR_MATERIAL_INPUT_ABSORBTION, "sigmaa" },
    { RPR_MATERIAL_INPUT_EMISSION, "emission" },
    { RPR_MATERIAL_INPUT_G, "g" },
    { RPR_MATERIAL_INPUT_MULTISCATTER, "multiscatter" },
	{ RPR_MATERIAL_INPUT_ANISOTROPIC, "anisotropic" },
	{ RPR_MATERIAL_INPUT_FRONTFACE, "frontface" },
	{ RPR_MATERIAL_INPUT_BACKFACE, "backface" },
	{ RPR_MATERIAL_INPUT_ORIGIN, "origin" },
	{ RPR_MATERIAL_INPUT_ZAXIS, "zaxis" },
	{ RPR_MATERIAL_INPUT_XAXIS, "xaxis" },
	{ RPR_MATERIAL_INPUT_THRESHOLD, "threshold" },
	{ RPR_MATERIAL_INPUT_OFFSET, "offset" },
	{ RPR_MATERIAL_INPUT_UV_TYPE, "uv_type" },
	{ RPR_MATERIAL_INPUT_RADIUS, "radius" },
	{ RPR_MATERIAL_INPUT_SIDE, "side" },
	
	{ RPR_MATERIAL_STANDARD_INPUT_DIFFUSE_COLOR, "diffuse.color" },
	{ RPR_MATERIAL_STANDARD_INPUT_DIFFUSE_NORMAL, "diffuse.normal" },
	{ RPR_MATERIAL_STANDARD_INPUT_GLOSSY_COLOR, "glossy.color" },
	{ RPR_MATERIAL_STANDARD_INPUT_GLOSSY_NORMAL, "glossy.normal" },
	{ RPR_MATERIAL_STANDARD_INPUT_GLOSSY_ROUGHNESS_X, "glossy.roughness_x" },
	{ RPR_MATERIAL_STANDARD_INPUT_GLOSSY_ROUGHNESS_Y, "glossy.roughness_y" },
	{ RPR_MATERIAL_STANDARD_INPUT_CLEARCOAT_COLOR, "clearcoat.color" },
	{ RPR_MATERIAL_STANDARD_INPUT_CLEARCOAT_NORMAL, "clearcoat.normal" },
	{ RPR_MATERIAL_STANDARD_INPUT_REFRACTION_COLOR, "refraction.color" },
	{ RPR_MATERIAL_STANDARD_INPUT_REFRACTION_NORMAL, "refraction.normal" },
	{ RPR_MATERIAL_STANDARD_INPUT_REFRACTION_ROUGHNESS, "refraction.roughness" },   //  REFRACTION doesn't have roughness parameter.
	{ RPR_MATERIAL_STANDARD_INPUT_REFRACTION_IOR, "refraction.ior" },
    { RPR_MATERIAL_STANDARD_INPUT_TRANSPARENCY_COLOR, "transparency.color" },
	{ RPR_MATERIAL_STANDARD_INPUT_DIFFUSE_TO_REFRACTION_WEIGHT, "weights.diffuse2refraction" },
	{ RPR_MATERIAL_STANDARD_INPUT_GLOSSY_TO_DIFFUSE_WEIGHT, "weights.glossy2diffuse" },
	{ RPR_MATERIAL_STANDARD_INPUT_CLEARCOAT_TO_GLOSSY_WEIGHT, "weights.clearcoat2glossy" },
	{ RPR_MATERIAL_STANDARD_INPUT_TRANSPARENCY, "weights.transparency" },
	{ RPR_MATERIAL_INPUT_RASTER_COLOR, "raster.color" },
	{ RPR_MATERIAL_INPUT_RASTER_NORMAL, "raster.normal" },
	{ RPR_MATERIAL_INPUT_RASTER_METALLIC, "raster.metallic" },
	{ RPR_MATERIAL_INPUT_RASTER_ROUGHNESS, "raster.roughness" },
	{ RPR_MATERIAL_INPUT_RASTER_SUBSURFACE, "raster.subsurface" },
	{ RPR_MATERIAL_INPUT_RASTER_ANISOTROPIC, "raster.anisotropic" },
	{ RPR_MATERIAL_INPUT_RASTER_SPECULAR, "raster.specular" },
	{ RPR_MATERIAL_INPUT_RASTER_SPECULARTINT, "raster.specularTint" },
	{ RPR_MATERIAL_INPUT_RASTER_SHEEN, "raster.sheen" },
	{ RPR_MATERIAL_INPUT_RASTER_SHEENTINT, "raster.sheenTint" },
	{ RPR_MATERIAL_INPUT_RASTER_CLEARCOAT, "raster.clearcoat" },
	{ RPR_MATERIAL_INPUT_RASTER_CLEARCOATGLOSS, "raster.clearcoatGloss" }

};

static std::map<std::string, uint32_t> MaterialNodeInputKeys = {
	{ "color", RPR_MATERIAL_INPUT_COLOR },
	{ "color0", RPR_MATERIAL_INPUT_COLOR0 },
	{ "color1", RPR_MATERIAL_INPUT_COLOR1 },
	{ "color2", RPR_MATERIAL_INPUT_COLOR2 },
	{ "color3", RPR_MATERIAL_INPUT_COLOR3 },
	{ "normal", RPR_MATERIAL_INPUT_NORMAL },
	{ "uv", RPR_MATERIAL_INPUT_UV },
	{ "data", RPR_MATERIAL_INPUT_DATA },
	{ "roughness", RPR_MATERIAL_INPUT_ROUGHNESS },
	{ "ior", RPR_MATERIAL_INPUT_IOR },
	{ "roughness_x", RPR_MATERIAL_INPUT_ROUGHNESS_X },
	{ "roughness_y", RPR_MATERIAL_INPUT_ROUGHNESS_Y },
	{ "rotation", RPR_MATERIAL_INPUT_ROTATION },
	{ "weight", RPR_MATERIAL_INPUT_WEIGHT },
	{ "op", RPR_MATERIAL_INPUT_OP },
	{ "invec", RPR_MATERIAL_INPUT_INVEC },
	{ "uv_scale", RPR_MATERIAL_INPUT_UV_SCALE },
	{ "value", RPR_MATERIAL_INPUT_VALUE },
	{ "reflectance", RPR_MATERIAL_INPUT_REFLECTANCE },
	{ "bumpscale", RPR_MATERIAL_INPUT_SCALE },   //  note : the name is badly chosen here it should be  "scale"  instead of  "bumpscale" . but we can't change it for retro-compatibility - avoid to use it in next new features
    { "sigmas", RPR_MATERIAL_INPUT_SCATTERING },
    { "sigmaa", RPR_MATERIAL_INPUT_ABSORBTION },
    { "emission", RPR_MATERIAL_INPUT_EMISSION },
    { "g", RPR_MATERIAL_INPUT_G },
    { "multiscatter", RPR_MATERIAL_INPUT_MULTISCATTER },
	{ "anisotropic", RPR_MATERIAL_INPUT_ANISOTROPIC },
	{ "frontface", RPR_MATERIAL_INPUT_FRONTFACE },
	{ "backface", RPR_MATERIAL_INPUT_BACKFACE },
	{ "origin", RPR_MATERIAL_INPUT_ORIGIN },
	{ "zaxis", RPR_MATERIAL_INPUT_ZAXIS },
	{ "xaxis", RPR_MATERIAL_INPUT_XAXIS },
	{ "threshold", RPR_MATERIAL_INPUT_THRESHOLD },
	{ "offset", RPR_MATERIAL_INPUT_OFFSET },
	{ "uv_type", RPR_MATERIAL_INPUT_UV_TYPE },
	{ "radius", RPR_MATERIAL_INPUT_RADIUS },
	{ "side", RPR_MATERIAL_INPUT_SIDE },
	
	{ "diffuse.color", RPR_MATERIAL_STANDARD_INPUT_DIFFUSE_COLOR },
	{ "diffuse.normal", RPR_MATERIAL_STANDARD_INPUT_DIFFUSE_NORMAL },
	{ "glossy.color", RPR_MATERIAL_STANDARD_INPUT_GLOSSY_COLOR },
	{ "glossy.normal", RPR_MATERIAL_STANDARD_INPUT_GLOSSY_NORMAL },
	{ "glossy.roughness_x", RPR_MATERIAL_STANDARD_INPUT_GLOSSY_ROUGHNESS_X },
	{ "glossy.roughness_y", RPR_MATERIAL_STANDARD_INPUT_GLOSSY_ROUGHNESS_Y },
	{ "clearcoat.color", RPR_MATERIAL_STANDARD_INPUT_CLEARCOAT_COLOR },
	{ "clearcoat.normal", RPR_MATERIAL_STANDARD_INPUT_CLEARCOAT_NORMAL },
	{ "refraction.color", RPR_MATERIAL_STANDARD_INPUT_REFRACTION_COLOR },
	{ "refraction.normal", RPR_MATERIAL_STANDARD_INPUT_REFRACTION_NORMAL },
	{ "refraction.roughness", RPR_MATERIAL_STANDARD_INPUT_REFRACTION_ROUGHNESS },
	{ "refraction.ior", RPR_MATERIAL_STANDARD_INPUT_REFRACTION_IOR },
    { "transparency.color", RPR_MATERIAL_STANDARD_INPUT_TRANSPARENCY_COLOR },
	{ "weights.diffuse2refraction", RPR_MATERIAL_STANDARD_INPUT_DIFFUSE_TO_REFRACTION_WEIGHT },
	{ "weights.glossy2diffuse", RPR_MATERIAL_STANDARD_INPUT_GLOSSY_TO_DIFFUSE_WEIGHT },
	{ "weights.clearcoat2glossy", RPR_MATERIAL_STANDARD_INPUT_CLEARCOAT_TO_GLOSSY_WEIGHT },
	{ "weights.transparency", RPR_MATERIAL_STANDARD_INPUT_TRANSPARENCY },
	{ "raster.color", RPR_MATERIAL_INPUT_RASTER_COLOR },
	{ "raster.normal", RPR_MATERIAL_INPUT_RASTER_NORMAL },
	{ "raster.metallic", RPR_MATERIAL_INPUT_RASTER_METALLIC  },
	{ "raster.roughness", RPR_MATERIAL_INPUT_RASTER_ROUGHNESS  },
	{ "raster.subsurface", RPR_MATERIAL_INPUT_RASTER_SUBSURFACE },
	{ "raster.anisotropic", RPR_MATERIAL_INPUT_RASTER_ANISOTROPIC },
	{ "raster.specular", RPR_MATERIAL_INPUT_RASTER_SPECULAR },
	{ "raster.specularTint", RPR_MATERIAL_INPUT_RASTER_SPECULARTINT },
	{ "raster.sheen", RPR_MATERIAL_INPUT_RASTER_SHEEN },
	{ "raster.sheenTint", RPR_MATERIAL_INPUT_RASTER_SHEENTINT },
	{ "raster.clearcoat", RPR_MATERIAL_INPUT_RASTER_CLEARCOAT },
	{ "raster.clearcoatGloss", RPR_MATERIAL_INPUT_RASTER_CLEARCOATGLOSS }
};

struct StdMatDesc
{
	StandardMaterialComponent comp;
	rpr_uint input;
};

static std::map<rpr_uint, StdMatDesc> StandardMaterialParameterMap =
{
	{ RPR_MATERIAL_STANDARD_INPUT_DIFFUSE_COLOR, { StandardMaterialComponent::Diffuse, RPR_MATERIAL_INPUT_COLOR } },
	{ RPR_MATERIAL_STANDARD_INPUT_DIFFUSE_NORMAL, { StandardMaterialComponent::Diffuse, RPR_MATERIAL_INPUT_NORMAL } },

	{ RPR_MATERIAL_STANDARD_INPUT_GLOSSY_COLOR, { StandardMaterialComponent::Glossy, RPR_MATERIAL_INPUT_COLOR } },
	{ RPR_MATERIAL_STANDARD_INPUT_GLOSSY_NORMAL, { StandardMaterialComponent::Glossy, RPR_MATERIAL_INPUT_NORMAL } },
	{ RPR_MATERIAL_STANDARD_INPUT_GLOSSY_ROUGHNESS_X, { StandardMaterialComponent::Glossy, RPR_MATERIAL_INPUT_ROUGHNESS_X } },
	{ RPR_MATERIAL_STANDARD_INPUT_GLOSSY_ROUGHNESS_Y,{ StandardMaterialComponent::Glossy, RPR_MATERIAL_INPUT_ROUGHNESS_Y } },

	{ RPR_MATERIAL_STANDARD_INPUT_CLEARCOAT_COLOR, { StandardMaterialComponent::Clearcoat, RPR_MATERIAL_INPUT_COLOR } },
	{ RPR_MATERIAL_STANDARD_INPUT_CLEARCOAT_NORMAL, { StandardMaterialComponent::Clearcoat, RPR_MATERIAL_INPUT_NORMAL } },

	{ RPR_MATERIAL_STANDARD_INPUT_REFRACTION_COLOR, { StandardMaterialComponent::Refraction, RPR_MATERIAL_INPUT_COLOR } },
	{ RPR_MATERIAL_STANDARD_INPUT_REFRACTION_NORMAL, { StandardMaterialComponent::Refraction, RPR_MATERIAL_INPUT_NORMAL } },
	{ RPR_MATERIAL_STANDARD_INPUT_REFRACTION_ROUGHNESS, { StandardMaterialComponent::Refraction, RPR_MATERIAL_INPUT_ROUGHNESS } },
	{ RPR_MATERIAL_STANDARD_INPUT_REFRACTION_IOR, { StandardMaterialComponent::Refraction, RPR_MATERIAL_INPUT_IOR } },
    { RPR_MATERIAL_STANDARD_INPUT_TRANSPARENCY_COLOR, { StandardMaterialComponent::Transparency, RPR_MATERIAL_INPUT_COLOR } },

	{ RPR_MATERIAL_STANDARD_INPUT_DIFFUSE_TO_REFRACTION_WEIGHT, { StandardMaterialComponent::Diffuse2Refraction, RPR_MATERIAL_INPUT_WEIGHT } },
	{ RPR_MATERIAL_STANDARD_INPUT_GLOSSY_TO_DIFFUSE_WEIGHT, { StandardMaterialComponent::Glossy2Diffuse, RPR_MATERIAL_INPUT_WEIGHT } },
	{ RPR_MATERIAL_STANDARD_INPUT_CLEARCOAT_TO_GLOSSY_WEIGHT, { StandardMaterialComponent::Clearcoat2Glossy, RPR_MATERIAL_INPUT_WEIGHT } },
	{ RPR_MATERIAL_STANDARD_INPUT_TRANSPARENCY, { StandardMaterialComponent::Transparent2Clearcoat, RPR_MATERIAL_INPUT_WEIGHT } }
};



#define MACRO__CHECK_ARGUMENT_TYPE(a,b)  if ( a && static_cast<FrNode*>(a)->GetType() !=  NodeTypes::b ) { throw FrException(__FILE__,__LINE__,RPR_ERROR_INVALID_PARAMETER, "invalid argument type",a); }
#define MACRO__CHECK_ARGUMENT_TYPE_ALLSHAPES(a)  if ( a && static_cast<FrNode*>(a)->GetType() !=  NodeTypes::Mesh && static_cast<FrNode*>(a)->GetType() !=  NodeTypes::Instance ) { throw FrException(__FILE__,__LINE__,RPR_ERROR_INVALID_PARAMETER, "invalid argument type",a); }

#define MACRO__CHECK_ARGUMENT_TYPE_ALLLIGHTS(a)  if ( a && static_cast<FrNode*>(a)->GetType() !=  NodeTypes::PointLight && \
													static_cast<FrNode*>(a)->GetType() !=  NodeTypes::DirectionalLight &&  \
													static_cast<FrNode*>(a)->GetType() !=  NodeTypes::SpotLight &&   \
													static_cast<FrNode*>(a)->GetType() !=  NodeTypes::EnvironmentLight &&  \
													static_cast<FrNode*>(a)->GetType() !=  NodeTypes::SkyLight &&  \
													static_cast<FrNode*>(a)->GetType() !=  NodeTypes::IESLight ) { throw FrException(__FILE__,__LINE__,RPR_ERROR_INVALID_PARAMETER, "invalid argument type",a); }


#define MACRO__CHECK_ARGUMENT_FLOAT_NAN(f,n) if ( std::isnan(f) ) { throw FrException(__FILE__,__LINE__,RPR_ERROR_INVALID_PARAMETER, "NAN float",n); }
#define MACRO__CHECK_ARGUMENT_FLOAT_INF(f,n) if ( std::isinf(f) ) { throw FrException(__FILE__,__LINE__,RPR_ERROR_INVALID_PARAMETER, "Infinite float",n); }

#define CHECK_NOT_NULL(frnode) if ( frnode == nullptr ) { throw FrException(__FILE__,__LINE__,RPR_ERROR_INVALID_PARAMETER, "null object",nullptr); }

static std::map<rpr_material_node_type, int32_t> nbMaterialInputCount;

static Logger s_logger;
static PluginManager g_pluginManager;

void DeleteIesData(FrNode* node)
{
	//free previous property
	rpr_ies_image_desc& previousState = node->GetProperty<rpr_ies_image_desc>(RPR_IES_LIGHT_IMAGE_DESC);
	if ( previousState.data )	  { delete[]previousState.data; previousState.data = NULL; }
	if ( previousState.filename ) { delete[]previousState.filename; previousState.filename = NULL; }
}

rpr_int rprRegisterPlugin(const rpr_char* path)
{

	rpr_int idReturn = g_pluginManager.RegisterPlugin(path);


	if ( idReturn != -1 )
	{
		//s_logger.Trace__NewFrObjectCreated(Logger::TRACE_ADDRESS_TYPE__TAHOEPLUGINID, (void*)idReturn);
		s_logger.Trace__DeclareFRobject("rpr_int", "tahoePluginID_0x", (void*)((size_t)idReturn));
		s_logger.TraceArg__rpr_tahoePluginID(idReturn);
		s_logger.printTrace(" = ");
	}
	TRACE__FUNCTION_OPEN
	s_logger.TraceArg__rpr_char_P(path);
	s_logger.Trace__FunctionClose();
	if ( idReturn == -1 )
	{
		s_logger.printTrace("ERROR : rprRegisterPlugin FAILED : returns -1\r\n");
	}

	return idReturn;
}


rpr_int rprCreateContext(rpr_int in_api_version, rpr_int* pluginIDs, size_t pluginCount, rpr_creation_flags in_creation_flags, const rpr_context_properties * in_props, const rpr_char* in_cache_path, rpr_context* out_context)
{

	s_logger.printTrace("\r\n//Context creation\r\n");
	s_logger.TraceArg_Prepare__rpr_context_properties_P(in_props);
	s_logger.printTrace("#if defined(RPRTRACE_DEV)\r\n");
	s_logger.Trace__FlushAllFiles(); // in case a crash, ensure trace informs the creation


	API_PRINT("rprCreateContext",0);

	try
	{

		// Check for validity of API version.
		if (in_api_version != RPR_API_VERSION)
		{
			const std::string errorMessage(VERSION_STRING__RPR_CPP____);
			throw FrException(__FILE__,__LINE__,RPR_ERROR_INVALID_API_VERSION, errorMessage.c_str(),nullptr);
		}

	
		if ( pluginIDs == nullptr || pluginCount == 0 )
			throw FrException(__FILE__,__LINE__,RPR_ERROR_INVALID_PARAMETER, "bad pluginID list.",nullptr);

		// Instantiate SceneGraph implementation.
		std::shared_ptr<FrSceneGraph> sceneGraph ( new FrSceneGraph() );

		// Create Context node.
		FrNode* context = sceneGraph->CreateNode(NodeTypes::Context);

		//example of in_props :  list of context property names and their corresponding values.
		//                       Each property name is immediately followed by the corresponding desired value.
		//                       The list is terminated with 0
		// rpr_context_properties in_props[16];
		// in_props[0] = (rpr_context_properties)RPR_CONTEXT_CREATEPROP_CPU_THREAD_LIMIT;
		// in_props[1] = (rpr_context_properties)32;
		// in_props[2] = (rpr_context_properties)0;
		if ( in_props )
		{
			for(int iArg=0; ; iArg++)
			{
				if ( (int64_t)in_props[iArg] == 0 )
				{
					break;
				}
				else if ( (int64_t)in_props[iArg] == RPR_CONTEXT_CREATEPROP_CPU_THREAD_LIMIT )
				{
					iArg++;
					int64_t value = (int64_t)in_props[iArg];
					context->SetProperty(RPR_CONTEXT_CPU_THREAD_LIMIT, (rpr_uint)value);
				}
				else if ((int64_t)in_props[iArg] == RPR_CONTEXT_CREATEPROP_COMPILE_CALLBACK)
				{
					iArg++;
					int64_t value = (int64_t)in_props[iArg];
					context->SetProperty(RPR_CONTEXT_CREATEPROP_COMPILE_CALLBACK, (void*)value);
				}
				else if ((int64_t)in_props[iArg] == RPR_CONTEXT_CREATEPROP_COMPILE_USER_DATA)
				{
					iArg++;
					int64_t value = (int64_t)in_props[iArg];
					context->SetProperty(RPR_CONTEXT_CREATEPROP_COMPILE_USER_DATA, (void*)value);
				}				
			}
		}


		context->SetProperty(RPR_CONTEXT_CREATION_FLAGS, in_creation_flags);
		context->SetProperty(RPR_CONTEXT_CACHE_PATH, static_cast<std::string>((in_cache_path) ? in_cache_path : ""));
		context->SetProperty(FR_SCENEGRAPH, sceneGraph);

#ifdef RPR_USE_OIIO
		context->SetProperty<std::shared_ptr<FrImageCache>>(FR_CONTEXT_IMAGE_CACHE, std::shared_ptr<FrImageCache>(new OiioImageCache(context)));
#else
		context->SetProperty<std::shared_ptr<FrImageCache>>(FR_CONTEXT_IMAGE_CACHE, std::shared_ptr<FrImageCache>(new SimpleImageCache(context)));
#endif

		// Create requested backend plugins.
		auto& pluginList = context->GetProperty<std::map<rpr_int, std::shared_ptr<FrRendererEncalps> >>(FR_CONTEXT_PLUGIN_LIST);
		for (auto i = 0U; i < pluginCount; ++i)
		{
			auto id = pluginIDs[i];

			// Check to see if the plugin ID has been registered.
			Plugin plugin;
			if (g_pluginManager.GetPlugin(id, &plugin))
			{
				// Try to create an instance of the plugin.
				std::shared_ptr<FrRendererEncalps> rendererEncaps ( new FrRendererEncalps(id,&g_pluginManager) );
				if (rendererEncaps->m_FrRenderer)
				{
					// Initialize the plugin and add it to the context's list.
					rendererEncaps->m_FrRenderer->Initialize(context);
					pluginList[id] = rendererEncaps;

					// Set the plugin as the current active one.
					context->SetProperty(RPR_CONTEXT_ACTIVE_PLUGIN, rendererEncaps);
				}
			}
		}

		std::shared_ptr<FrRendererEncalps> renderer = context->GetProperty< std::shared_ptr<FrRendererEncalps> >(RPR_CONTEXT_ACTIVE_PLUGIN);

		// At least one plugin must have been loaded and made active.
		if (renderer->m_FrRenderer == nullptr)
			throw FrException(__FILE__,__LINE__,RPR_ERROR_INTERNAL_ERROR, "No compute API plugin loaded!",context);



		//set devices name parameters
		struct DEVICE_FLAG_TO_CONTEXT_FLAG
		{
			DEVICE_FLAG_TO_CONTEXT_FLAG(rpr_creation_flags f, rpr_context_info   i ) { flag = f; info = i; }
			rpr_creation_flags flag;
			rpr_context_info   info;
		};
		std::vector<DEVICE_FLAG_TO_CONTEXT_FLAG> deviceFlagToContextFlag;
		deviceFlagToContextFlag.push_back(DEVICE_FLAG_TO_CONTEXT_FLAG(RPR_CREATION_FLAGS_ENABLE_GPU0,RPR_CONTEXT_GPU0_NAME));
		deviceFlagToContextFlag.push_back(DEVICE_FLAG_TO_CONTEXT_FLAG(RPR_CREATION_FLAGS_ENABLE_GPU1,RPR_CONTEXT_GPU1_NAME));
		deviceFlagToContextFlag.push_back(DEVICE_FLAG_TO_CONTEXT_FLAG(RPR_CREATION_FLAGS_ENABLE_GPU2,RPR_CONTEXT_GPU2_NAME));
		deviceFlagToContextFlag.push_back(DEVICE_FLAG_TO_CONTEXT_FLAG(RPR_CREATION_FLAGS_ENABLE_GPU3,RPR_CONTEXT_GPU3_NAME));
		deviceFlagToContextFlag.push_back(DEVICE_FLAG_TO_CONTEXT_FLAG(RPR_CREATION_FLAGS_ENABLE_GPU4,RPR_CONTEXT_GPU4_NAME));
		deviceFlagToContextFlag.push_back(DEVICE_FLAG_TO_CONTEXT_FLAG(RPR_CREATION_FLAGS_ENABLE_GPU5,RPR_CONTEXT_GPU5_NAME));
		deviceFlagToContextFlag.push_back(DEVICE_FLAG_TO_CONTEXT_FLAG(RPR_CREATION_FLAGS_ENABLE_GPU6,RPR_CONTEXT_GPU6_NAME));
		deviceFlagToContextFlag.push_back(DEVICE_FLAG_TO_CONTEXT_FLAG(RPR_CREATION_FLAGS_ENABLE_GPU7,RPR_CONTEXT_GPU7_NAME));
		deviceFlagToContextFlag.push_back(DEVICE_FLAG_TO_CONTEXT_FLAG(RPR_CREATION_FLAGS_ENABLE_CPU,RPR_CONTEXT_CPU_NAME));
		for(int i=0; i<deviceFlagToContextFlag.size(); i++)
		{
			if ( in_creation_flags & deviceFlagToContextFlag[i].flag ) 
			{
				char* deviceName = new char[ renderer->m_FrRenderer->GetInfoSize(deviceFlagToContextFlag[i].info) ];
				renderer->m_FrRenderer->GetInfo(deviceFlagToContextFlag[i].info,deviceName);
				context->SetProperty<std::string>(deviceFlagToContextFlag[i].info, std::string(deviceName) );
				delete[] deviceName; deviceName=nullptr;
			}
		}



		// Pass context pointer back to caller.
		*out_context = context;

		s_logger.Trace__NewFrObjectCreated(Logger::TRACE_ADDRESS_TYPE__CONTEXT, *out_context);
		s_logger.TraceArg_Prepare__rpr_tahoePluginIDlist(pluginIDs,pluginCount);
		s_logger.Trace__DeclareFRobject("rpr_context","context_0x", *out_context);
		s_logger.printTrace("status = ");
		TRACE__FUNCTION_OPEN;
		if (in_api_version == RPR_API_VERSION)
		{
			s_logger.printTrace("(rpr_int)RPR_API_VERSION,");
		}
		else
		{
			s_logger.TraceArg__rpr_int_hexa(in_api_version); s_logger.TraceArg__COMMA();
		}
		s_logger.TraceArg_Use__rpr_tahoePluginIDlist(pluginIDs); s_logger.TraceArg__COMMA();
		s_logger.TraceArg__size_t(pluginCount); s_logger.TraceArg__COMMA();
		s_logger.TraceArg__rpr_creation_flags(in_creation_flags); s_logger.TraceArg__COMMA();
		s_logger.TraceArg_Use__rpr_context_properties_P(in_props); s_logger.TraceArg__COMMA();
		//s_logger.TraceArg__rpr_char_P(cache_path); s_logger.TraceArg__COMMA();
		s_logger.printTrace("0,");
		s_logger.printTrace("&context_0x%p", *out_context);
		s_logger.printTrace(");\r\n");
		s_logger.printTrace("#else\r\n");
		s_logger.TraceArg_Prepare__rpr_tahoePluginIDlist(pluginIDs,pluginCount); // TraceArg_Prepare__rpr_tahoePluginIDlist 2 times, because we call TraceArg_Use__rpr_tahoePluginIDlist 2 times
		s_logger.printTrace("status = ");
		TRACE__FUNCTION_OPEN;
		s_logger.TraceArg__rpr_int_hexa(in_api_version); s_logger.TraceArg__COMMA();
		s_logger.TraceArg_Use__rpr_tahoePluginIDlist(pluginIDs); s_logger.TraceArg__COMMA();
		s_logger.TraceArg__size_t(pluginCount); s_logger.TraceArg__COMMA();
		s_logger.TraceArg__rpr_creation_flags(in_creation_flags); s_logger.TraceArg__COMMA();
		s_logger.TraceArg_Use__rpr_context_properties_P(in_props); s_logger.TraceArg__COMMA();
		s_logger.TraceArg__rpr_char_P(in_cache_path); s_logger.TraceArg__COMMA();
		s_logger.printTrace("&context_0x%p", *out_context);
		s_logger.printTrace(");\r\n");
		s_logger.printTrace("#endif\r\nRPRTRACE_CHECK\r\n\r\n");
		s_logger.Trace__FlushAllFiles();

		return RPR_SUCCESS;
	}
	catch (FrException& e)
	{
		s_logger.printTrace("#endif\r\nRPRTRACE_CHECK\r\n");

		//in case of fail, it could be useful to have the input argument - for debugging
		s_logger.printTrace("// arguments: (");
		s_logger.TraceArg__rpr_int_hexa(in_api_version); s_logger.TraceArg__COMMA();
		s_logger.TraceArg_Use__rpr_tahoePluginIDlist(pluginIDs); s_logger.TraceArg__COMMA();
		s_logger.TraceArg__size_t(pluginCount); s_logger.TraceArg__COMMA();
		s_logger.TraceArg__rpr_creation_flags(in_creation_flags); s_logger.TraceArg__COMMA();
		s_logger.TraceArg_Use__rpr_context_properties_P(in_props); s_logger.TraceArg__COMMA();
		s_logger.TraceArg__rpr_char_P(in_cache_path); s_logger.TraceArg__COMMA();
		s_logger.printTrace(" out_context");
		s_logger.printTrace(");\r\n\r\n");

		TRACE__FUNCTION_FAILED(0); return e.GetErrorCode();
	}
}

rpr_int rprContextRender(rpr_context context)
{
	s_logger.printTrace("status = ");
	TRACE__FUNCTION_OPEN
	s_logger.TraceArg__rpr_context(context);
	s_logger.Trace__FunctionClose();
	s_logger.Trace__CommentPossibilityExport_contex(context);

	API_PRINT("rprContextRender", 1, context);
	FrNode* ctx = static_cast<FrNode*>(context);

	try
	{
		CHECK_NOT_NULL(ctx);
		MACRO__CHECK_ARGUMENT_TYPE(context,Context);

		std::shared_ptr<FrRendererEncalps> renderer = ctx->GetProperty< std::shared_ptr<FrRendererEncalps> >(RPR_CONTEXT_ACTIVE_PLUGIN);
		
		if (!renderer->m_FrRenderer) 
			throw FrException(__FILE__,__LINE__,RPR_ERROR_INVALID_PARAMETER, "No active compute API set",context);

		renderer->m_FrRenderer->Render();
		return RPR_SUCCESS;
	}
	catch (FrException& e)
	{
		TRACE__FUNCTION_FAILED(context); 
		return e.GetErrorCode();
	}
}

rpr_int rprContextRenderTile(rpr_context context, rpr_uint xmin, rpr_uint xmax, rpr_uint ymin, rpr_uint ymax)
{
	s_logger.printTrace("status = ");
	TRACE__FUNCTION_OPEN
	s_logger.TraceArg__rpr_context(context); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_uint(xmin); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_uint(xmax); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_uint(ymin); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_uint(ymax);
	s_logger.Trace__FunctionClose();
	s_logger.printTrace("//rprFrameBufferSaveToFile(framebuffer____, \"img_0001.png\"); // <-- uncomment if you want export image\r\n\r\n");

	API_PRINT("rprContextRenderTile", 1, context);
	FrNode* ctx = static_cast<FrNode*>(context);

	try
	{
		CHECK_NOT_NULL(ctx);
		MACRO__CHECK_ARGUMENT_TYPE(context,Context);

		std::shared_ptr<FrRendererEncalps> renderer = ctx->GetProperty<  std::shared_ptr<FrRendererEncalps>  >(RPR_CONTEXT_ACTIVE_PLUGIN);
		if (!renderer->m_FrRenderer) throw FrException(__FILE__,__LINE__,RPR_ERROR_INVALID_PARAMETER, "No active compute API set",context);
		renderer->m_FrRenderer->RenderTile(xmin, xmax, ymin, ymax);
		return RPR_SUCCESS;
	}
	catch (FrException& e)
	{
		TRACE__FUNCTION_FAILED(context); return e.GetErrorCode();
	}
}

rpr_int rprContextCreateImage(rpr_context context, const rpr_image_format format, const rpr_image_desc* image_desc, const void* data, rpr_image* out_image)
{
	//in case of issues during creation, it's safer to return a null pointer, rather than let the pointer undefined.
	if ( out_image )
		*out_image = nullptr;
	



	int comp_size = 4;
	switch (format.type)
	{
	case RPR_COMPONENT_TYPE_UINT8:
		comp_size = 1;
		break;
	case RPR_COMPONENT_TYPE_FLOAT16:
		comp_size = 2;
		break;
	case RPR_COMPONENT_TYPE_FLOAT32:
		comp_size = 4;
		break;
	}

	// Todo : improve management of image_depth,image_row_pitch,image_slice_pitch
	if (s_logger.IsTraceActivated())
	{
		unsigned long long trace_dataSize = 0;

		s_logger.TraceArg_Prepare__rpr_image_format(format);
		s_logger.TraceArg_Prepare__rpr_image_desc_P(image_desc);


		rpr_uint trace_rowpitch = 0;
		if	( image_desc->image_row_pitch == 0 ) 
		{ 
			//compute the row pitch
			trace_rowpitch = image_desc->image_width * format.num_components * comp_size; 
		}
		else if ( image_desc->image_row_pitch  < image_desc->image_width * format.num_components * comp_size )
		{
			//if the pitch is too small : error
			s_logger.printTrace("#error : WRONG ROW PITCH: image_depth=%d image_row_pitch=%d image_slice_pitch=%d\r\n",image_desc->image_depth,image_desc->image_row_pitch, image_desc->image_slice_pitch);
		}
		else 
		{ 
			//if the pitch has a padding
			trace_rowpitch = image_desc->image_row_pitch; 
		}

		rpr_uint trace_slicepitch = 0;
		if ( image_desc->image_slice_pitch == 0 ) 
		{ 
			//compute the slice pitch
			trace_slicepitch = trace_rowpitch * image_desc->image_height; 
		}
		else if ( image_desc->image_slice_pitch  < trace_rowpitch * image_desc->image_height )
		{
			//if the pitch is too small : error
			s_logger.printTrace("#error : WRONG SLICE PITCH: image_depth=%d image_row_pitch=%d image_slice_pitch=%d\r\n",image_desc->image_depth,image_desc->image_row_pitch, image_desc->image_slice_pitch);
		}
		else 
		{ 
			//if the pitch has a padding
			trace_slicepitch = image_desc->image_slice_pitch; 
		}

		trace_dataSize = trace_slicepitch;

		s_logger.TraceArg_Prepare__DATA(data, trace_dataSize, "pData1");
		s_logger.printTrace("//Image creation\r\n");
		s_logger.printTrace("//size in data file for this image: %llu\r\n", trace_dataSize);
		s_logger.Trace__FlushAllFiles(); // in case a crash, ensure trace informs the creation
	}

	API_PRINT("rprContextCreateImage", 1, context);
	FrNode* ctx = static_cast<FrNode*>(context);

	try
	{
		CHECK_NOT_NULL(ctx);
		MACRO__CHECK_ARGUMENT_TYPE(context,Context);


		// Make a copy of the data.

		size_t dataSize = 0;
		if ( image_desc->image_depth == 0 )
			dataSize = image_desc->image_width * image_desc->image_height							 * format.num_components * comp_size;
		else
			dataSize = image_desc->image_width * image_desc->image_height * image_desc->image_depth * format.num_components * comp_size;

		if ( dataSize == 0 )
		{
			throw FrException(__FILE__,__LINE__,RPR_ERROR_INVALID_PARAMETER, "Invalid image size",context);
		}

		std::shared_ptr<unsigned char> sharedDataPtr(new unsigned char[dataSize], std::default_delete<unsigned char[]>());
		memcpy(sharedDataPtr.get(), data, dataSize);

		// Create the image node and set its properties.
		std::shared_ptr<FrSceneGraph> sceneGraph = ctx->GetProperty<std::shared_ptr<FrSceneGraph>>(FR_SCENEGRAPH);
		std::shared_ptr<FrRendererEncalps> renderer = ctx->GetProperty< std::shared_ptr<FrRendererEncalps> >(RPR_CONTEXT_ACTIVE_PLUGIN);
		FrNode* node = sceneGraph->CreateNode(NodeTypes::Image, [&](FrNode* n) {
			n->SetProperty(FR_NODE_CONTEXT, ctx);
			n->SetProperty(RPR_IMAGE_FORMAT, format);
			n->SetProperty(RPR_IMAGE_DESC, *image_desc);
			n->SetProperty(RPR_IMAGE_DATA, std::move(sharedDataPtr));
			n->SetProperty(RPR_IMAGE_DATA_SIZEBYTE, dataSize);
            n->AddProperty(FR_SCENEGRAPH, sceneGraph);
			n->AddProperty(RPR_CONTEXT_ACTIVE_PLUGIN, renderer);
		});

		//if RPR doesn't keep image DATA, then release the pointer
		#ifdef RPR_DOESNT_KEEP_TEXTURE_DATA
		auto& imgData = node->GetProperty< std::shared_ptr<unsigned char> >(RPR_IMAGE_DATA);
		imgData.reset();
		#endif


		*out_image = node;

		if (s_logger.IsTraceActivated())
		{
			s_logger.Trace__NewFrObjectCreated(Logger::TRACE_ADDRESS_TYPE__IMAGE, *out_image);
			s_logger.Trace__DeclareFRobject("rpr_image", "image_0x", *out_image);
			s_logger.printTrace("status = ");
			TRACE__FUNCTION_OPEN
			s_logger.TraceArg__rpr_context(context); s_logger.TraceArg__COMMA();
			s_logger.TraceArg_Use__rpr_image_format(format); s_logger.TraceArg__COMMA();
			s_logger.TraceArg_Use__rpr_image_desc_P(image_desc); s_logger.TraceArg__COMMA();
			s_logger.TraceArg_Use__DATA_const_void_P("pData1"); s_logger.TraceArg__COMMA();
			s_logger.printTrace("&image_0x%p", *out_image);
			s_logger.Trace__FunctionClose();
			s_logger.printTrace("status = rprObjectSetName (image_0x%p,\"image_0x%p\"); RPRTRACE_CHECK// added by tracing for debug\r\n",  *out_image ,  *out_image);
		}

		return RPR_SUCCESS;
	}
	catch (FrException& e)
	{
		TRACE__FUNCTION_FAILED(context); return e.GetErrorCode();
	}
}

rpr_int rprContextCreateBuffer(rpr_context context, const rpr_buffer_desc* buffer_desc, const void* data, rpr_buffer* out_buffer)
{
	//in case of issues during creation, it's safer to return a null pointer, rather than let the pointer undefined.
	if ( out_buffer )
		*out_buffer = nullptr;
	

	API_PRINT("rprContextCreateBuffer", 1, context);

	try
	{
		MACRO__CHECK_ARGUMENT_TYPE(context,Context);

		if ( buffer_desc == nullptr || data == nullptr || out_buffer == nullptr )
		{
			throw FrException(__FILE__,__LINE__,RPR_ERROR_INVALID_PARAMETER, "null argument",context);
		}

		int sizeOneComponent = 0;
		if ( buffer_desc->element_type == RPR_BUFFER_ELEMENT_TYPE_INT32 )
		{
			sizeOneComponent = 4;
		}
		else if ( buffer_desc->element_type == RPR_BUFFER_ELEMENT_TYPE_FLOAT32 )
		{
			sizeOneComponent = 4;
		}
		else
		{
			throw FrException(__FILE__,__LINE__,RPR_ERROR_INVALID_PARAMETER, "Invalid element_type",context);
		}

		if (s_logger.IsTraceActivated())
		{
		
			s_logger.TraceArg_Prepare__rpr_buffer_desc_P(buffer_desc);

			unsigned long long bufferSizeByte = buffer_desc->element_channel_size * sizeOneComponent * buffer_desc->nb_element;

			s_logger.TraceArg_Prepare__DATA(data, bufferSizeByte, "pData1");
			s_logger.printTrace("//Buffer creation\r\n");
			s_logger.printTrace("//size in data file for this buffer: %llu\r\n", bufferSizeByte);
			s_logger.Trace__FlushAllFiles(); // in case a crash, ensure trace informs the creation
		}

		
		FrNode* ctx = static_cast<FrNode*>(context);
		CHECK_NOT_NULL(ctx);


		// Make a copy of the data.
		size_t dataSizeByte = buffer_desc->nb_element * buffer_desc->element_channel_size * sizeOneComponent;

		if ( dataSizeByte == 0 )
		{
			throw FrException(__FILE__,__LINE__,RPR_ERROR_INVALID_PARAMETER, "Invalid buffer size",context);
		}

		std::shared_ptr<unsigned char> sharedDataPtr(new unsigned char[dataSizeByte], std::default_delete<unsigned char[]>());
		memcpy(sharedDataPtr.get(), data, dataSizeByte);

		// Create the buffer node and set its properties.
		std::shared_ptr<FrSceneGraph> sceneGraph = ctx->GetProperty<std::shared_ptr<FrSceneGraph>>(FR_SCENEGRAPH);
		std::shared_ptr<FrRendererEncalps> renderer = ctx->GetProperty< std::shared_ptr<FrRendererEncalps> >(RPR_CONTEXT_ACTIVE_PLUGIN);
		FrNode* node = sceneGraph->CreateNode(NodeTypes::Buffer, [&](FrNode* n) {
			n->SetProperty(FR_NODE_CONTEXT, ctx);
			n->SetProperty(RPR_BUFFER_DATA, std::move(sharedDataPtr));
			n->SetProperty(RPR_BUFFER_DESC, *buffer_desc);
            n->AddProperty(FR_SCENEGRAPH, sceneGraph);
			n->AddProperty(RPR_CONTEXT_ACTIVE_PLUGIN, renderer);
		});

		*out_buffer = node;

		if (s_logger.IsTraceActivated())
		{
			s_logger.Trace__NewFrObjectCreated(Logger::TRACE_ADDRESS_TYPE__BUFFER, *out_buffer);
			s_logger.Trace__DeclareFRobject("rpr_buffer", "buffer_0x", *out_buffer);
			s_logger.printTrace("status = ");
			TRACE__FUNCTION_OPEN
			s_logger.TraceArg__rpr_context(context); s_logger.TraceArg__COMMA();
			s_logger.TraceArg_Use__rpr_buffer_desc_P(buffer_desc); s_logger.TraceArg__COMMA();
			s_logger.TraceArg_Use__DATA_const_void_P("pData1"); s_logger.TraceArg__COMMA();
			s_logger.printTrace("&buffer_0x%p", *out_buffer);
			s_logger.Trace__FunctionClose();
			s_logger.printTrace("status = rprObjectSetName (buffer_0x%p,\"buffer_0x%p\"); RPRTRACE_CHECK// added by tracing for debug\r\n",  *out_buffer ,  *out_buffer);
		}

		return RPR_SUCCESS;
	}
	catch (FrException& e)
	{
		TRACE__FUNCTION_FAILED(context); return e.GetErrorCode();
	}
}


rpr_int rprContextCreateFrameBuffer(rpr_context context, const rpr_framebuffer_format format, const rpr_framebuffer_desc* fb_desc, rpr_framebuffer* out_fb)
{

	//in case of issues during creation, it's safer to return a null pointer, rather than let the pointer undefined.
	if ( out_fb )
		*out_fb = nullptr;
	

	s_logger.TraceArg_Prepare__rpr_framebuffer_format(format);
	s_logger.TraceArg_Prepare__rpr_framebuffer_desc_P(fb_desc);
	s_logger.printTrace("//FrameBuffer creation\r\n");
	s_logger.Trace__FlushAllFiles(); // in case a crash, ensure trace informs the creation

	API_PRINT("rprContextCreateFrameBuffer", 1, context);
	FrNode* ctx = static_cast<FrNode*>(context);

	try
	{
		CHECK_NOT_NULL(ctx);
		MACRO__CHECK_ARGUMENT_TYPE(context,Context);


		std::shared_ptr<FrSceneGraph> sceneGraph = ctx->GetProperty< std::shared_ptr<FrSceneGraph> >(FR_SCENEGRAPH);
		std::shared_ptr<FrRendererEncalps> renderer = ctx->GetProperty< std::shared_ptr<FrRendererEncalps> >(RPR_CONTEXT_ACTIVE_PLUGIN);
		FrNode* node = sceneGraph->CreateNode(NodeTypes::FrameBuffer, [&](FrNode* n) {
			n->SetProperty(FR_NODE_CONTEXT, ctx);
			n->SetProperty(RPR_FRAMEBUFFER_FORMAT, format);
			n->SetProperty(RPR_FRAMEBUFFER_DESC, *fb_desc);
            n->AddProperty(FR_SCENEGRAPH, sceneGraph);
			n->AddProperty(RPR_CONTEXT_ACTIVE_PLUGIN, renderer);
		});

		*out_fb = node;

		s_logger.Trace__NewFrObjectCreated(Logger::TRACE_ADDRESS_TYPE__FRAMEBUFFER, *out_fb);
		s_logger.Trace__DeclareFRobject("rpr_framebuffer", "framebuffer_0x", *out_fb);
		s_logger.printTrace("status = ");
		TRACE__FUNCTION_OPEN
		s_logger.TraceArg__rpr_context(context); s_logger.TraceArg__COMMA();
		s_logger.TraceArg_Use__rpr_framebuffer_format(format); s_logger.TraceArg__COMMA();
		s_logger.TraceArg_Use__rpr_framebuffer_desc_P(fb_desc); s_logger.TraceArg__COMMA();
		s_logger.printTrace("&framebuffer_0x%p", *out_fb);
		s_logger.Trace__FunctionClose();
		s_logger.printTrace("status = rprObjectSetName (framebuffer_0x%p,\"framebuffer_0x%p\"); RPRTRACE_CHECK// added by tracing for debug\r\n",  *out_fb ,  *out_fb);

		//in Tahoe, the initial default value of a frame buffer is undefined.
		//so it's better that RPR guarantees a cleaned frame buffer when created
		renderer->m_FrRenderer->FrameBufferClear(node);

		return RPR_SUCCESS;
	}
	catch (FrException& e)
	{
		TRACE__FUNCTION_FAILED(context); return e.GetErrorCode();
	}
}

rpr_int rprContextCreateCamera(rpr_context context, rpr_camera* out_camera)
{
	//in case of issues during creation, it's safer to return a null pointer, rather than let the pointer undefined.
	if ( out_camera )
		*out_camera = nullptr;
	

	s_logger.printTrace("//Camera creation\r\n");
	s_logger.Trace__FlushAllFiles(); // in case a crash, ensure trace informs the creation

	API_PRINT("rprContextCreateCamera", 1, context);
	FrNode* ctx = static_cast<FrNode*>(context);

	try
	{
		CHECK_NOT_NULL(ctx);
		MACRO__CHECK_ARGUMENT_TYPE(context,Context);


		std::shared_ptr<FrSceneGraph> sceneGraph = ctx->GetProperty< std::shared_ptr<FrSceneGraph> >(FR_SCENEGRAPH);
		std::shared_ptr<FrRendererEncalps> renderer = ctx->GetProperty< std::shared_ptr<FrRendererEncalps> >(RPR_CONTEXT_ACTIVE_PLUGIN);
		FrNode* node = sceneGraph->CreateNode(NodeTypes::Camera, [&](FrNode* n) {
			n->SetProperty(FR_NODE_CONTEXT, ctx);
            n->AddProperty(FR_SCENEGRAPH, sceneGraph);
			n->AddProperty(RPR_CONTEXT_ACTIVE_PLUGIN, renderer);
		});

		*out_camera = node;

		s_logger.Trace__NewFrObjectCreated(Logger::TRACE_ADDRESS_TYPE__CAMERA, *out_camera);
		s_logger.Trace__DeclareFRobject("rpr_camera", "camera_0x", *out_camera);
		s_logger.printTrace("status = ");
		TRACE__FUNCTION_OPEN
		s_logger.TraceArg__rpr_context(context); s_logger.TraceArg__COMMA();
		s_logger.printTrace("&camera_0x%p", *out_camera);
		s_logger.Trace__FunctionClose();
		s_logger.printTrace("status = rprObjectSetName (camera_0x%p,\"camera_0x%p\"); RPRTRACE_CHECK// added by tracing for debug\r\n",  *out_camera ,  *out_camera);

		return RPR_SUCCESS;
	}
	catch (FrException& e)
	{
		TRACE__FUNCTION_FAILED(context); return e.GetErrorCode();
	}
}

rpr_int rprContextSetScene(rpr_context context, rpr_scene scene)
{
	s_logger.printTrace("status = ");
	TRACE__FUNCTION_OPEN;
	s_logger.TraceArg__rpr_context(context); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_scene(scene);
	s_logger.Trace__FunctionClose();

	API_PRINT("rprContextSetScene", 1, context);
	FrNode* ctx = static_cast<FrNode*>(context);

	try
	{
		CHECK_NOT_NULL(ctx);
		MACRO__CHECK_ARGUMENT_TYPE(context,Context);
		MACRO__CHECK_ARGUMENT_TYPE(scene,Scene);
		ctx->SetProperty(RPR_CONTEXT_SCENE, static_cast<FrNode*>(scene) , static_cast<FrNode*>(scene) );
		//ctx->PropertyChanged(RPR_CONTEXT_SCENE, static_cast<FrNode*>(scene) );
		return RPR_SUCCESS;
	}
	catch (FrException& e)
	{
		TRACE__FUNCTION_FAILED(context); return e.GetErrorCode();
	}

	return RPR_SUCCESS;
}

rpr_int rprContextSetActivePlugin(rpr_context context, rpr_int pluginID)
{

	s_logger.printTrace("status = ");
	TRACE__FUNCTION_OPEN
	s_logger.TraceArg__rpr_context(context); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_tahoePluginID(pluginID);
	s_logger.Trace__FunctionClose();

	API_PRINT("rprContextSetActivePlugin", 1, context);
	FrNode* ctx = static_cast<FrNode*>(context);

	try
	{
		CHECK_NOT_NULL(ctx);
		MACRO__CHECK_ARGUMENT_TYPE(context,Context);


		auto& pluginList = ctx->GetProperty<std::map<rpr_int,  std::shared_ptr<FrRendererEncalps>  >>(FR_CONTEXT_PLUGIN_LIST);
		auto itr = pluginList.find(pluginID);
		if (itr != pluginList.end())
			ctx->SetProperty(RPR_CONTEXT_ACTIVE_PLUGIN, itr->second);
		else
			throw FrException(__FILE__,__LINE__,RPR_ERROR_INVALID_PARAMETER, "Invalid plugin ID",context);
	}
	catch (FrException& e)
	{
		TRACE__FUNCTION_FAILED(context); return e.GetErrorCode();
	}

	return RPR_SUCCESS;
}

rpr_int rprContextGetInfo(rpr_context context, rpr_context_info context_info, size_t size, void* data, size_t* size_ret)
{
	s_logger.printTrace("//"); //  ignore getInfo for the trace
	TRACE__FUNCTION_OPEN;
	s_logger.Trace__FunctionClose();

	API_PRINT("rprContextGetInfo", 1, context);
	FrNode* ctx = static_cast<FrNode*>(context);

	try
	{
		CHECK_NOT_NULL(ctx);
		MACRO__CHECK_ARGUMENT_TYPE(context,Context);


		size_t requiredSize = 0;
		switch (context_info)
		{
		case RPR_CONTEXT_CREATION_FLAGS:
			requiredSize = sizeof(rpr_creation_flags);
			break;

		case RPR_CONTEXT_CACHE_PATH:
		{
			std::string name = ctx->GetProperty<std::string>(RPR_CONTEXT_CACHE_PATH);
			requiredSize = name.size() + 1;
			break;
		}

		case RPR_CONTEXT_CPU_THREAD_LIMIT:
		case RPR_CONTEXT_API_VERSION:
			requiredSize = sizeof(rpr_uint);
			break;

		case RPR_CONTEXT_RENDER_STATUS:
			requiredSize = sizeof(rpr_uint);
			break;

		case RPR_CONTEXT_DEVICE_COUNT:
			requiredSize = sizeof(rpr_uint);
			break;

		case RPR_CONTEXT_ACTIVE_PLUGIN:
			throw FrException(__FILE__,__LINE__,RPR_ERROR_UNIMPLEMENTED, "",ctx);
			break;

		case RPR_CONTEXT_SCENE:
			requiredSize = sizeof(rpr_scene);
			break;

		case RPR_CONTEXT_PARAMETER_COUNT:
			requiredSize = sizeof(size_t);
			break;

		case RPR_CONTEXT_RENDER_STATISTICS:
			requiredSize = sizeof(rpr_render_statistics);
			break;

		case RPR_OBJECT_NAME:
		{
			std::string name = ctx->GetProperty<std::string>(RPR_OBJECT_NAME);
			requiredSize = name.size() + 1;
			break;
		}

		default:
		{
			//check if the parameter is in ContextParameterDescriptions
			if ( ContextParameterDescriptions.find(context_info) != ContextParameterDescriptions.end() )
			{
				const ParameterDesc& paramDesc = ContextParameterDescriptions[context_info];
					 if ( paramDesc.type == RPR_PARAMETER_TYPE_FLOAT ) { requiredSize = sizeof(rpr_float); }
				else if ( paramDesc.type == RPR_PARAMETER_TYPE_FLOAT2 ) { requiredSize = sizeof(rpr_float2); }
				else if ( paramDesc.type == RPR_PARAMETER_TYPE_FLOAT3 ) { requiredSize = sizeof(rpr_float3); }
				else if ( paramDesc.type == RPR_PARAMETER_TYPE_FLOAT4 ) { requiredSize = sizeof(rpr_float4); }
				else if ( paramDesc.type == RPR_PARAMETER_TYPE_UINT ) { requiredSize = sizeof(rpr_uint); }
				else if ( paramDesc.type == RPR_PARAMETER_TYPE_STRING )
				{
					std::string str = ctx->GetProperty<std::string>(context_info);
					requiredSize = str.length() + 1;
				}
				else 
				{ 
					throw FrException(__FILE__,__LINE__,RPR_ERROR_INVALID_PARAMETER, "",ctx);
				}
			}
			else
			{
				std::shared_ptr<FrRendererEncalps> renderer = ctx->GetProperty<  std::shared_ptr<FrRendererEncalps>   >(RPR_CONTEXT_ACTIVE_PLUGIN);
				requiredSize = renderer->m_FrRenderer->GetInfoSize(context_info);
			}
			break;
		}

		}

		if (data)
		{
			if (size >= requiredSize)
			{
				switch (context_info)
				{

				case RPR_CONTEXT_CREATION_FLAGS :
					*static_cast<rpr_creation_flags*>(data) = ctx->GetProperty<rpr_creation_flags>(RPR_CONTEXT_CREATION_FLAGS);
					break;
					
				case RPR_CONTEXT_CACHE_PATH :
				{
					std::string name = ctx->GetProperty<std::string>(RPR_CONTEXT_CACHE_PATH);
					memcpy(data, name.c_str(), name.length());
					reinterpret_cast<char*>(data)[name.length()] = '\0';
					break;
				}

				case RPR_CONTEXT_CPU_THREAD_LIMIT:
					*static_cast<rpr_uint*>(data) = ctx->GetProperty<rpr_uint>(RPR_CONTEXT_CPU_THREAD_LIMIT);
					break;

				case RPR_CONTEXT_API_VERSION:
					*static_cast<rpr_uint*>(data) = ctx->GetProperty<rpr_uint>(RPR_CONTEXT_API_VERSION);
					break;

				case RPR_CONTEXT_RENDER_STATUS :
					*static_cast<rpr_uint*>(data) = ctx->GetProperty<rpr_uint>(RPR_CONTEXT_RENDER_STATUS);
					break;

				case RPR_CONTEXT_DEVICE_COUNT  :
					*static_cast<rpr_uint*>(data) = ctx->GetProperty<rpr_uint>(RPR_CONTEXT_DEVICE_COUNT);
					break;

				case RPR_CONTEXT_ACTIVE_PLUGIN :
					throw FrException(__FILE__,__LINE__,RPR_ERROR_UNIMPLEMENTED, "",ctx);
					break;

				case RPR_CONTEXT_SCENE :
					*static_cast<rpr_scene*>(data) = ctx->GetProperty<FrNode*>(RPR_CONTEXT_SCENE);
					break;

				case RPR_CONTEXT_PARAMETER_COUNT:
					*static_cast<size_t*>(data) = ctx->GetProperty<rpr_uint>(RPR_CONTEXT_PARAMETER_COUNT);
					break;

				case RPR_CONTEXT_RENDER_STATISTICS:
				{
					rpr_render_statistics stat;
					std::shared_ptr<FrRendererEncalps> renderer = ctx->GetProperty<  std::shared_ptr<FrRendererEncalps>  >(RPR_CONTEXT_ACTIVE_PLUGIN);
					if (!renderer->m_FrRenderer) throw FrException(__FILE__,__LINE__,RPR_ERROR_INVALID_PARAMETER, "No active compute API set",context);
					renderer->m_FrRenderer->GetRenderStatistics(stat);
					*static_cast<rpr_render_statistics*>(data) = stat;
					break;
				}
				case RPR_OBJECT_NAME:
				{
					std::string name = ctx->GetProperty<std::string>(RPR_OBJECT_NAME);
					auto len = name.size();
					auto str = reinterpret_cast<rpr_char*>(data);
					std::copy(name.cbegin(), name.cend(), str);
					*(str + len) = 0;
					break;
				}
				default:
				{

					//check if the parameter is in ContextParameterDescriptions
					if ( ContextParameterDescriptions.find(context_info) != ContextParameterDescriptions.end() )
					{
						const ParameterDesc& paramDesc = ContextParameterDescriptions[context_info];
						if ( paramDesc.type == RPR_PARAMETER_TYPE_FLOAT ) 
						{ 
							*reinterpret_cast<rpr_float*>(data) = ctx->GetProperty<rpr_float>(context_info);
						}
						else if ( paramDesc.type == RPR_PARAMETER_TYPE_FLOAT2 ) 
						{ 
							rpr_float* dest = reinterpret_cast<rpr_float*>(data);
							rpr_float2 src = ctx->GetProperty<rpr_float2>(context_info);
							*dest++ = src.x;
							*dest++ = src.y;
						}
						else if ( paramDesc.type == RPR_PARAMETER_TYPE_FLOAT3 ) 
						{ 
							rpr_float* dest = reinterpret_cast<rpr_float*>(data);
							rpr_float3 src = ctx->GetProperty<rpr_float3>(context_info);
							*dest++ = src.x;
							*dest++ = src.y;
							*dest++ = src.z;
						}
						else if ( paramDesc.type == RPR_PARAMETER_TYPE_FLOAT4 ) 
						{ 
							rpr_float* dest = reinterpret_cast<rpr_float*>(data);
							rpr_float4 src = ctx->GetProperty<rpr_float4>(context_info);
							*dest++ = src.x;
							*dest++ = src.y;
							*dest++ = src.z;
							*dest++ = src.w;
						}
						else if ( paramDesc.type == RPR_PARAMETER_TYPE_UINT ) 
						{ 
							*reinterpret_cast<rpr_uint*>(data) = ctx->GetProperty<rpr_uint>(context_info);
						}
						else if ( paramDesc.type == RPR_PARAMETER_TYPE_STRING ) 
						{ 
							std::string name = ctx->GetProperty<std::string>(context_info);
							auto len = name.size();
							auto str = reinterpret_cast<rpr_char*>(data);
							std::copy(name.cbegin(), name.cend(), str);
							*(str + len) = 0;

							if (  
								len == 0 && (
								   context_info == RPR_CONTEXT_GPU0_NAME
								|| context_info == RPR_CONTEXT_GPU1_NAME
								|| context_info == RPR_CONTEXT_GPU2_NAME
								|| context_info == RPR_CONTEXT_GPU3_NAME
								|| context_info == RPR_CONTEXT_GPU4_NAME
								|| context_info == RPR_CONTEXT_GPU5_NAME
								|| context_info == RPR_CONTEXT_GPU6_NAME
								|| context_info == RPR_CONTEXT_GPU7_NAME
								|| context_info == RPR_CONTEXT_CPU_NAME )
								)
							{
								//if the name of a device is empty, this means that the device is not included inside the context
								//for more clarity for the user, it's better to return an error, instead of just returning an empty string
								throw FrException(__FILE__,__LINE__,RPR_ERROR_UNSUPPORTED, "",ctx);
							}
							
						}
						else 
						{ 
							throw FrException(__FILE__,__LINE__,RPR_ERROR_INVALID_PARAMETER, "",ctx);
						}
					}
					else
					{
						std::shared_ptr<FrRendererEncalps> renderer = ctx->GetProperty<  std::shared_ptr<FrRendererEncalps>  >(RPR_CONTEXT_ACTIVE_PLUGIN);
						renderer->m_FrRenderer->GetInfo(context_info, data);
					}

					break;
				}
				}
			}
			else
			{
				throw FrException(__FILE__,__LINE__,RPR_ERROR_INVALID_PARAMETER, "",ctx);
			}
		}

		if (size_ret)
			*size_ret = requiredSize;
	}
	catch (FrException& e)
	{
		TRACE__FUNCTION_FAILED(context); return e.GetErrorCode();
	}

	return RPR_SUCCESS;
}

rpr_int rprContextGetParameterInfo(rpr_context context, int param_idx, rpr_parameter_info parameter_info, size_t size, void* data, size_t* size_ret)
{
	s_logger.printTrace("//"); //  ignore getInfo for the trace
	TRACE__FUNCTION_OPEN;
	s_logger.Trace__FunctionClose();

	API_PRINT("rprContextGetParameterInfo", 1, context);
	FrNode* ctx = static_cast<FrNode*>(context);

	try
	{
		CHECK_NOT_NULL(ctx);
		MACRO__CHECK_ARGUMENT_TYPE(context,Context);


		int propIndex = 0;
		int32_t propertyKey = -1;
		for (int i = 0; i < ctx->GetNumProperties(); ++i)
		{
			uint32_t key = ctx->GetPropertyKey(i);
			if (key >= RPR_CONTEXT_AA_CELL_SIZE && key <= RPR_CONTEXT_MAX-1)
			{
				if (propIndex == param_idx)
				{
					propertyKey = key;
					break;
				}

				++propIndex;
			}
		}

		if (propertyKey == -1)
			throw FrException(__FILE__,__LINE__,RPR_ERROR_INVALID_PARAMETER, "",ctx);

		const ParameterDesc& paramDesc = ContextParameterDescriptions[propertyKey];

		size_t requiredSize = 0;
		switch (parameter_info)
		{
		case RPR_PARAMETER_TYPE:
			requiredSize = sizeof(rpr_parameter_type);
			break;

		case RPR_PARAMETER_NAME:
			requiredSize = sizeof(rpr_context_info);
			break;

		case RPR_PARAMETER_NAME_STRING:
			requiredSize = paramDesc.name.size() + 1;
			break;

		case RPR_PARAMETER_DESCRIPTION:
			requiredSize = sizeof(rpr_char)* (paramDesc.desc.size() + 1);
			break;

		case RPR_PARAMETER_VALUE:
			switch (paramDesc.type)
			{
			case RPR_PARAMETER_TYPE_FLOAT:
				requiredSize = sizeof(rpr_float);
				break;

			case RPR_PARAMETER_TYPE_FLOAT3:
				requiredSize = sizeof(rpr_float3);
				break;

			case RPR_PARAMETER_TYPE_FLOAT4:
				requiredSize = sizeof(rpr_float4);
				break;

			case RPR_PARAMETER_TYPE_UINT:
				requiredSize = sizeof(rpr_uint);
				break;

			case RPR_PARAMETER_TYPE_STRING:
			{
				std::string paramStr = ctx->GetProperty<std::string>(propertyKey);
				requiredSize = paramStr.length() + 1;
				break;
			}
			}
			break;
		}

		if (data)
		{
			if (size >= requiredSize)
			{
				switch (parameter_info)
				{
				case RPR_PARAMETER_TYPE:
					*reinterpret_cast<rpr_parameter_type*>(data) = paramDesc.type;
					break;

				case RPR_PARAMETER_NAME:
					*reinterpret_cast<rpr_context_info*>(data) = propertyKey;
					break;

				case RPR_PARAMETER_NAME_STRING:
					memcpy(data, paramDesc.name.c_str(), paramDesc.name.length());
					reinterpret_cast<char*>(data)[paramDesc.name.length()] = '\0';
					break;

				case RPR_PARAMETER_DESCRIPTION:
				{
					rpr_char* dest = reinterpret_cast<rpr_char*>(data);
					memcpy(dest, paramDesc.desc.c_str(), paramDesc.desc.size());
					*(dest + paramDesc.desc.size()) = '\0';
					break;
				}

				case RPR_PARAMETER_VALUE:
					switch (paramDesc.type)
					{
					case RPR_PARAMETER_TYPE_FLOAT:
						*reinterpret_cast<rpr_float*>(data) = ctx->GetProperty<rpr_float>(propertyKey);
						break;

					case RPR_PARAMETER_TYPE_FLOAT3:
					{
						rpr_float* dest = reinterpret_cast<rpr_float*>(data);
						rpr_float3 src = ctx->GetProperty<rpr_float3>(propertyKey);
						*dest++ = src.x;
						*dest++ = src.y;
						*dest++ = src.z;
						break;
					}

					case RPR_PARAMETER_TYPE_FLOAT4:
					{
						rpr_float* dest = reinterpret_cast<rpr_float*>(data);
						rpr_float4 src = ctx->GetProperty<rpr_float4>(propertyKey);
						*dest++ = src.x;
						*dest++ = src.y;
						*dest++ = src.z;
						*dest++ = src.w;
						break;
					}

					case RPR_PARAMETER_TYPE_UINT:
						*reinterpret_cast<rpr_uint*>(data) = ctx->GetProperty<rpr_uint>(propertyKey);
						break;

					case RPR_PARAMETER_TYPE_STRING:
					{
						std::string paramStr = ctx->GetProperty<std::string>(propertyKey);
						memcpy(data, paramStr.c_str(), paramStr.length());
						reinterpret_cast<char*>(data)[paramStr.length()] = '\0';
						break;
					}

					}
					break;

				}
			}
			else
			{
				throw FrException(__FILE__,__LINE__,RPR_ERROR_INVALID_PARAMETER, "",ctx);
			}
		}

		if (size_ret)
			*size_ret = requiredSize;
	}
	catch (FrException& e)
	{
		TRACE__FUNCTION_FAILED(context); return e.GetErrorCode();
	}

	return RPR_SUCCESS;

}

rpr_int rprContextGetAOV(rpr_context context, rpr_aov aov, rpr_framebuffer* out_fb)
{
	s_logger.printTrace("//"); //  ignore getInfo for the trace
	s_logger.printTrace("status = ");
	TRACE__FUNCTION_OPEN;
	s_logger.TraceArg__rpr_context(context); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_uint(aov); s_logger.TraceArg__COMMA();
	s_logger.Trace__FunctionClose();

	API_PRINT("rprContextGetAOV", 1, context);
	FrNode* ctx = static_cast<FrNode*>(context);

	try
	{
		CHECK_NOT_NULL(ctx);
		MACRO__CHECK_ARGUMENT_TYPE(context,Context);


		FrAOVList& aovList = ctx->GetProperty<FrAOVList>(FR_CONTEXT_AOV_LIST);
		*out_fb = reinterpret_cast<rpr_framebuffer*>(aovList[aov]);
	}
	catch (FrException& e)
	{
		TRACE__FUNCTION_FAILED(context); return e.GetErrorCode();
	}

	return RPR_SUCCESS;
}

rpr_int rprContextSetAOV(rpr_context context, rpr_aov aov, rpr_framebuffer frame_buffer)
{
	s_logger.printTrace("status = ");
	TRACE__FUNCTION_OPEN;
	s_logger.TraceArg__rpr_context(context); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_aov(aov); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_framebuffer(frame_buffer);
	s_logger.Trace__FunctionClose();
	s_logger.Trace__rprContextSetAOV(context, aov, frame_buffer);

	API_PRINT("rprContextSetAOV", 1, context);
	FrNode* ctx = static_cast<FrNode*>(context);

	try
	{
		CHECK_NOT_NULL(ctx);
		MACRO__CHECK_ARGUMENT_TYPE(context,Context);
		MACRO__CHECK_ARGUMENT_TYPE(frame_buffer,FrameBuffer);


		FrAOVList& aovList = ctx->GetProperty<FrAOVList>(FR_CONTEXT_AOV_LIST);
		aovList[aov] = reinterpret_cast<FrNode*>(frame_buffer);
		ctx->PropertyChanged(FR_CONTEXT_AOV_LIST,&aov);
	}
	catch (FrException& e)
	{
		TRACE__FUNCTION_FAILED(context); return e.GetErrorCode();
	}

	return RPR_SUCCESS;
}


rpr_int rprContextSetAOVindexLookup(rpr_context context, rpr_int key, rpr_float colorR, rpr_float colorG, rpr_float colorB, rpr_float colorA )
{
	s_logger.printTrace("status = ");
	TRACE__FUNCTION_OPEN;
	s_logger.TraceArg__rpr_context(context); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_int(key); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_float(colorR); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_float(colorG); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_float(colorB); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_float(colorA); 
	s_logger.Trace__FunctionClose();

	API_PRINT("rprContextSetAOVindexLookup", 1, context);
	FrNode* ctx = static_cast<FrNode*>(context);

	try
	{
		CHECK_NOT_NULL(ctx);
		MACRO__CHECK_ARGUMENT_TYPE(context,Context);

		std::shared_ptr<FrRendererEncalps> renderer = ctx->GetProperty< std::shared_ptr<FrRendererEncalps> >(RPR_CONTEXT_ACTIVE_PLUGIN);
		
		if (!renderer->m_FrRenderer) 
			throw FrException(__FILE__,__LINE__,RPR_ERROR_INVALID_PARAMETER, "No active compute API set",context);

		renderer->m_FrRenderer->SetAOVindexLookup(key, rpr_float4(colorR,colorG,colorB,colorA) );
		return RPR_SUCCESS;
		
	}
	catch (FrException& e)
	{
		TRACE__FUNCTION_FAILED(context); return e.GetErrorCode();
	}

	return RPR_SUCCESS;
}


rpr_int rprContextGetScene(rpr_context context, rpr_scene* out_scene)
{
	s_logger.printTrace("//"); //  ignore getter for the trace
	s_logger.printTrace("status = ");
	TRACE__FUNCTION_OPEN;
	s_logger.TraceArg__rpr_context(context); s_logger.TraceArg__COMMA();
	s_logger.Trace__FunctionClose();

	API_PRINT("rprContextGetScene", 1, context);
	FrNode* ctx = static_cast<FrNode*>(context);

	try
	{
		CHECK_NOT_NULL(ctx);
		MACRO__CHECK_ARGUMENT_TYPE(context,Context);


		*out_scene = ctx->GetProperty<FrNode*>(RPR_CONTEXT_SCENE);
		return RPR_SUCCESS;
	}
	catch (FrException& e)
	{
		TRACE__FUNCTION_FAILED(context); return e.GetErrorCode();
	}
}

rpr_int rprContextClearMemory(rpr_context context)
{
	s_logger.printTrace("status = ");
	TRACE__FUNCTION_OPEN;
	s_logger.TraceArg__rpr_context(context);
	s_logger.Trace__FunctionClose();

	API_PRINT("rprContextClearMemory", 1, context);
	FrNode* ctx = static_cast<FrNode*>(context);

	try
	{
		CHECK_NOT_NULL(ctx);
		MACRO__CHECK_ARGUMENT_TYPE(context,Context);


		std::shared_ptr<FrRendererEncalps> renderer = ctx->GetProperty<std::shared_ptr<FrRendererEncalps>>(RPR_CONTEXT_ACTIVE_PLUGIN);
		if (!renderer)
			throw FrException(__FILE__,__LINE__,RPR_ERROR_INVALID_OBJECT, "No active compute API set",context);

		renderer->m_FrRenderer->ClearMemory();

		return RPR_SUCCESS;
	}
	catch (FrException& e)
	{
		TRACE__FUNCTION_FAILED(context); return e.GetErrorCode();
	}
}

//return TRUE if the context parameter is constant
bool ContextParameterIsConstantValue(rpr_context_info in_info)
{
	//note: don't generate trace here, function not exposed - but used internally.

	if (

		//list of read-only parameters
		   in_info == RPR_CONTEXT_MATERIAL_STACK_SIZE
		|| in_info == RPR_CONTEXT_GPU0_NAME
		|| in_info == RPR_CONTEXT_GPU1_NAME
		|| in_info == RPR_CONTEXT_GPU2_NAME
		|| in_info == RPR_CONTEXT_GPU3_NAME
		|| in_info == RPR_CONTEXT_GPU4_NAME
		|| in_info == RPR_CONTEXT_GPU5_NAME
		|| in_info == RPR_CONTEXT_GPU6_NAME
		|| in_info == RPR_CONTEXT_GPU7_NAME
		|| in_info == RPR_CONTEXT_CPU_NAME
		|| in_info == RPR_CONTEXT_CPU_THREAD_LIMIT
		|| in_info == RPR_CONTEXT_API_VERSION
		|| in_info == RPR_CONTEXT_LAST_ERROR_MESSAGE
		)
	{
		return true;
	}

	return false;
}

rpr_int rprContextSetParameterByKey1u(rpr_context context, rpr_context_info in_info, rpr_uint value)
{
	//note: don't generate trace here, function not exposed - but used internally.

	API_PRINT("rprContextSetParameterByKey1u", 1, context);
	FrNode* ctx = static_cast<FrNode*>(context);

	try
	{
		CHECK_NOT_NULL(ctx);
		MACRO__CHECK_ARGUMENT_TYPE(context,Context);


		if (in_info == RPR_CONTEXT_ACTIVE_PLUGIN)
		{
			auto& list = ctx->GetProperty<std::map<rpr_int, std::shared_ptr<FrRendererEncalps>  >>(FR_CONTEXT_PLUGIN_LIST);
			auto itr = list.find(value);
			if (itr != list.cend())
			{
				ctx->SetProperty(RPR_CONTEXT_ACTIVE_PLUGIN, itr->second);
			}
			else
			{
				throw FrException(__FILE__,__LINE__,RPR_ERROR_INTERNAL_ERROR, "No compute API matching value found",context);
			}
		}

		// prohibit to modify constant values
		else if ( ContextParameterIsConstantValue(in_info) ) 
		{
			throw FrException(__FILE__,__LINE__,RPR_ERROR_INVALID_PARAMETER, "",ctx);
		}

		ctx->SetProperty(in_info, value);
		return RPR_SUCCESS;
	}
	catch (FrException& e)
	{
		TRACE__FUNCTION_FAILED(context); return e.GetErrorCode();
	}
	catch (FireSG::property_not_found_error&)
	{
		return RPR_ERROR_INVALID_PARAMETER;
	}
	catch (FireSG::property_type_error&)
	{
		return RPR_ERROR_INVALID_PARAMETER_TYPE;
	}
}

rpr_int rprContextSetParameterByKey1f(rpr_context context, rpr_context_info in_info, rpr_float x)
{
	//note: don't generate trace here, function not exposed - but used internally.


	API_PRINT("rprContextSetParameter1f", 1, context);
	FrNode* ctx = static_cast<FrNode*>(context);

	try
	{
		CHECK_NOT_NULL(ctx);
		MACRO__CHECK_ARGUMENT_TYPE(context,Context);

		// prohibit to modify constant values
		if ( ContextParameterIsConstantValue(in_info) ) 
		{
			throw FrException(__FILE__,__LINE__,RPR_ERROR_INVALID_PARAMETER, "",ctx);
		}

		ctx->SetProperty(in_info, static_cast<rpr_float>(x));
		return RPR_SUCCESS;
	}
	catch (FrException& e)
	{
		TRACE__FUNCTION_FAILED(context); return e.GetErrorCode();
	}
	catch (FireSG::property_not_found_error&)
	{
		return RPR_ERROR_INVALID_PARAMETER;
	}
	catch (FireSG::property_type_error&)
	{
		return RPR_ERROR_INVALID_PARAMETER_TYPE;
	}
}

rpr_int rprContextSetParameterByKey3f(rpr_context context, rpr_context_info in_info, rpr_float x, rpr_float y, rpr_float z)
{
	//note: don't generate trace here, function not exposed - but used internally.

	API_PRINT("rprContextSetParameter3f", 1, context);
	FrNode* ctx = static_cast<FrNode*>(context);

	try
	{
		CHECK_NOT_NULL(ctx);
		MACRO__CHECK_ARGUMENT_TYPE(context,Context);

		// prohibit to modify constant values
		if ( ContextParameterIsConstantValue(in_info) ) 
		{
			throw FrException(__FILE__,__LINE__,RPR_ERROR_INVALID_PARAMETER, "",ctx);
		}

		ctx->SetProperty(in_info, rpr_float3(x, y, z));
		return RPR_SUCCESS;
	}
	catch (FrException& e)
	{
		TRACE__FUNCTION_FAILED(context); return e.GetErrorCode();
	}
	catch (FireSG::property_not_found_error&)
	{
		return RPR_ERROR_INVALID_PARAMETER;
	}
	catch (FireSG::property_type_error&)
	{
		return RPR_ERROR_INVALID_PARAMETER_TYPE;
	}
}

rpr_int rprContextSetParameterByKey4f(rpr_context context, rpr_context_info in_info, rpr_float x, rpr_float y, rpr_float z, rpr_float w)
{
	//note: don't generate trace here, function not exposed - but used internally.

	API_PRINT("rprContextSetParameter4f", 1, context);
	FrNode* ctx = static_cast<FrNode*>(context);

	try
	{
		CHECK_NOT_NULL(ctx);
		MACRO__CHECK_ARGUMENT_TYPE(context,Context);

		if ( ContextParameterIsConstantValue(in_info) ) 
		{
			throw FrException(__FILE__,__LINE__,RPR_ERROR_INVALID_PARAMETER, "",ctx);
		}

		ctx->SetProperty(in_info, rpr_float4(x, y, z, w));
		return RPR_SUCCESS;
	}
	catch (FrException& e)
	{
		TRACE__FUNCTION_FAILED(context); return e.GetErrorCode();
	}
	catch (FireSG::property_not_found_error&)
	{
		return RPR_ERROR_INVALID_PARAMETER;
	}
	catch (FireSG::property_type_error&)
	{
		return RPR_ERROR_INVALID_PARAMETER_TYPE;
	}
}

rpr_int rprContextSetParameterByKeyString(rpr_context context, rpr_context_info in_info, const char* str)
{
	//note: don't generate trace here, function not exposed - but used internally.

	API_PRINT("rprContextSetParameter4f", 1, context);
	FrNode* ctx = static_cast<FrNode*>(context);

	try
	{
		CHECK_NOT_NULL(ctx);
		MACRO__CHECK_ARGUMENT_TYPE(context,Context);

		if ( ContextParameterIsConstantValue(in_info) ) 
		{
			throw FrException(__FILE__,__LINE__,RPR_ERROR_INVALID_PARAMETER, "",ctx);
		}

		ctx->SetProperty(in_info, std::string(str));
		return RPR_SUCCESS;
	}
	catch (FrException& e)
	{
		TRACE__FUNCTION_FAILED(context); return e.GetErrorCode();
	}
	catch (FireSG::property_not_found_error&)
	{
		return RPR_ERROR_INVALID_PARAMETER;
	}
	catch (FireSG::property_type_error&)
	{
		return RPR_ERROR_INVALID_PARAMETER_TYPE;
	}
}


rpr_int rprContextSetParameterString(rpr_context context, const rpr_char* name, const rpr_char* valuestr)
{

	//the tracing feature is not a real exposed feature.
	//so it's tested here directly.
	//example:  rprContextSetParameterString(0,"tracingfolder","C:\\folder\\");
	//path can be relative or absolute.
	if (strcmp(name, "tracingfolder") == 0)
	{
		s_logger.SetTracingFolder(valuestr);
		return RPR_SUCCESS; // return directly
	}

	s_logger.printTrace("status = ");
	TRACE__FUNCTION_OPEN
	s_logger.TraceArg__rpr_context(context); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_char_P(name); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_char_P(valuestr); 
	s_logger.Trace__FunctionClose();

	API_PRINT("rprContextSetParameterString", 1, context);
	FrNode* ctx = static_cast<FrNode*>(context);

	try
	{
		CHECK_NOT_NULL(ctx);
		MACRO__CHECK_ARGUMENT_TYPE(context,Context);
		
		// Convert named string to key and call overloaded method which takes the key.
		std::string lcname = name;
		std::transform(lcname.begin(), lcname.end(), lcname.begin(), ::tolower);
		auto itr = ContextParameterNamesToKeys.find(lcname);
		if (itr != ContextParameterNamesToKeys.end())
		{
			return rprContextSetParameterByKeyString(context, itr->second, valuestr);
		}
		else
		{
			throw FrException(__FILE__,__LINE__,RPR_ERROR_INVALID_PARAMETER, "",ctx);
		}

	}
	catch (FrException& e)
	{
		TRACE__FUNCTION_FAILED(context); return e.GetErrorCode();
	}

	return RPR_ERROR_INVALID_PARAMETER;
}

rpr_int rprContextSetParameter1u(rpr_context context, const rpr_char* name, rpr_uint value)
{
	//the tracing feature is not a real exposed feature.
	//so I prefere to test it here rather than inside  ctx->SetParameter1u.
	//imo, it's more clean.
	if (strcmp(name, "tracing") == 0)
	{
		if (value == 0)
		{
			s_logger.StopTrace();
		}
		else if (value == 1)
		{
			s_logger.StartTrace();
		}
		return RPR_SUCCESS; // return directly
		//note that we don't need a 'context' to enable tracing.
		//we can just call:  rprContextSetParameter1u(0,"tracing",1);
	}

	s_logger.printTrace("status = ");
	TRACE__FUNCTION_OPEN
	s_logger.TraceArg__rpr_context(context); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_char_P(name); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_uint(value);
	s_logger.Trace__FunctionClose();

	API_PRINT("rprContextSetParameter1u", 1, context);
	FrNode* ctx = static_cast<FrNode*>(context);

	try
	{
		CHECK_NOT_NULL(ctx);
		MACRO__CHECK_ARGUMENT_TYPE(context,Context);
		

		// Convert named string to key and call overloaded method which takes the key.
		std::string lcname = name;
		std::transform(lcname.begin(), lcname.end(), lcname.begin(), ::tolower);
		auto itr = ContextParameterNamesToKeys.find(lcname);
		if (itr != ContextParameterNamesToKeys.end())
		{
			return rprContextSetParameterByKey1u(context, itr->second, value);
		}
		else
		{
			throw FrException(__FILE__,__LINE__,RPR_ERROR_INVALID_PARAMETER, "",ctx);
		}

	}
	catch (FrException& e)
	{
		TRACE__FUNCTION_FAILED(context); return e.GetErrorCode();
	}

}

rpr_int rprContextSetParameter1f(rpr_context context, const rpr_char* name, rpr_float x)
{
	s_logger.printTrace("status = ");
	TRACE__FUNCTION_OPEN
	s_logger.TraceArg__rpr_context(context); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_char_P(name); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_float(x);
	s_logger.Trace__FunctionClose();

	API_PRINT("rprContextSetParameter1f", 1, context);
	FrNode* ctx = static_cast<FrNode*>(context);

	try
	{
		CHECK_NOT_NULL(ctx);
		MACRO__CHECK_ARGUMENT_TYPE(context,Context);

		// Convert named string to key and call overloaded method which takes the key.
		std::string lcname = name;
		std::transform(lcname.begin(), lcname.end(), lcname.begin(), ::tolower);
		auto itr = ContextParameterNamesToKeys.find(lcname);
		if (itr != ContextParameterNamesToKeys.end())
		{
			return rprContextSetParameterByKey1f(context, itr->second, x);
		}
		else
		{
			throw FrException(__FILE__,__LINE__,RPR_ERROR_INVALID_PARAMETER, "",ctx);
		}
	}
	catch (FrException& e)
	{
		TRACE__FUNCTION_FAILED(context); return e.GetErrorCode();
	}
}

rpr_int rprContextSetParameter3f(rpr_context context, const rpr_char* name, rpr_float x, rpr_float y, rpr_float z)
{
	s_logger.printTrace("status = ");
	TRACE__FUNCTION_OPEN
	s_logger.TraceArg__rpr_context(context); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_char_P(name); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_float(x); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_float(y); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_float(z);
	s_logger.Trace__FunctionClose();

	API_PRINT("rprContextSetParameter3f", 1, context);
	FrNode* ctx = static_cast<FrNode*>(context);

	try
	{
		CHECK_NOT_NULL(ctx);
		MACRO__CHECK_ARGUMENT_TYPE(context,Context);

		// Convert named string to key and call overloaded method which takes the key.
		std::string lcname = name;
		std::transform(lcname.begin(), lcname.end(), lcname.begin(), ::tolower);
		auto itr = ContextParameterNamesToKeys.find(lcname);
		if (itr != ContextParameterNamesToKeys.end())
		{
			return rprContextSetParameterByKey3f(context, itr->second, x, y, z);
		}
		else
		{
			throw FrException(__FILE__,__LINE__,RPR_ERROR_INVALID_PARAMETER, "",ctx);
		}
	}
	catch (FrException& e)
	{
		TRACE__FUNCTION_FAILED(context); return e.GetErrorCode();
	}
}

rpr_int rprContextSetParameter4f(rpr_context context, const rpr_char* name, rpr_float x, rpr_float y, rpr_float z, rpr_float w)
{
	s_logger.printTrace("status = ");
	TRACE__FUNCTION_OPEN
	s_logger.TraceArg__rpr_context(context); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_char_P(name); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_float(x); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_float(y); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_float(z); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_float(w);
	s_logger.Trace__FunctionClose();

	API_PRINT("rprContextSetParameter4f", 1, context);
	FrNode* ctx = static_cast<FrNode*>(context);

	try
	{
		CHECK_NOT_NULL(ctx);
		MACRO__CHECK_ARGUMENT_TYPE(context,Context);

		// Convert named string to key and call overloaded method which takes the key.
		std::string lcname = name;
		std::transform(lcname.begin(), lcname.end(), lcname.begin(), ::tolower);
		auto itr = ContextParameterNamesToKeys.find(lcname);
		if (itr != ContextParameterNamesToKeys.end())
		{
			return rprContextSetParameterByKey4f(context, itr->second, x, y, z, w);
		}
		else
		{
			throw FrException(__FILE__,__LINE__,RPR_ERROR_INVALID_PARAMETER, "",ctx);
		}
	}
	catch (FrException& e)
	{
		TRACE__FUNCTION_FAILED(context); return e.GetErrorCode();
	}
}

rpr_int rprContextCreateScene(rpr_context context, rpr_scene* out_scene)
{
	//in case of issues during creation, it's safer to return a null pointer, rather than let the pointer undefined.
	if ( out_scene )
		*out_scene = nullptr;
	

	s_logger.printTrace("//Scene creation\r\n");
	s_logger.Trace__FlushAllFiles(); // in case a crash, ensure trace informs the creation

	FrNode* ctx = static_cast<FrNode*>(context);
	API_PRINT("frCreateScene", 1, context);

	try
	{
		CHECK_NOT_NULL(ctx);
		MACRO__CHECK_ARGUMENT_TYPE(context,Context);


		std::shared_ptr<FrSceneGraph> sceneGraph = ctx->GetProperty< std::shared_ptr<FrSceneGraph> >(FR_SCENEGRAPH);
		std::shared_ptr<FrRendererEncalps> renderer = ctx->GetProperty< std::shared_ptr<FrRendererEncalps> >(RPR_CONTEXT_ACTIVE_PLUGIN);
		FrNode* node = sceneGraph->CreateNode(NodeTypes::Scene, [&](FrNode* n) {
			n->SetProperty(FR_NODE_CONTEXT, ctx);
            n->AddProperty(FR_SCENEGRAPH, sceneGraph);
			n->AddProperty(RPR_CONTEXT_ACTIVE_PLUGIN, renderer);
		});

		*out_scene = node;

		s_logger.Trace__NewFrObjectCreated(Logger::TRACE_ADDRESS_TYPE__SCENE, *out_scene);
		s_logger.Trace__DeclareFRobject("rpr_scene", "scene_0x", *out_scene);
		s_logger.printTrace("status = ");
		TRACE__FUNCTION_OPEN
		s_logger.TraceArg__rpr_context(context); s_logger.TraceArg__COMMA();
		s_logger.printTrace("&scene_0x%p", *out_scene);
		s_logger.Trace__FunctionClose();
		s_logger.printTrace("status = rprObjectSetName (scene_0x%p,\"scene_0x%p\"); RPRTRACE_CHECK// added by tracing for debug\r\n",  *out_scene ,  *out_scene);

		return RPR_SUCCESS;
	}
	catch (FrException& e)
	{
		TRACE__FUNCTION_FAILED(context); return e.GetErrorCode();
	}
}

rpr_int rprContextCreateInstance(rpr_context context, rpr_shape shape, rpr_shape* out_instance)
{
	//in case of issues during creation, it's safer to return a null pointer, rather than let the pointer undefined.
	if ( out_instance )
		*out_instance = nullptr;
	

	s_logger.printTrace("//Instance creation\r\n");
	s_logger.Trace__FlushAllFiles(); // in case a crash, ensure trace informs the creation

	API_PRINT("rprContextCreateInstance", 1, context);
	FrNode* ctx = static_cast<FrNode*>(context);

	try
	{
		CHECK_NOT_NULL(ctx);
		MACRO__CHECK_ARGUMENT_TYPE(context,Context);
		MACRO__CHECK_ARGUMENT_TYPE_ALLSHAPES(shape);


		std::shared_ptr<FrSceneGraph> sceneGraph = ctx->GetProperty< std::shared_ptr<FrSceneGraph> >(FR_SCENEGRAPH);
		std::shared_ptr<FrRendererEncalps> renderer = ctx->GetProperty< std::shared_ptr<FrRendererEncalps> >(RPR_CONTEXT_ACTIVE_PLUGIN);
		FrNode* node = sceneGraph->CreateNode(NodeTypes::Instance, [&](FrNode* n) {
			n->SetProperty(FR_NODE_CONTEXT, ctx);
			n->SetProperty(RPR_INSTANCE_PARENT_SHAPE, static_cast<FrNode*>(shape));
            n->AddProperty(FR_SCENEGRAPH, sceneGraph);
			n->AddProperty(RPR_CONTEXT_ACTIVE_PLUGIN, renderer);
		});

		*out_instance = node;

		s_logger.Trace__NewFrObjectCreated(Logger::TRACE_ADDRESS_TYPE__SHAPE, *out_instance);
		s_logger.Trace__DeclareFRobject("rpr_shape", "shape_0x", *out_instance);
		s_logger.printTrace("status = ");
		TRACE__FUNCTION_OPEN
		s_logger.TraceArg__rpr_context(context); s_logger.TraceArg__COMMA();
		s_logger.TraceArg__rpr_shape(shape); s_logger.TraceArg__COMMA();
		s_logger.printTrace("&shape_0x%p", *out_instance);
		s_logger.Trace__FunctionClose();
		s_logger.printTrace("status = rprObjectSetName (shape_0x%p,\"shape_0x%p\"); RPRTRACE_CHECK// added by tracing for debug\r\n",  *out_instance ,  *out_instance);

		return RPR_SUCCESS;
	}
	catch (FrException& e)
	{
		TRACE__FUNCTION_FAILED(context); return e.GetErrorCode();
	}
}

rpr_int rprContextCreateMesh(rpr_context context, const rpr_float* vertices, size_t num_vertices, rpr_int vertex_stride,
	const rpr_float* normals, size_t num_normals, rpr_int normal_stride,
	const rpr_float* texcoords, size_t num_texcoords, rpr_int texcoord_stride,
	const rpr_int*   vertex_indices, rpr_int vidx_stride,
	const rpr_int*   normal_indices, rpr_int nidx_stride,
	const rpr_int*   texcoord_indices, rpr_int tidx_stride,
	const rpr_int*   num_face_vertices, size_t num_faces,
	rpr_shape* out_mesh)
{
	//in case of issues during creation, it's safer to return a null pointer, rather than let the pointer undefined.
	if ( out_mesh )
		*out_mesh = nullptr;
	

	if (s_logger.IsTraceActivated())
	{
		//unsigned long long dataSize = 0;
		unsigned long long wholeDataSize = 0;

		unsigned long long data1_size = num_vertices*vertex_stride;
		s_logger.TraceArg_Prepare__DATA(vertices, data1_size, "pData1");
		wholeDataSize += data1_size;

		unsigned long long data2_size = num_normals*normal_stride;
		s_logger.TraceArg_Prepare__DATA(normals, data2_size, "pData2");
		wholeDataSize += data2_size;

		unsigned long long data3_size = num_texcoords*texcoord_stride;
		s_logger.TraceArg_Prepare__DATA(texcoords, data3_size, "pData3");
		wholeDataSize += data3_size;

		unsigned int nbTotalIndices = 0;
		for (int iFace = 0; iFace < num_faces; iFace++)
		{
			nbTotalIndices += num_face_vertices[iFace];
		}

		unsigned long long data4_size = nbTotalIndices*vidx_stride;
		s_logger.TraceArg_Prepare__DATA(vertex_indices, data4_size, "pData4");
		wholeDataSize += data4_size;


		unsigned long long data5_size = nbTotalIndices*nidx_stride;
		s_logger.TraceArg_Prepare__DATA(normal_indices, data5_size, "pData5");
		wholeDataSize += data5_size;

		unsigned long long data6_size = nbTotalIndices*tidx_stride;
		s_logger.TraceArg_Prepare__DATA(texcoord_indices, data6_size, "pData6");
		wholeDataSize += data6_size;

		unsigned long long data7_size = num_faces * sizeof(num_face_vertices[0]);
		s_logger.TraceArg_Prepare__DATA(num_face_vertices, data7_size, "pData7");
		wholeDataSize += data7_size;

		s_logger.printTrace("//Mesh creation\r\n");
		s_logger.printTrace("//size in data file for this mesh: %llu\r\n", wholeDataSize);
		s_logger.printTrace("//number of indices: %d\r\n", nbTotalIndices);
		s_logger.Trace__FlushAllFiles(); // in case a crash, ensure trace informs the creation
	}

	API_PRINT("rprContextCreateMesh", 1, context);
	FrNode* ctx = static_cast<FrNode*>(context);

	try
	{
		CHECK_NOT_NULL(ctx);
		MACRO__CHECK_ARGUMENT_TYPE(context,Context);


		// Count the total number of indices by iterating over face array and checking primitive type.
		size_t indicesCount = 0U;
		for (size_t i = 0; i < num_faces; ++i)
		{
			indicesCount += num_face_vertices[i];
		}

		// The goal here is to make copies of each of the incoming arrays as is.  So the arrays need to
		// be checked to see if any are interleaved.
		auto copiedArrays = CopyMeshArrays({
			{ reinterpret_cast<void*>(const_cast<float*>(vertices)), sizeof(float) * 3, static_cast<size_t>(vertex_stride), num_vertices, 0 },
			{ reinterpret_cast<void*>(const_cast<float*>(normals)), sizeof(float) * 3, static_cast<size_t>(normal_stride), num_normals, 1 },
			{ reinterpret_cast<void*>(const_cast<float*>(texcoords)), sizeof(float) * 2, static_cast<size_t>(texcoord_stride), num_texcoords, 2 },
		});

		auto copiedIndices = CopyMeshArrays({
			{ reinterpret_cast<void*>(const_cast<int*>(vertex_indices)), sizeof(int), static_cast<size_t>(vidx_stride), indicesCount, 0 },
			{ reinterpret_cast<void*>(const_cast<int*>(normal_indices)), sizeof(int), static_cast<size_t>(nidx_stride), indicesCount, 1 },
			{ reinterpret_cast<void*>(const_cast<int*>(texcoord_indices)), sizeof(int), static_cast<size_t>(tidx_stride), indicesCount, 2 },
		});

		//rpr_int* copiedIndices = new rpr_int[indicesCount];
		//char* src = reinterpret_cast<char*>(const_cast<rpr_int*>(vertex_indices));
		//for (size_t i = 0; i < indicesCount; ++i)
		//{
			//copiedIndices[i] = *reinterpret_cast<rpr_int*>(src + i * vidx_stride);
	//	}

		rpr_int* numFaceVerticesCopy = new rpr_int[num_faces];
		memcpy(numFaceVerticesCopy, num_face_vertices, num_faces * sizeof(rpr_int));

		// Create the mesh node and add copied data.
		std::shared_ptr<FrSceneGraph> sceneGraph = ctx->GetProperty< std::shared_ptr<FrSceneGraph> >(FR_SCENEGRAPH);
		std::shared_ptr<FrRendererEncalps> renderer = ctx->GetProperty< std::shared_ptr<FrRendererEncalps> >(RPR_CONTEXT_ACTIVE_PLUGIN);
		FrNode* node = sceneGraph->CreateNode(NodeTypes::Mesh, [&](FrNode* n) {
			n->SetProperty(FR_NODE_CONTEXT, ctx);

			// Vertex arrays.
			for (auto& a : copiedArrays)
			{
				if (a.flags == 0)
				{
					n->SetProperty(RPR_MESH_VERTEX_ARRAY, reinterpret_cast<rpr_float*>(a.pointer));
					n->SetProperty(RPR_MESH_VERTEX_STRIDE, a.stride);
					n->SetProperty(RPR_MESH_VERTEX_COUNT, a.elementCount);
				}
				else if (a.flags == 1)
				{
					n->SetProperty(RPR_MESH_NORMAL_ARRAY, reinterpret_cast<rpr_float*>(a.pointer));
					n->SetProperty(RPR_MESH_NORMAL_STRIDE, a.stride);
					n->SetProperty(RPR_MESH_NORMAL_COUNT, a.elementCount);
				}
				else if (a.flags == 2)
				{
					n->SetProperty(RPR_MESH_UV_ARRAY, reinterpret_cast<rpr_float*>(a.pointer));
					n->SetProperty(RPR_MESH_UV_STRIDE, a.stride);
					n->SetProperty(RPR_MESH_UV_COUNT, a.elementCount);
				}
				else if (a.flags == 3)
				{
					n->SetProperty(RPR_MESH_UV2_ARRAY, reinterpret_cast<rpr_float*>(a.pointer));
					n->SetProperty(RPR_MESH_UV2_STRIDE, a.stride);
					n->SetProperty(RPR_MESH_UV2_COUNT, a.elementCount);
				}
			}

			// Index arrays.
			for (auto& a : copiedIndices)
			{
				if (a.flags == 0)
				{
					n->SetProperty(RPR_MESH_VERTEX_INDEX_ARRAY, reinterpret_cast<rpr_int*>(a.pointer));
					n->SetProperty(RPR_MESH_VERTEX_INDEX_STRIDE, a.stride);
				}
				else if (a.flags == 1)
				{
					n->SetProperty(RPR_MESH_NORMAL_INDEX_ARRAY, reinterpret_cast<rpr_int*>(a.pointer));
					n->SetProperty(RPR_MESH_NORMAL_INDEX_STRIDE, a.stride);
				}
				else if (a.flags == 2)
				{
					n->SetProperty(RPR_MESH_UV_INDEX_ARRAY, reinterpret_cast<rpr_int*>(a.pointer));
					n->SetProperty(RPR_MESH_UV_INDEX_STRIDE, a.stride);
				}
			}

			//n->SetProperty(RPR_MESH_VERTEX_INDEX_ARRAY, copiedIndices);
			//n->SetProperty(RPR_MESH_VERTEX_INDEX_STRIDE, sizeof(rpr_int));

			// Faces.
			n->SetProperty(RPR_MESH_NUM_FACE_VERTICES_ARRAY, numFaceVerticesCopy);
			n->SetProperty(RPR_MESH_POLYGON_COUNT, num_faces);
            n->AddProperty(FR_SCENEGRAPH, sceneGraph);
			n->AddProperty(RPR_CONTEXT_ACTIVE_PLUGIN, renderer);
		});

		*out_mesh = node;


		if (s_logger.IsTraceActivated())
		{
			s_logger.Trace__NewFrObjectCreated(Logger::TRACE_ADDRESS_TYPE__SHAPE, *out_mesh);
			s_logger.Trace__DeclareFRobject("rpr_shape", "shape_0x", *out_mesh);
			s_logger.printTrace("status = ");
			TRACE__FUNCTION_OPEN
			s_logger.TraceArg__rpr_context(context); s_logger.TraceArg__COMMA();
			s_logger.TraceArg_Use__DATA_const_rpr_float_P("pData1"); s_logger.TraceArg__COMMA();
			s_logger.TraceArg__size_t(num_vertices);  s_logger.TraceArg__COMMA();
			s_logger.TraceArg__rpr_int(vertex_stride);  s_logger.TraceArg__COMMA();
			s_logger.TraceArg_Use__DATA_const_rpr_float_P("pData2"); s_logger.TraceArg__COMMA();
			s_logger.TraceArg__size_t(num_normals);  s_logger.TraceArg__COMMA();
			s_logger.TraceArg__rpr_int(normal_stride); s_logger.TraceArg__COMMA();
			s_logger.TraceArg_Use__DATA_const_rpr_float_P("pData3"); s_logger.TraceArg__COMMA();
			s_logger.TraceArg__size_t(num_texcoords);  s_logger.TraceArg__COMMA();
			s_logger.TraceArg__rpr_int(texcoord_stride);  s_logger.TraceArg__COMMA();
			s_logger.TraceArg_Use__DATA_const_rpr_int_P("pData4"); s_logger.TraceArg__COMMA();
			s_logger.TraceArg__rpr_int(vidx_stride); s_logger.TraceArg__COMMA();
			s_logger.TraceArg_Use__DATA_const_rpr_int_P("pData5"); s_logger.TraceArg__COMMA();
			s_logger.TraceArg__rpr_int(nidx_stride);  s_logger.TraceArg__COMMA();
			s_logger.TraceArg_Use__DATA_const_rpr_int_P("pData6"); s_logger.TraceArg__COMMA();
			s_logger.TraceArg__rpr_int(tidx_stride);  s_logger.TraceArg__COMMA();
			s_logger.TraceArg_Use__DATA_const_rpr_int_P("pData7"); s_logger.TraceArg__COMMA();
			s_logger.TraceArg__size_t(num_faces); s_logger.TraceArg__COMMA();
			s_logger.printTrace("&shape_0x%p", *out_mesh);
			s_logger.Trace__FunctionClose();
			s_logger.printTrace("status = rprObjectSetName (shape_0x%p,\"shape_0x%p\"); RPRTRACE_CHECK// added by tracing for debug\r\n",  *out_mesh ,  *out_mesh);
		}

		return RPR_SUCCESS;
	}
	catch (FrException& e)
	{
		TRACE__FUNCTION_FAILED(context); return e.GetErrorCode();
	}
}

rpr_int rprContextCreateMeshEx(
	rpr_context context, 
	const rpr_float* vertices, size_t num_vertices, rpr_int vertex_stride,
	const rpr_float* normals, size_t num_normals, rpr_int normal_stride,
	const rpr_int* perVertexFlag, size_t num_perVertexFlags, rpr_int perVertexFlag_stride, 
	rpr_int numberOfTexCoordLayers, const rpr_float** texcoords, const size_t* num_texcoords, const rpr_int* texcoord_stride,
	const rpr_int*   vertex_indices, rpr_int vidx_stride,
	const rpr_int*   normal_indices, rpr_int nidx_stride,
	const rpr_int**   texcoord_indices_, const rpr_int* tidx_stride_,
	const rpr_int*   num_face_vertices, size_t num_faces,
	rpr_shape* out_mesh)
{

	#ifdef RPR_FOR_BAIKAL
	if (num_perVertexFlags == 0 && numberOfTexCoordLayers == 1)
    {
        //can use rprContextCreateMesh
        return rprContextCreateMesh(context,
                                vertices, num_vertices, vertex_stride,
                                normals, num_normals, normal_stride,
                                texcoords[0], num_texcoords[0], texcoord_stride[0],
                                vertex_indices, vidx_stride,
                                normal_indices, nidx_stride,
                                texcoord_indices_[0], tidx_stride_[0],
                                num_face_vertices, num_faces, out_mesh);
    }
	return RPR_ERROR_UNIMPLEMENTED;
	#endif


	
	//in case of issues during creation, it's safer to return a null pointer, rather than let the pointer undefined.
	if ( out_mesh )
		*out_mesh = nullptr;
	

	const int NB_MAX_UV_CHANNELS = 2;
	const rpr_float* texcoords_array[NB_MAX_UV_CHANNELS] = { nullptr , nullptr };
	size_t num_texcoords_array[NB_MAX_UV_CHANNELS] = { 0 , 0 };
	rpr_int texcoord_stride_array[NB_MAX_UV_CHANNELS] = { 0 , 0 };
	const rpr_int*   texcoord_indices_array[NB_MAX_UV_CHANNELS] = { nullptr , nullptr };
	rpr_int tidx_stride_array[NB_MAX_UV_CHANNELS] = { 0 , 0 };

	API_PRINT("rprContextCreateMeshEx", 1, context);
	FrNode* ctx = static_cast<FrNode*>(context);

	try
	{
		CHECK_NOT_NULL(ctx);
		if ( numberOfTexCoordLayers > NB_MAX_UV_CHANNELS )
		{
			throw FrException(__FILE__,__LINE__,RPR_ERROR_INVALID_PARAMETER, "",ctx);
		}

		for(int i=0; i<numberOfTexCoordLayers; i++)
		{
			texcoords_array[i] = texcoords[i];
			num_texcoords_array[i] = num_texcoords[i];
			texcoord_stride_array[i] = texcoord_stride[i];
			texcoord_indices_array[i] = texcoord_indices_[i];
			tidx_stride_array[i] = tidx_stride_[i];
		}

	
		if (s_logger.IsTraceActivated())
		{
			unsigned long long wholeDataSize = 0;
	
			unsigned long long data1_size = num_vertices*vertex_stride;
			s_logger.TraceArg_Prepare__DATA(vertices, data1_size, "pData1");
			wholeDataSize += data1_size;
	
			unsigned long long data2_size = num_normals*normal_stride;
			s_logger.TraceArg_Prepare__DATA(normals, data2_size, "pData2");
			wholeDataSize += data2_size;
	
			unsigned long long data3_size = num_perVertexFlags*perVertexFlag_stride;
			s_logger.TraceArg_Prepare__DATA(perVertexFlag, data3_size, "pData3");
			wholeDataSize += data3_size;
	

			unsigned long long* ppData1_size = new unsigned long long[numberOfTexCoordLayers];
			for(int i=0; i<numberOfTexCoordLayers; i++)
			{
				ppData1_size[i] = num_texcoords[i] * texcoord_stride[i];
				wholeDataSize += ppData1_size[i];
			}
			s_logger.TraceArg_Prepare__PDATA((const void **)texcoords, ppData1_size, numberOfTexCoordLayers, "ppData1");
			delete[] ppData1_size; ppData1_size=NULL;
	

			unsigned long long data4_size = numberOfTexCoordLayers*sizeof(size_t);
			s_logger.TraceArg_Prepare__DATA(num_texcoords, data4_size, "pData4");
			wholeDataSize += data4_size;

			unsigned long long data5_size = numberOfTexCoordLayers*sizeof(rpr_int);
			s_logger.TraceArg_Prepare__DATA(texcoord_stride, data5_size, "pData5");
			wholeDataSize += data5_size;

			unsigned int nbTotalIndices = 0;
			for (int iFace = 0; iFace < num_faces; iFace++)
			{
				nbTotalIndices += num_face_vertices[iFace];
			}
	
			unsigned long long data6_size = nbTotalIndices*vidx_stride;
			s_logger.TraceArg_Prepare__DATA(vertex_indices, data6_size, "pData6");
			wholeDataSize += data6_size;
	
	
			unsigned long long data7_size = nbTotalIndices*nidx_stride;
			s_logger.TraceArg_Prepare__DATA(normal_indices, data7_size, "pData7");
			wholeDataSize += data7_size;
	
			unsigned long long* ppData2_size = new unsigned long long[numberOfTexCoordLayers];
			for(int i=0; i<numberOfTexCoordLayers; i++)
			{
				ppData2_size[i] = nbTotalIndices*tidx_stride_[i];
				wholeDataSize += ppData2_size[i];
			}
			s_logger.TraceArg_Prepare__PDATA((const void **)texcoord_indices_, ppData2_size, numberOfTexCoordLayers, "ppData2");
			delete[] ppData2_size; ppData2_size=NULL;
	
			unsigned long long data8_size = numberOfTexCoordLayers*sizeof(rpr_int);
			s_logger.TraceArg_Prepare__DATA(tidx_stride_, data8_size, "pData8");
			wholeDataSize += data8_size;

			unsigned long long data9_size = num_faces * sizeof(num_face_vertices[0]);
			s_logger.TraceArg_Prepare__DATA(num_face_vertices, data9_size, "pData9");
			wholeDataSize += data9_size;
	
			s_logger.printTrace("//Mesh creation\r\n");
			s_logger.printTrace("//size in data file for this mesh: %llu\r\n", wholeDataSize);
			s_logger.printTrace("//number of indices: %d\r\n", nbTotalIndices);
			s_logger.Trace__FlushAllFiles(); // in case a crash, ensure trace informs the creation
		}


		MACRO__CHECK_ARGUMENT_TYPE(context,Context);


		// Count the total number of indices by iterating over face array and checking primitive type.
		size_t indicesCount = 0U;
		for (size_t i = 0; i < num_faces; ++i)
		{
			indicesCount += num_face_vertices[i];
		}

		// The goal here is to make copies of each of the incoming arrays as is.  So the arrays need to
		// be checked to see if any are interleaved.
		auto copiedArrays = CopyMeshArrays({
			{ reinterpret_cast<void*>(const_cast<float*>(vertices)), sizeof(float) * 3, static_cast<size_t>(vertex_stride), num_vertices, 0 },
			{ reinterpret_cast<void*>(const_cast<float*>(normals)), sizeof(float) * 3, static_cast<size_t>(normal_stride), num_normals, 1 },
			{ reinterpret_cast<void*>(const_cast<float*>(texcoords_array[0])), sizeof(float) * 2, static_cast<size_t>(texcoord_stride_array[0]), num_texcoords_array[0], 2 },
			{ reinterpret_cast<void*>(const_cast<float*>(texcoords_array[1])), sizeof(float) * 2, static_cast<size_t>(texcoord_stride_array[1]), num_texcoords_array[1], 3 },
		});

		auto copiedIndices = CopyMeshArrays({
			{ reinterpret_cast<void*>(const_cast<int*>(vertex_indices)), sizeof(int), static_cast<size_t>(vidx_stride), indicesCount, 0 },
			{ reinterpret_cast<void*>(const_cast<int*>(normal_indices)), sizeof(int), static_cast<size_t>(nidx_stride), indicesCount, 1 },
			{ reinterpret_cast<void*>(const_cast<int*>(texcoord_indices_array[0])), sizeof(int), static_cast<size_t>(tidx_stride_array[0]), indicesCount, 2 },
			{ reinterpret_cast<void*>(const_cast<int*>(texcoord_indices_array[1])), sizeof(int), static_cast<size_t>(tidx_stride_array[1]), indicesCount, 3 },
		});

		//rpr_int* copiedIndices = new rpr_int[indicesCount];
		//char* src = reinterpret_cast<char*>(const_cast<rpr_int*>(vertex_indices));
		//for (size_t i = 0; i < indicesCount; ++i)
		//{
			//copiedIndices[i] = *reinterpret_cast<rpr_int*>(src + i * vidx_stride);
	//	}

		rpr_int* numFaceVerticesCopy = new rpr_int[num_faces];
		memcpy(numFaceVerticesCopy, num_face_vertices, num_faces * sizeof(rpr_int));

		// Create the mesh node and add copied data.
		std::shared_ptr<FrSceneGraph> sceneGraph = ctx->GetProperty< std::shared_ptr<FrSceneGraph> >(FR_SCENEGRAPH);
		std::shared_ptr<FrRendererEncalps> renderer = ctx->GetProperty< std::shared_ptr<FrRendererEncalps> >(RPR_CONTEXT_ACTIVE_PLUGIN);
		FrNode* node = sceneGraph->CreateNode(NodeTypes::Mesh, [&](FrNode* n) {
			n->SetProperty(FR_NODE_CONTEXT, ctx);

			// Vertex arrays.
			for (auto& a : copiedArrays)
			{
				if (a.flags == 0)
				{
					n->SetProperty(RPR_MESH_VERTEX_ARRAY, reinterpret_cast<rpr_float*>(a.pointer));
					n->SetProperty(RPR_MESH_VERTEX_STRIDE, a.stride);
					n->SetProperty(RPR_MESH_VERTEX_COUNT, a.elementCount);
				}
				else if (a.flags == 1)
				{
					n->SetProperty(RPR_MESH_NORMAL_ARRAY, reinterpret_cast<rpr_float*>(a.pointer));
					n->SetProperty(RPR_MESH_NORMAL_STRIDE, a.stride);
					n->SetProperty(RPR_MESH_NORMAL_COUNT, a.elementCount);
				}
				else if (a.flags == 2)
				{
					n->SetProperty(RPR_MESH_UV_ARRAY, reinterpret_cast<rpr_float*>(a.pointer));
					n->SetProperty(RPR_MESH_UV_STRIDE, a.stride);
					n->SetProperty(RPR_MESH_UV_COUNT, a.elementCount);
				}
				else if (a.flags == 3)
				{
					n->SetProperty(RPR_MESH_UV2_ARRAY, reinterpret_cast<rpr_float*>(a.pointer));
					n->SetProperty(RPR_MESH_UV2_STRIDE, a.stride);
					n->SetProperty(RPR_MESH_UV2_COUNT, a.elementCount);
				}
			}

			// Index arrays.
			for (auto& a : copiedIndices)
			{
				if (a.flags == 0)
				{
					n->SetProperty(RPR_MESH_VERTEX_INDEX_ARRAY, reinterpret_cast<rpr_int*>(a.pointer));
					n->SetProperty(RPR_MESH_VERTEX_INDEX_STRIDE, a.stride);
				}
				else if (a.flags == 1)
				{
					n->SetProperty(RPR_MESH_NORMAL_INDEX_ARRAY, reinterpret_cast<rpr_int*>(a.pointer));
					n->SetProperty(RPR_MESH_NORMAL_INDEX_STRIDE, a.stride);
				}
				else if (a.flags == 2)
				{
					n->SetProperty(RPR_MESH_UV_INDEX_ARRAY, reinterpret_cast<rpr_int*>(a.pointer));
					n->SetProperty(RPR_MESH_UV_INDEX_STRIDE, a.stride);
				}
				else if (a.flags == 3)
				{
					n->SetProperty(RPR_MESH_UV2_INDEX_ARRAY, reinterpret_cast<rpr_int*>(a.pointer));
					n->SetProperty(RPR_MESH_UV2_INDEX_STRIDE, a.stride);
				}
			}

			//n->SetProperty(RPR_MESH_VERTEX_INDEX_ARRAY, copiedIndices);
			//n->SetProperty(RPR_MESH_VERTEX_INDEX_STRIDE, sizeof(rpr_int));

			// Faces.
			n->SetProperty(RPR_MESH_NUM_FACE_VERTICES_ARRAY, numFaceVerticesCopy);
			n->SetProperty(RPR_MESH_POLYGON_COUNT, num_faces);
            n->AddProperty(FR_SCENEGRAPH, sceneGraph);
			n->AddProperty(RPR_CONTEXT_ACTIVE_PLUGIN, renderer);
		});

		*out_mesh = node;

		
		if (s_logger.IsTraceActivated())
		{
			s_logger.Trace__NewFrObjectCreated(Logger::TRACE_ADDRESS_TYPE__SHAPE, *out_mesh);
			s_logger.Trace__DeclareFRobject("rpr_shape", "shape_0x", *out_mesh);
			s_logger.printTrace("status = ");
			TRACE__FUNCTION_OPEN

			s_logger.TraceArg__rpr_context(context); s_logger.TraceArg__COMMA();
			s_logger.TraceArg_Use__DATA_const_rpr_float_P("pData1"); s_logger.TraceArg__COMMA();
			s_logger.TraceArg__size_t(num_vertices); s_logger.TraceArg__COMMA();
			s_logger.TraceArg__rpr_int(vertex_stride);  s_logger.TraceArg__COMMA();
			s_logger.TraceArg_Use__DATA_const_rpr_float_P("pData2"); s_logger.TraceArg__COMMA();
			s_logger.TraceArg__size_t(num_normals); s_logger.TraceArg__COMMA();
			s_logger.TraceArg__rpr_int(normal_stride);  s_logger.TraceArg__COMMA();
			s_logger.TraceArg_Use__DATA_const_rpr_int_P("pData3"); s_logger.TraceArg__COMMA();
			s_logger.TraceArg__size_t(num_perVertexFlags); s_logger.TraceArg__COMMA();
			s_logger.TraceArg__rpr_int(perVertexFlag_stride);  s_logger.TraceArg__COMMA();
			s_logger.TraceArg__rpr_int(numberOfTexCoordLayers);  s_logger.TraceArg__COMMA();
			s_logger.TraceArg_Use__DATA_const_rpr_float_PP("&ppData1[0]"); s_logger.TraceArg__COMMA();
			s_logger.TraceArg_Use__DATA_const_size_t_P("pData4"); s_logger.TraceArg__COMMA();
			s_logger.TraceArg_Use__DATA_const_rpr_int_P("pData5"); s_logger.TraceArg__COMMA();
			s_logger.TraceArg_Use__DATA_const_rpr_int_P("pData6"); s_logger.TraceArg__COMMA();
			s_logger.TraceArg__rpr_int(vidx_stride);  s_logger.TraceArg__COMMA();
			s_logger.TraceArg_Use__DATA_const_rpr_int_P("pData7"); s_logger.TraceArg__COMMA();
			s_logger.TraceArg__rpr_int(nidx_stride);  s_logger.TraceArg__COMMA();
			s_logger.TraceArg_Use__DATA_const_rpr_int_PP("&ppData2[0]"); s_logger.TraceArg__COMMA();
			s_logger.TraceArg_Use__DATA_const_rpr_int_P("pData8"); s_logger.TraceArg__COMMA();
			s_logger.TraceArg_Use__DATA_const_rpr_int_P("pData9"); s_logger.TraceArg__COMMA();
			s_logger.TraceArg__size_t(num_faces); s_logger.TraceArg__COMMA();
			s_logger.printTrace("&shape_0x%p", *out_mesh);
			s_logger.Trace__FunctionClose();
			s_logger.printTrace("status = rprObjectSetName (shape_0x%p,\"shape_0x%p\"); RPRTRACE_CHECK// added by tracing for debug\r\n",  *out_mesh ,  *out_mesh);
		}
		

		return RPR_SUCCESS;
	}
	catch (FrException& e)
	{
		TRACE__FUNCTION_FAILED(context); return e.GetErrorCode();
	}
}


rpr_int rprContextCreateMeshEx2(
	rpr_context context, 
	const rpr_float* vertices, size_t num_vertices, rpr_int vertex_stride,
	const rpr_float* normals, size_t num_normals, rpr_int normal_stride,
	const rpr_int* perVertexFlag, size_t num_perVertexFlags, rpr_int perVertexFlag_stride, 
	rpr_int numberOfTexCoordLayers, const rpr_float** texcoords, const size_t* num_texcoords, const rpr_int* texcoord_stride,
	const rpr_int*   vertex_indices, rpr_int vidx_stride,
	const rpr_int*   normal_indices, rpr_int nidx_stride,
	const rpr_int**   texcoord_indices_, const rpr_int* tidx_stride_,
	const rpr_int*   num_face_vertices, size_t num_faces,
	rpr_mesh_info const * mesh_properties,
	rpr_shape* out_mesh)
{
	//in case of issues during creation, it's safer to return a null pointer, rather than let the pointer undefined.
	if ( out_mesh )
		*out_mesh = nullptr;
	

	const int NB_MAX_UV_CHANNELS = 2;
	const rpr_float* texcoords_array[NB_MAX_UV_CHANNELS] = { nullptr , nullptr };
	size_t num_texcoords_array[NB_MAX_UV_CHANNELS] = { 0 , 0 };
	rpr_int texcoord_stride_array[NB_MAX_UV_CHANNELS] = { 0 , 0 };
	const rpr_int*   texcoord_indices_array[NB_MAX_UV_CHANNELS] = { nullptr , nullptr };
	rpr_int tidx_stride_array[NB_MAX_UV_CHANNELS] = { 0 , 0 };

	API_PRINT("rprContextCreateMeshEx", 1, context);
	FrNode* ctx = static_cast<FrNode*>(context);

	
	try
	{
		CHECK_NOT_NULL(ctx);

		if ( numberOfTexCoordLayers > NB_MAX_UV_CHANNELS )
		{
			throw FrException(__FILE__,__LINE__,RPR_ERROR_INVALID_PARAMETER, "",ctx);
		}


		//example of mesh_properties :  list of mesh property names and their corresponding values.
		//                       Each property name is immediately followed by the corresponding desired value.
		//                       The list is terminated with 0
		// rpr_mesh_info mesh_properties[16];
		// mesh_properties[0] = (rpr_mesh_info)RPR_MESH_UV_DIM;
		// mesh_properties[1] = (rpr_mesh_info)2;
		// mesh_properties[2] = (rpr_mesh_info)0;
		rpr_uint uvdim = TAHOE_SHAPE_DEFAULT_UVDIM;
		if ( mesh_properties )
		{
			for(int iArg=0; ; iArg++)
			{
				if ( (int64_t)mesh_properties[iArg] == 0 )
				{
					break;
				}
				else if ( (int64_t)mesh_properties[iArg] == RPR_MESH_UV_DIM )
				{
					iArg++;
					uvdim = (rpr_uint)mesh_properties[iArg];

					if ( uvdim < 2 || uvdim > 3 ) // Tahoe only supporting  UV and  UVW
					{
						throw FrException(__FILE__,__LINE__,RPR_ERROR_INVALID_PARAMETER, "",ctx);
					}
				
				}
				else
				{
					throw FrException(__FILE__,__LINE__,RPR_ERROR_INVALID_PARAMETER, "",ctx);
				}
				
			}
		}


		for(int i=0; i<numberOfTexCoordLayers; i++)
		{
			texcoords_array[i] = texcoords[i];
			num_texcoords_array[i] = num_texcoords[i];
			texcoord_stride_array[i] = texcoord_stride[i];
			texcoord_indices_array[i] = texcoord_indices_[i];
			tidx_stride_array[i] = tidx_stride_[i];
		}

	
		if (s_logger.IsTraceActivated())
		{
		

			unsigned long long wholeDataSize = 0;
	
			unsigned long long data1_size = num_vertices*vertex_stride;
			s_logger.TraceArg_Prepare__DATA(vertices, data1_size, "pData1");
			wholeDataSize += data1_size;
	
			unsigned long long data2_size = num_normals*normal_stride;
			s_logger.TraceArg_Prepare__DATA(normals, data2_size, "pData2");
			wholeDataSize += data2_size;
	
			unsigned long long data3_size = num_perVertexFlags*perVertexFlag_stride;
			s_logger.TraceArg_Prepare__DATA(perVertexFlag, data3_size, "pData3");
			wholeDataSize += data3_size;
	

			unsigned long long* ppData1_size = new unsigned long long[numberOfTexCoordLayers];
			for(int i=0; i<numberOfTexCoordLayers; i++)
			{
				ppData1_size[i] = num_texcoords[i] * texcoord_stride[i];
				wholeDataSize += ppData1_size[i];
			}
			s_logger.TraceArg_Prepare__PDATA((const void **)texcoords, ppData1_size, numberOfTexCoordLayers, "ppData1");
			delete[] ppData1_size; ppData1_size=NULL;
	

			unsigned long long data4_size = numberOfTexCoordLayers*sizeof(size_t);
			s_logger.TraceArg_Prepare__DATA(num_texcoords, data4_size, "pData4");
			wholeDataSize += data4_size;

			unsigned long long data5_size = numberOfTexCoordLayers*sizeof(rpr_int);
			s_logger.TraceArg_Prepare__DATA(texcoord_stride, data5_size, "pData5");
			wholeDataSize += data5_size;

			unsigned int nbTotalIndices = 0;
			for (int iFace = 0; iFace < num_faces; iFace++)
			{
				nbTotalIndices += num_face_vertices[iFace];
			}
	
			unsigned long long data6_size = nbTotalIndices*vidx_stride;
			s_logger.TraceArg_Prepare__DATA(vertex_indices, data6_size, "pData6");
			wholeDataSize += data6_size;
	
	
			unsigned long long data7_size = nbTotalIndices*nidx_stride;
			s_logger.TraceArg_Prepare__DATA(normal_indices, data7_size, "pData7");
			wholeDataSize += data7_size;
	
			unsigned long long* ppData2_size = new unsigned long long[numberOfTexCoordLayers];
			for(int i=0; i<numberOfTexCoordLayers; i++)
			{
				ppData2_size[i] = nbTotalIndices*tidx_stride_[i];
				wholeDataSize += ppData2_size[i];
			}
			s_logger.TraceArg_Prepare__PDATA((const void **)texcoord_indices_, ppData2_size, numberOfTexCoordLayers, "ppData2");
			delete[] ppData2_size; ppData2_size=NULL;
	
			unsigned long long data8_size = numberOfTexCoordLayers*sizeof(rpr_int);
			s_logger.TraceArg_Prepare__DATA(tidx_stride_, data8_size, "pData8");
			wholeDataSize += data8_size;

			unsigned long long data9_size = num_faces * sizeof(num_face_vertices[0]);
			s_logger.TraceArg_Prepare__DATA(num_face_vertices, data9_size, "pData9");
			wholeDataSize += data9_size;

		
			s_logger.printTrace("\r\n//Mesh creation\r\n");
			s_logger.printTrace("//size in data file for this mesh: %llu\r\n", wholeDataSize);
			s_logger.printTrace("//number of indices: %d\r\n", nbTotalIndices);

			s_logger.TraceArg_Prepare__rpr_mesh_info_P(mesh_properties);

			s_logger.Trace__FlushAllFiles(); // in case a crash, ensure trace informs the creation
		}




		MACRO__CHECK_ARGUMENT_TYPE(context,Context);


		// Count the total number of indices by iterating over face array and checking primitive type.
		size_t indicesCount = 0U;
		for (size_t i = 0; i < num_faces; ++i)
		{
			indicesCount += num_face_vertices[i];
		}

		// The goal here is to make copies of each of the incoming arrays as is.  So the arrays need to
		// be checked to see if any are interleaved.
		auto copiedArrays = CopyMeshArrays({
			{ reinterpret_cast<void*>(const_cast<float*>(vertices)), sizeof(float) * 3, static_cast<size_t>(vertex_stride), num_vertices, 0 },
			{ reinterpret_cast<void*>(const_cast<float*>(normals)), sizeof(float) * 3, static_cast<size_t>(normal_stride), num_normals, 1 },
			{ reinterpret_cast<void*>(const_cast<float*>(texcoords_array[0])), sizeof(float) * uvdim, static_cast<size_t>(texcoord_stride_array[0]), num_texcoords_array[0], 2 },
			{ reinterpret_cast<void*>(const_cast<float*>(texcoords_array[1])), sizeof(float) * uvdim, static_cast<size_t>(texcoord_stride_array[1]), num_texcoords_array[1], 3 },
		});

		auto copiedIndices = CopyMeshArrays({
			{ reinterpret_cast<void*>(const_cast<int*>(vertex_indices)), sizeof(int), static_cast<size_t>(vidx_stride), indicesCount, 0 },
			{ reinterpret_cast<void*>(const_cast<int*>(normal_indices)), sizeof(int), static_cast<size_t>(nidx_stride), indicesCount, 1 },
			{ reinterpret_cast<void*>(const_cast<int*>(texcoord_indices_array[0])), sizeof(int), static_cast<size_t>(tidx_stride_array[0]), indicesCount, 2 },
			{ reinterpret_cast<void*>(const_cast<int*>(texcoord_indices_array[1])), sizeof(int), static_cast<size_t>(tidx_stride_array[1]), indicesCount, 3 },
		});

		//rpr_int* copiedIndices = new rpr_int[indicesCount];
		//char* src = reinterpret_cast<char*>(const_cast<rpr_int*>(vertex_indices));
		//for (size_t i = 0; i < indicesCount; ++i)
		//{
			//copiedIndices[i] = *reinterpret_cast<rpr_int*>(src + i * vidx_stride);
	//	}

		rpr_int* numFaceVerticesCopy = new rpr_int[num_faces];
		memcpy(numFaceVerticesCopy, num_face_vertices, num_faces * sizeof(rpr_int));

		// Create the mesh node and add copied data.
		std::shared_ptr<FrSceneGraph> sceneGraph = ctx->GetProperty< std::shared_ptr<FrSceneGraph> >(FR_SCENEGRAPH);
		std::shared_ptr<FrRendererEncalps> renderer = ctx->GetProperty< std::shared_ptr<FrRendererEncalps> >(RPR_CONTEXT_ACTIVE_PLUGIN);
		FrNode* node = sceneGraph->CreateNode(NodeTypes::Mesh, [&](FrNode* n) {
			n->SetProperty(FR_NODE_CONTEXT, ctx);

			// Vertex arrays.
			for (auto& a : copiedArrays)
			{
				if (a.flags == 0)
				{
					n->SetProperty(RPR_MESH_VERTEX_ARRAY, reinterpret_cast<rpr_float*>(a.pointer));
					n->SetProperty(RPR_MESH_VERTEX_STRIDE, a.stride);
					n->SetProperty(RPR_MESH_VERTEX_COUNT, a.elementCount);
				}
				else if (a.flags == 1)
				{
					n->SetProperty(RPR_MESH_NORMAL_ARRAY, reinterpret_cast<rpr_float*>(a.pointer));
					n->SetProperty(RPR_MESH_NORMAL_STRIDE, a.stride);
					n->SetProperty(RPR_MESH_NORMAL_COUNT, a.elementCount);
				}
				else if (a.flags == 2)
				{
					n->SetProperty(RPR_MESH_UV_ARRAY, reinterpret_cast<rpr_float*>(a.pointer));
					n->SetProperty(RPR_MESH_UV_STRIDE, a.stride);
					n->SetProperty(RPR_MESH_UV_COUNT, a.elementCount);
				}
				else if (a.flags == 3)
				{
					n->SetProperty(RPR_MESH_UV2_ARRAY, reinterpret_cast<rpr_float*>(a.pointer));
					n->SetProperty(RPR_MESH_UV2_STRIDE, a.stride);
					n->SetProperty(RPR_MESH_UV2_COUNT, a.elementCount);
				}
			}

			// Index arrays.
			for (auto& a : copiedIndices)
			{
				if (a.flags == 0)
				{
					n->SetProperty(RPR_MESH_VERTEX_INDEX_ARRAY, reinterpret_cast<rpr_int*>(a.pointer));
					n->SetProperty(RPR_MESH_VERTEX_INDEX_STRIDE, a.stride);
				}
				else if (a.flags == 1)
				{
					n->SetProperty(RPR_MESH_NORMAL_INDEX_ARRAY, reinterpret_cast<rpr_int*>(a.pointer));
					n->SetProperty(RPR_MESH_NORMAL_INDEX_STRIDE, a.stride);
				}
				else if (a.flags == 2)
				{
					n->SetProperty(RPR_MESH_UV_INDEX_ARRAY, reinterpret_cast<rpr_int*>(a.pointer));
					n->SetProperty(RPR_MESH_UV_INDEX_STRIDE, a.stride);
				}
				else if (a.flags == 3)
				{
					n->SetProperty(RPR_MESH_UV2_INDEX_ARRAY, reinterpret_cast<rpr_int*>(a.pointer));
					n->SetProperty(RPR_MESH_UV2_INDEX_STRIDE, a.stride);
				}
			}

			//n->SetProperty(RPR_MESH_VERTEX_INDEX_ARRAY, copiedIndices);
			//n->SetProperty(RPR_MESH_VERTEX_INDEX_STRIDE, sizeof(rpr_int));

			// Faces.
			n->SetProperty(RPR_MESH_NUM_FACE_VERTICES_ARRAY, numFaceVerticesCopy);
			n->SetProperty(RPR_MESH_POLYGON_COUNT, num_faces);
            n->AddProperty(FR_SCENEGRAPH, sceneGraph);
			n->AddProperty(RPR_CONTEXT_ACTIVE_PLUGIN, renderer);
			n->SetProperty(RPR_MESH_UV_DIM, uvdim);
		});

		*out_mesh = node;

		
		if (s_logger.IsTraceActivated())
		{
			s_logger.Trace__NewFrObjectCreated(Logger::TRACE_ADDRESS_TYPE__SHAPE, *out_mesh);
			s_logger.Trace__DeclareFRobject("rpr_shape", "shape_0x", *out_mesh);
			s_logger.printTrace("status = ");
			TRACE__FUNCTION_OPEN

			s_logger.TraceArg__rpr_context(context); s_logger.TraceArg__COMMA();
			s_logger.TraceArg_Use__DATA_const_rpr_float_P("pData1"); s_logger.TraceArg__COMMA();
			s_logger.TraceArg__size_t(num_vertices); s_logger.TraceArg__COMMA();
			s_logger.TraceArg__rpr_int(vertex_stride);  s_logger.TraceArg__COMMA();
			s_logger.TraceArg_Use__DATA_const_rpr_float_P("pData2"); s_logger.TraceArg__COMMA();
			s_logger.TraceArg__size_t(num_normals); s_logger.TraceArg__COMMA();
			s_logger.TraceArg__rpr_int(normal_stride);  s_logger.TraceArg__COMMA();
			s_logger.TraceArg_Use__DATA_const_rpr_int_P("pData3"); s_logger.TraceArg__COMMA();
			s_logger.TraceArg__size_t(num_perVertexFlags); s_logger.TraceArg__COMMA();
			s_logger.TraceArg__rpr_int(perVertexFlag_stride);  s_logger.TraceArg__COMMA();
			s_logger.TraceArg__rpr_int(numberOfTexCoordLayers);  s_logger.TraceArg__COMMA();
			s_logger.TraceArg_Use__DATA_const_rpr_float_PP("&ppData1[0]"); s_logger.TraceArg__COMMA();
			s_logger.TraceArg_Use__DATA_const_size_t_P("pData4"); s_logger.TraceArg__COMMA();
			s_logger.TraceArg_Use__DATA_const_rpr_int_P("pData5"); s_logger.TraceArg__COMMA();
			s_logger.TraceArg_Use__DATA_const_rpr_int_P("pData6"); s_logger.TraceArg__COMMA();
			s_logger.TraceArg__rpr_int(vidx_stride);  s_logger.TraceArg__COMMA();
			s_logger.TraceArg_Use__DATA_const_rpr_int_P("pData7"); s_logger.TraceArg__COMMA();
			s_logger.TraceArg__rpr_int(nidx_stride);  s_logger.TraceArg__COMMA();
			s_logger.TraceArg_Use__DATA_const_rpr_int_PP("&ppData2[0]"); s_logger.TraceArg__COMMA();
			s_logger.TraceArg_Use__DATA_const_rpr_int_P("pData8"); s_logger.TraceArg__COMMA();
			s_logger.TraceArg_Use__DATA_const_rpr_int_P("pData9"); s_logger.TraceArg__COMMA();
			s_logger.TraceArg__size_t(num_faces); s_logger.TraceArg__COMMA();
			s_logger.TraceArg_Use__rpr_mesh_info_P(mesh_properties); s_logger.TraceArg__COMMA();
			s_logger.printTrace("&shape_0x%p", *out_mesh);
			s_logger.Trace__FunctionClose();
			s_logger.printTrace("status = rprObjectSetName (shape_0x%p,\"shape_0x%p\"); RPRTRACE_CHECK// added by tracing for debug\r\n\r\n",  *out_mesh ,  *out_mesh);
		}
		

		return RPR_SUCCESS;
	}
	catch (FrException& e)
	{
		TRACE__FUNCTION_FAILED(context); return e.GetErrorCode();
	}
}



rpr_int rprContextCreateImageFromFile(rpr_context context, const rpr_char* path, rpr_image* out_image)
{
	//in case of issues during creation, it's safer to return a null pointer, rather than let the pointer undefined.
	if ( out_image )
		*out_image = nullptr;
	

	s_logger.printTrace("//ImageFromFile creation : original path : ");
	s_logger.TraceArg__rpr_char_P(path);
	s_logger.printTrace("\r\n");
	s_logger.Trace__FlushAllFiles(); // in case a crash, ensure trace informs the creation

	API_PRINT("rprContextCreateImageFromFile", 1, context);
	FrNode* ctx = static_cast<FrNode*>(context);
	try
	{
		CHECK_NOT_NULL(ctx);
		MACRO__CHECK_ARGUMENT_TYPE(context,Context);


		std::shared_ptr<FrImageCache> imageCache = ctx->GetProperty<std::shared_ptr<FrImageCache>>(FR_CONTEXT_IMAGE_CACHE);
		FrImageCache::Entry entry = imageCache->LoadImage(ctx, path);

		int comp_size = 4;
		switch (entry.format.type)
		{
		case RPR_COMPONENT_TYPE_UINT8:
			comp_size = 1;
			break;
		case RPR_COMPONENT_TYPE_FLOAT16:
			comp_size = 2;
			break;
		case RPR_COMPONENT_TYPE_FLOAT32:
			comp_size = 4;
			break;
		}

		size_t dataSize = 0;
		if ( entry.desc.image_depth == 0 )
			dataSize = entry.desc.image_width * entry.desc.image_height							 * entry.format.num_components * comp_size;
		else
			dataSize = entry.desc.image_width * entry.desc.image_height * entry.desc.image_depth * entry.format.num_components * comp_size;

		std::shared_ptr<FrSceneGraph> sceneGraph = ctx->GetProperty< std::shared_ptr<FrSceneGraph> >(FR_SCENEGRAPH);
		std::shared_ptr<FrRendererEncalps> renderer = ctx->GetProperty< std::shared_ptr<FrRendererEncalps> >(RPR_CONTEXT_ACTIVE_PLUGIN);
		FrNode* node = sceneGraph->CreateNode(NodeTypes::Image, [&](FrNode* n) {
			n->SetProperty(FR_NODE_CONTEXT, ctx);
			n->SetProperty(RPR_IMAGE_FORMAT, entry.format);
			n->SetProperty(RPR_IMAGE_DESC, entry.desc);
			n->SetProperty(RPR_IMAGE_DATA, entry.data);
			n->SetProperty(RPR_IMAGE_DATA_SIZEBYTE, dataSize);
            n->AddProperty(FR_SCENEGRAPH, sceneGraph);
			n->AddProperty(RPR_CONTEXT_ACTIVE_PLUGIN, renderer);
		});


		//if RPR doesn't keep image DATA, then release the pointer
		#ifdef RPR_DOESNT_KEEP_TEXTURE_DATA
		auto& imgData = node->GetProperty< std::shared_ptr<unsigned char> >(RPR_IMAGE_DATA);
		imgData.reset();
		#endif


		*out_image = node;

		s_logger.PauseTracing(); // pause tracing, because calling exposed traced API function internally
		rprObjectSetName(node, path);
		s_logger.ContinueTracing();

		s_logger.Trace__NewFrObjectCreated(Logger::TRACE_ADDRESS_TYPE__IMAGE, *out_image);
		s_logger.Trace__DeclareFRobject("rpr_image", "image_0x", *out_image);
		std::string localFileTraceImage;
		s_logger.ImportFileInTraceFolders(path,localFileTraceImage);
		s_logger.printTrace("status = ");
		TRACE__FUNCTION_OPEN
		s_logger.TraceArg__rpr_context(context); s_logger.TraceArg__COMMA();
		s_logger.TraceArg__rpr_char_P(localFileTraceImage.c_str()); s_logger.TraceArg__COMMA();
		s_logger.printTrace("&image_0x%p", *out_image);
		s_logger.Trace__FunctionClose();
		s_logger.printTrace("status = rprObjectSetName (image_0x%p,\"image_0x%p\"); RPRTRACE_CHECK// added by tracing for debug\r\n",  *out_image ,  *out_image);

		return RPR_SUCCESS;
	}
	catch (FrException& e)
	{
		TRACE__FUNCTION_FAILED(context); return e.GetErrorCode();
	}
}

rpr_int rprCameraGetInfo(rpr_camera camera, rpr_camera_info camera_info, size_t size, void* data, size_t* size_ret)
{
	s_logger.printTrace("//"); //  ignore getInfo for the trace
	TRACE__FUNCTION_OPEN;
	s_logger.Trace__FunctionClose();

	API_PRINT("rprCameraGetInfo", 1, camera);
	FrNode* node = static_cast<FrNode*>(camera);

	try
	{
		CHECK_NOT_NULL(node);
		MACRO__CHECK_ARGUMENT_TYPE(camera,Camera);


		size_t requiredSize = 0;


		switch (camera_info)
		{
		case RPR_OBJECT_NAME:
		{
			std::string name = node->GetProperty<std::string>(RPR_OBJECT_NAME);
			requiredSize = name.size() + 1;
			break;
		}

		default:
			requiredSize = node->GetPropertyTypeSize(camera_info);
		}


		if (data)
		{
			if (size >= requiredSize)
			{
				switch (camera_info)
				{
				case RPR_CAMERA_TRANSFORM:
				{
					matrix& transform = node->GetProperty<matrix>(camera_info);
					rpr_float* dest = reinterpret_cast<rpr_float*>(data);
					for (int i = 0; i < 4; ++i)
					{
						for (int j = 0; j < 4; ++j)
						{
							*dest++ = transform.m[i][j];
						}
					}
					break;
				}

				case RPR_CAMERA_FSTOP:
				case RPR_CAMERA_RESPONSE:
				case RPR_CAMERA_EXPOSURE:
				case RPR_CAMERA_FOCAL_LENGTH:
				case RPR_CAMERA_ORTHO_WIDTH:
                case RPR_CAMERA_ORTHO_HEIGHT:
				case RPR_CAMERA_FOCUS_DISTANCE:
				case RPR_CAMERA_FOCAL_TILT:
				case RPR_CAMERA_IPD:
				case RPR_CAMERA_NEAR_PLANE:
				case RPR_CAMERA_FAR_PLANE:
					*reinterpret_cast<rpr_float*>(data) = node->GetProperty<rpr_float>(camera_info);
					break;

				case RPR_CAMERA_APERTURE_BLADES:
					*reinterpret_cast<rpr_uint*>(data) = node->GetProperty<rpr_uint>(camera_info);
					break;

				case RPR_CAMERA_SENSOR_SIZE:
				case RPR_CAMERA_LENS_SHIFT:
				case RPR_CAMERA_TILT_CORRECTION:
				{
					rpr_float2& sensorSize = node->GetProperty<rpr_float2>(camera_info);
					rpr_float* dest = reinterpret_cast<rpr_float*>(data);
					*dest++ = sensorSize.x;
					*dest = sensorSize.y;
					break;
				}

				case RPR_CAMERA_MODE:
					*reinterpret_cast<rpr_camera_mode*>(data) = node->GetProperty<rpr_camera_mode>(camera_info);
					break;

				case RPR_CAMERA_POSITION:
				case RPR_CAMERA_LOOKAT:
				case RPR_CAMERA_UP:
				{
					rpr_float3& sensorSize = node->GetProperty<rpr_float3>(camera_info);
					rpr_float* dest = reinterpret_cast<rpr_float*>(data);
					*dest++ = sensorSize.x;
					*dest++ = sensorSize.y;
					*dest = sensorSize.z;
					break;
				}

				case RPR_CAMERA_LINEAR_MOTION:
				{
					rpr_float* dest = reinterpret_cast<rpr_float*>(data);
					rpr_float4& linearMotion = node->GetProperty<rpr_float4>(RPR_CAMERA_LINEAR_MOTION);
					dest[0] = linearMotion[0];
					dest[1] = linearMotion[1];
					dest[2] = linearMotion[2];
					dest[3] = 0;
					break;
				}

				case RPR_CAMERA_ANGULAR_MOTION:
				{
					rpr_float* dest = reinterpret_cast<rpr_float*>(data);
					rpr_float4& angularMotion = node->GetProperty<rpr_float4>(RPR_CAMERA_ANGULAR_MOTION);
					dest[0] = angularMotion[0];
					dest[1] = angularMotion[1];
					dest[2] = angularMotion[2];
					dest[3] = angularMotion[3];
					break;
				}

				case RPR_OBJECT_NAME:
				{
					std::string name = node->GetProperty<std::string>(RPR_OBJECT_NAME);
					auto len = name.size();
					auto str = reinterpret_cast<rpr_char*>(data);
					std::copy(name.cbegin(), name.cend(), str);
					*(str + len) = 0;
					break;
				}

				}
			}
			else
			{
				throw FrException(__FILE__,__LINE__,RPR_ERROR_INVALID_PARAMETER, "",node);
			}
		}

		if (size_ret)
			*size_ret = requiredSize;
	}
	catch (FrException& e)
	{
		TRACE__FUNCTION_FAILED(node); return e.GetErrorCode();
	}

	return RPR_SUCCESS;
}

rpr_int rprCameraSetTransform(rpr_camera camera, rpr_bool transpose, rpr_float* transform)
{
	s_logger.TraceArg_Prepare__rpr_float_P16(transform);
	s_logger.printTrace("status = ");
	TRACE__FUNCTION_OPEN;
	s_logger.TraceArg__rpr_camera(camera); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_bool(transpose); s_logger.TraceArg__COMMA();
	s_logger.TraceArg_Use__rpr_float_P16(transform);
	s_logger.Trace__FunctionClose();

	API_PRINT("rprCameraSetTransform", 1, camera);
	FrNode* node = static_cast<FrNode*>(camera);

	try
	{
		CHECK_NOT_NULL(node);
		MACRO__CHECK_ARGUMENT_TYPE(camera,Camera);


		matrix m(transform[0], transform[1], transform[2], transform[3],
			transform[4], transform[5], transform[6], transform[7],
			transform[8], transform[9], transform[10], transform[11],
			transform[12], transform[13], transform[14], transform[15]
		);

		#ifdef RPR_FOR_BAIKAL
		if (!transpose)
		#else
		if (transpose)
		#endif
			m = m.transpose();

		node->SetProperty(RPR_CAMERA_TRANSFORM, m);

		//compute RPR_CAMERA_POSITION/RPR_CAMERA_LOOKAT/RPR_CAMERA_UP from   RPR_CAMERA_TRANSFORM 
		//so that we ensure both systems are synchronized

		//convention of RPR : an identity matrix produces a camera with: Y as up vector, and -Z as lookat vector.

		rpr_float3 pos =  rpr_float3(  m.m30,    m.m31,    m.m32   );

		rpr_float3 lookAtVec = rpr_float3(0.0, 0.0, -1.0);
		rpr_float3 lookAtVec_transf;
		lookAtVec_transf.x = lookAtVec.x * m.m00 + lookAtVec.y * m.m10 + lookAtVec.z * m.m20;
		lookAtVec_transf.y = lookAtVec.x * m.m01 + lookAtVec.y * m.m11 + lookAtVec.z * m.m21;
		lookAtVec_transf.z = lookAtVec.x * m.m02 + lookAtVec.y * m.m12 + lookAtVec.z * m.m22;

		rpr_float3 upVec = rpr_float3(0.0, 1.0, 0.0);
		rpr_float3 upVec_transf;
		upVec_transf.x = upVec.x * m.m00 + upVec.y * m.m10 + upVec.z * m.m20;
		upVec_transf.y = upVec.x * m.m01 + upVec.y * m.m11 + upVec.z * m.m21;
		upVec_transf.z = upVec.x * m.m02 + upVec.y * m.m12 + upVec.z * m.m22;

		rpr_float3 at =  pos + lookAtVec_transf;
		rpr_float3 up =  upVec_transf;

		node->SetProperty(RPR_CAMERA_POSITION, pos);
		node->SetProperty(RPR_CAMERA_LOOKAT, at);
		node->SetProperty(RPR_CAMERA_UP, up);

		return RPR_SUCCESS;
	}
	catch (FrException& e)
	{
		TRACE__FUNCTION_FAILED(node); return e.GetErrorCode();
	}
}



rpr_int rprCameraLookAt(rpr_camera camera, rpr_float posx, rpr_float posy, rpr_float posz,
	rpr_float atx, rpr_float aty, rpr_float atz,
	rpr_float upx, rpr_float upy, rpr_float upz)
{
	s_logger.printTrace("status = ");
	TRACE__FUNCTION_OPEN;
	s_logger.TraceArg__rpr_camera(camera); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_float(posx); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_float(posy); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_float(posz); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_float(atx); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_float(aty); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_float(atz); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_float(upx); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_float(upy); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_float(upz);
	s_logger.Trace__FunctionClose();

	API_PRINT("rprCameraLookAt", 1, camera);
	FrNode* node = static_cast<FrNode*>(camera);

	try
	{
		CHECK_NOT_NULL(node);
		MACRO__CHECK_ARGUMENT_TYPE(camera,Camera);


		rpr_float3 pos = rpr_float3(posx, posy, posz);
		rpr_float3 at = rpr_float3(atx, aty, atz);
		rpr_float3 up = rpr_float3(upx, upy, upz);

		node->SetProperty(RPR_CAMERA_POSITION, pos);
		node->SetProperty(RPR_CAMERA_LOOKAT, at);
		node->SetProperty(RPR_CAMERA_UP, up);


		//compute  RPR_CAMERA_TRANSFORM  from  RPR_CAMERA_POSITION/RPR_CAMERA_LOOKAT/RPR_CAMERA_UP
		//so that we ensure both systems are synchronized
		rpr_float3 directionVector =  normalize(at - pos);
		rpr_float3 left = normalize(cross(up,directionVector));
		matrix m(
			left.x,             left.y,             left.z,             0.0,
			up.x,               up.y,               up.z,               0.0,
			-directionVector.x, -directionVector.y, -directionVector.z, 0.0,
			pos.x,              pos.y,              pos.z,              1.0
			);
		node->SetProperty(RPR_CAMERA_TRANSFORM, m);

		return RPR_SUCCESS;
	}
	catch (FrException& e)
	{
		TRACE__FUNCTION_FAILED(node); return e.GetErrorCode();
	}
}

rpr_int rprCameraSetFocalLength(rpr_camera camera, rpr_float flength)
{
	s_logger.printTrace("status = ");
	TRACE__FUNCTION_OPEN;
	s_logger.TraceArg__rpr_camera(camera); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_float(flength);
	s_logger.Trace__FunctionClose();

	API_PRINT("rprCameraSetFocalLength", 1, camera);
	FrNode* node = static_cast<FrNode*>(camera);

	try
	{
		CHECK_NOT_NULL(node);
		MACRO__CHECK_ARGUMENT_TYPE(camera,Camera);


		node->SetProperty(RPR_CAMERA_FOCAL_LENGTH, flength);
		return RPR_SUCCESS;
	}
	catch (FrException& e)
	{
		TRACE__FUNCTION_FAILED(node); return e.GetErrorCode();
	}
}

rpr_int rprCameraSetFocusDistance(rpr_camera camera, rpr_float fdist)
{
	s_logger.printTrace("status = ");
	TRACE__FUNCTION_OPEN;
	s_logger.TraceArg__rpr_camera(camera); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_float(fdist);
	s_logger.Trace__FunctionClose();

	API_PRINT("rprCameraSetFocusDistance", 1, camera);
	FrNode* node = static_cast<FrNode*>(camera);

	try
	{
		CHECK_NOT_NULL(node);
		MACRO__CHECK_ARGUMENT_TYPE(camera,Camera);

		// FIR-791 -  AMDBLENDER-572
		// we have BSOD when fdist = 0.0f
		const float fdist_minimum_value = 0.001f;
		if ( fdist < fdist_minimum_value )
		{
			throw FrException(__FILE__,__LINE__,RPR_ERROR_INVALID_PARAMETER, "fdist 0 is forbidden",camera);
		}

		node->SetProperty(RPR_CAMERA_FOCUS_DISTANCE, fdist);
		return RPR_SUCCESS;
	}
	catch (FrException& e)
	{
		TRACE__FUNCTION_FAILED(node); return e.GetErrorCode();
	}

}

rpr_int rprCameraSetFStop(rpr_camera camera, rpr_float fstop)
{
	s_logger.printTrace("status = ");
	TRACE__FUNCTION_OPEN;
	s_logger.TraceArg__rpr_camera(camera); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_float(fstop);
	s_logger.Trace__FunctionClose();

	API_PRINT("rprCameraSetFStop", 1, camera);
	FrNode* node = static_cast<FrNode*>(camera);

	try
	{
		CHECK_NOT_NULL(node);
		MACRO__CHECK_ARGUMENT_TYPE(camera,Camera);


		node->SetProperty(RPR_CAMERA_FSTOP, fstop);
		return RPR_SUCCESS;
	}
	catch (FrException& e)
	{
		TRACE__FUNCTION_FAILED(node); return e.GetErrorCode();
	}
}


rpr_int rprCameraSetSensorSize(rpr_camera camera, rpr_float width, rpr_float height)
{
	s_logger.printTrace("status = ");
	TRACE__FUNCTION_OPEN;
	s_logger.TraceArg__rpr_camera(camera); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_float(width); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_float(height);
	s_logger.Trace__FunctionClose();

	API_PRINT("rprCameraSetSensorSize", 1, camera);
	FrNode* node = static_cast<FrNode*>(camera);

	try
	{
		CHECK_NOT_NULL(node);
		MACRO__CHECK_ARGUMENT_TYPE(camera,Camera);


		node->SetProperty(RPR_CAMERA_SENSOR_SIZE, rpr_float2( width, height ));
		return RPR_SUCCESS;
	}
	catch (FrException& e)
	{
		TRACE__FUNCTION_FAILED(node); return e.GetErrorCode();
	}
}

rpr_int rprCameraSetApertureBlades(rpr_camera camera, rpr_uint num_blades)
{
	s_logger.printTrace("status = ");
	TRACE__FUNCTION_OPEN;
	s_logger.TraceArg__rpr_camera(camera); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_uint(num_blades);
	s_logger.Trace__FunctionClose();

	API_PRINT("rprCameraSetApertureBlades", 1, camera);
	FrNode* node = static_cast<FrNode*>(camera);

	try
	{
		CHECK_NOT_NULL(node);
		MACRO__CHECK_ARGUMENT_TYPE(camera,Camera);


		node->SetProperty(RPR_CAMERA_APERTURE_BLADES, num_blades);
		return RPR_SUCCESS;
	}
	catch (FrException& e)
	{
		TRACE__FUNCTION_FAILED(node); return e.GetErrorCode();
	}
}

rpr_int rprCameraSetExposure(rpr_camera camera, rpr_float exposure)
{
	s_logger.printTrace("status = ");
	TRACE__FUNCTION_OPEN;
	s_logger.TraceArg__rpr_camera(camera); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_float(exposure);
	s_logger.Trace__FunctionClose();

	API_PRINT("rprCameraSetExposure", 1, camera);
	FrNode* node = static_cast<FrNode*>(camera);

	try
	{
		CHECK_NOT_NULL(node);
		MACRO__CHECK_ARGUMENT_TYPE(camera,Camera);


		node->SetProperty(RPR_CAMERA_EXPOSURE, exposure);
		return RPR_SUCCESS;
	}
	catch (FrException& e)
	{
		TRACE__FUNCTION_FAILED(node); return e.GetErrorCode();
	}
}

rpr_int rprCameraSetMode(rpr_camera camera, rpr_camera_mode mode)
{
	s_logger.printTrace("status = ");
	TRACE__FUNCTION_OPEN;
	s_logger.TraceArg__rpr_camera(camera); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_camera_mode(mode);
	s_logger.Trace__FunctionClose();

	API_PRINT("rprCameraSetMode", 1, camera);
	FrNode* node = static_cast<FrNode*>(camera);

	try
	{
		CHECK_NOT_NULL(node);
		MACRO__CHECK_ARGUMENT_TYPE(camera,Camera);



		node->SetProperty(RPR_CAMERA_MODE, mode);
		return RPR_SUCCESS;
	}
	catch (FrException& e)
	{
		TRACE__FUNCTION_FAILED(node); return e.GetErrorCode();
	}

}

rpr_int rprCameraSetOrthoWidth(rpr_camera camera, rpr_float width)
{
	s_logger.printTrace("status = ");
	TRACE__FUNCTION_OPEN;
	s_logger.TraceArg__rpr_camera(camera); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_float(width);
	s_logger.Trace__FunctionClose();

	API_PRINT("rprCameraSetOrthoWidth", 1, camera);
	FrNode* node = static_cast<FrNode*>(camera);

	try
	{
		CHECK_NOT_NULL(node);
		MACRO__CHECK_ARGUMENT_TYPE(camera,Camera);


		node->SetProperty(RPR_CAMERA_ORTHO_WIDTH, width);
		return RPR_SUCCESS;
	}
	catch (FrException& e)
	{
		return e.GetErrorCode();
	}
}

rpr_int rprCameraSetFocalTilt(rpr_camera camera, rpr_float tilt)
{
	s_logger.printTrace("status = ");
	TRACE__FUNCTION_OPEN;
	s_logger.TraceArg__rpr_camera(camera); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_float(tilt);
	s_logger.Trace__FunctionClose();

	API_PRINT("rprCameraSetFocalTilt", 1, camera);
	FrNode* node = static_cast<FrNode*>(camera);

	try
	{
		CHECK_NOT_NULL(node);
		MACRO__CHECK_ARGUMENT_TYPE(camera,Camera);


		node->SetProperty(RPR_CAMERA_FOCAL_TILT, tilt);
		return RPR_SUCCESS;
	}
	catch (FrException& e)
	{
		TRACE__FUNCTION_FAILED(node); return e.GetErrorCode();
	}
}
rpr_int rprCameraSetIPD(rpr_camera camera, rpr_float ipd)
{
	s_logger.printTrace("status = ");
	TRACE__FUNCTION_OPEN;
	s_logger.TraceArg__rpr_camera(camera); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_float(ipd);
	s_logger.Trace__FunctionClose();

	API_PRINT("rprCameraSetIPD", 1, camera);
	FrNode* node = static_cast<FrNode*>(camera);

	try
	{
		CHECK_NOT_NULL(node);
		MACRO__CHECK_ARGUMENT_TYPE(camera,Camera);


		node->SetProperty(RPR_CAMERA_IPD, ipd);
		return RPR_SUCCESS;
	}
	catch (FrException& e)
	{
		TRACE__FUNCTION_FAILED(node); return e.GetErrorCode();
	}
}
rpr_int rprCameraSetLensShift(rpr_camera camera, rpr_float shiftX, rpr_float shiftY)
{
	s_logger.printTrace("status = ");
	TRACE__FUNCTION_OPEN;
	s_logger.TraceArg__rpr_camera(camera); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_float(shiftX);  s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_float(shiftY);
	s_logger.Trace__FunctionClose();

	API_PRINT("rprCameraSetLensShift", 1, camera);
	FrNode* node = static_cast<FrNode*>(camera);

	try
	{
		CHECK_NOT_NULL(node);
		MACRO__CHECK_ARGUMENT_TYPE(camera,Camera);


		node->SetProperty(RPR_CAMERA_LENS_SHIFT, rpr_float2( shiftX, shiftY ) );
		return RPR_SUCCESS;
	}
	catch (FrException& e)
	{
		TRACE__FUNCTION_FAILED(node); return e.GetErrorCode();
	}
}
rpr_int rprCameraSetTiltCorrection(rpr_camera camera, rpr_float tiltX, rpr_float tiltY)
{
	s_logger.printTrace("status = ");
	TRACE__FUNCTION_OPEN;
	s_logger.TraceArg__rpr_camera(camera); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_float(tiltX);  s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_float(tiltY);
	s_logger.Trace__FunctionClose();

	API_PRINT("rprCameraSetTiltCorrection", 1, camera);
	FrNode* node = static_cast<FrNode*>(camera);

	try
	{
		CHECK_NOT_NULL(node);
		MACRO__CHECK_ARGUMENT_TYPE(camera,Camera);


		node->SetProperty(RPR_CAMERA_TILT_CORRECTION, rpr_float2( tiltX, tiltY ) );
		return RPR_SUCCESS;
	}
	catch (FrException& e)
	{
		TRACE__FUNCTION_FAILED(node); return e.GetErrorCode();
	}
}


rpr_int rprCameraSetOrthoHeight(rpr_camera camera, rpr_float height)
{
    s_logger.printTrace("status = ");
    TRACE__FUNCTION_OPEN;
    s_logger.TraceArg__rpr_camera(camera); s_logger.TraceArg__COMMA();
    s_logger.TraceArg__rpr_float(height);
    s_logger.Trace__FunctionClose();

    API_PRINT("rprCameraSetOrthoHeight", 1, camera);
    FrNode* node = static_cast<FrNode*>(camera);

    try
    {
		CHECK_NOT_NULL(node);
		MACRO__CHECK_ARGUMENT_TYPE(camera,Camera);


        node->SetProperty(RPR_CAMERA_ORTHO_HEIGHT, height);
        return RPR_SUCCESS;
    }
    catch (FrException& e)
    {
        TRACE__FUNCTION_FAILED(node); return e.GetErrorCode();
    }
}

rpr_int rprCameraSetNearPlane(rpr_camera camera, rpr_float near)
{
	s_logger.printTrace("status = ");
	TRACE__FUNCTION_OPEN;
	s_logger.TraceArg__rpr_camera(camera); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_float(near);
	s_logger.Trace__FunctionClose();

	API_PRINT("rprCameraSetNearPlane", 1, camera);
	FrNode* node = static_cast<FrNode*>(camera);

	try
	{
		CHECK_NOT_NULL(node);
		node->SetProperty(RPR_CAMERA_NEAR_PLANE, near);
		return RPR_SUCCESS;
	}
	catch (FrException& e)
	{
		TRACE__FUNCTION_FAILED(node); return e.GetErrorCode();
	}
}

rpr_int rprCameraSetFarPlane(rpr_camera camera, rpr_float far)
{
	s_logger.printTrace("status = ");
	TRACE__FUNCTION_OPEN;
	s_logger.TraceArg__rpr_camera(camera); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_float(far);
	s_logger.Trace__FunctionClose();

	API_PRINT("rprCameraSetFarPlane", 1, camera);
	FrNode* node = static_cast<FrNode*>(camera);

	try
	{
		CHECK_NOT_NULL(node);
		node->SetProperty(RPR_CAMERA_FAR_PLANE, far);
		return RPR_SUCCESS;
	}
	catch (FrException& e)
	{
		TRACE__FUNCTION_FAILED(node); return e.GetErrorCode();
	}
}

rpr_int rprBufferGetInfo(rpr_buffer buffer, rpr_buffer_info buffer_info, size_t size, void * data, size_t * size_ret)
{
	s_logger.printTrace("//"); //  ignore getInfo for the trace
	TRACE__FUNCTION_OPEN;
	s_logger.Trace__FunctionClose();

	FrNode* img = static_cast<FrNode*>(buffer);

	try
	{
		CHECK_NOT_NULL(img);
		MACRO__CHECK_ARGUMENT_TYPE(buffer,Buffer);


		size_t requiredSize = img->GetPropertyTypeSize(buffer_info);
		if (buffer_info == RPR_BUFFER_DATA)
		{

			rpr_buffer_desc buffer_desc = img->GetProperty< rpr_buffer_desc >(RPR_BUFFER_DESC);

			int sizeOneComponent = 0;
			if ( buffer_desc.element_type == RPR_BUFFER_ELEMENT_TYPE_INT32 )
			{
				sizeOneComponent = 4;
			}
			else if ( buffer_desc.element_type == RPR_BUFFER_ELEMENT_TYPE_FLOAT32 )
			{
				sizeOneComponent = 4;
			}
			else
			{
				throw FrException(__FILE__,__LINE__,RPR_ERROR_INVALID_PARAMETER, "Invalid element_type",img);
			}
		
			requiredSize = buffer_desc.nb_element * buffer_desc.element_channel_size * sizeOneComponent;

		}
		else if (buffer_info == RPR_OBJECT_NAME)
		{
			std::string name = img->GetProperty<std::string>(RPR_OBJECT_NAME);
			requiredSize = name.size() + 1;
		}

		if (data)
		{
			if (requiredSize <= size)
			{
				switch (buffer_info)
				{
				case RPR_BUFFER_DESC:
					*reinterpret_cast<rpr_buffer_desc*>(data) = img->GetProperty<rpr_buffer_desc>(RPR_BUFFER_DESC);
					break;

				case RPR_BUFFER_DATA:
				{
					auto& imgData = img->GetProperty< std::shared_ptr<unsigned char> >(RPR_BUFFER_DATA);
					//std::copy(imgData.cbegin(), imgData.cend(), reinterpret_cast<char*>(data));
					memcpy(data,imgData.get(),requiredSize);
					break;
				}

				case RPR_OBJECT_NAME:
				{
					std::string name = img->GetProperty<std::string>(RPR_OBJECT_NAME);
					auto len = name.size();
					auto str = reinterpret_cast<rpr_char*>(data);
					std::copy(name.cbegin(), name.cend(), str);
					*(str + len) = 0;
					break;
				}

				default:
					throw FrException(__FILE__,__LINE__,RPR_ERROR_INVALID_PARAMETER, "Invalid buffer info requested",buffer);
				}
			}
			else
			{
				throw FrException(__FILE__,__LINE__,RPR_ERROR_INVALID_PARAMETER, "",img);
			}
		}

		if (size_ret)
			*size_ret = requiredSize;
	}
	catch (FrException& e)
	{
		TRACE__FUNCTION_FAILED(img); return e.GetErrorCode();
	}

	return RPR_SUCCESS;
}

rpr_int rprImageGetInfo(rpr_image image, rpr_image_info image_info, size_t size, void* data, size_t* size_ret)
{

	s_logger.printTrace("//"); //  ignore getInfo for the trace
	TRACE__FUNCTION_OPEN;
	s_logger.Trace__FunctionClose();

	//FrImageWrapper* imgWrapper = static_cast<FrImageWrapper*>(image);
	//FrNode* img = nullptr;// imgWrapper->ptr.get();
	FrNode* img = static_cast<FrNode*>(image);

	try
	{
		CHECK_NOT_NULL(img);
		MACRO__CHECK_ARGUMENT_TYPE(image,Image);


		size_t requiredSize = img->GetPropertyTypeSize(image_info);
		if (image_info == RPR_IMAGE_DATA)
		{
			//requiredSize = img->GetProperty<std::vector<rpr_char>>(RPR_IMAGE_DATA).size();
			requiredSize = img->GetProperty< size_t >(RPR_IMAGE_DATA_SIZEBYTE);
		}
		else if (image_info == RPR_IMAGE_DATA_SIZEBYTE)
		{
			requiredSize = sizeof(size_t);
		}
		else if (image_info == RPR_OBJECT_NAME)
		{
			std::string name = img->GetProperty<std::string>(RPR_OBJECT_NAME);
			requiredSize = name.size() + 1;
		}

		if (data)
		{
			if (requiredSize <= size)
			{
				switch (image_info)
				{
				case RPR_IMAGE_DESC:
					*reinterpret_cast<rpr_image_desc*>(data) = img->GetProperty<rpr_image_desc>(RPR_IMAGE_DESC);
					break;

				case RPR_IMAGE_FORMAT:
					*reinterpret_cast<rpr_image_format*>(data) = img->GetProperty<rpr_image_format>(RPR_IMAGE_FORMAT);
					break;

				case RPR_IMAGE_DATA:
				{
				#ifndef RPR_DOESNT_KEEP_TEXTURE_DATA

					auto& imgData = img->GetProperty< std::shared_ptr<unsigned char> >(RPR_IMAGE_DATA);
					//std::copy(imgData.cbegin(), imgData.cend(), reinterpret_cast<char*>(data));
					memcpy(data,imgData.get(),requiredSize);
					

				#else

					FrNode* ctx = img->GetProperty<FrNode*>(FR_NODE_CONTEXT);
					if (!ctx)
						throw FrException(__FILE__, __LINE__, RPR_ERROR_INVALID_PARAMETER, "no ctx", ctx);
					std::shared_ptr<FrRendererEncalps> renderer = ctx->GetProperty< std::shared_ptr<FrRendererEncalps> >(RPR_CONTEXT_ACTIVE_PLUGIN);
					if (!renderer->m_FrRenderer)
						throw FrException(__FILE__, __LINE__, RPR_ERROR_INVALID_PARAMETER, "no renderer", ctx);
					renderer->m_FrRenderer->GetImageData(img,data);

				#endif

					break;
				}

				case RPR_IMAGE_DATA_SIZEBYTE:
				{
					*reinterpret_cast<size_t*>(data) = img->GetProperty<size_t>(RPR_IMAGE_DATA_SIZEBYTE);
					break;
				}

				case RPR_OBJECT_NAME:
				{
					std::string name = img->GetProperty<std::string>(RPR_OBJECT_NAME);
					auto len = name.size();
					auto str = reinterpret_cast<rpr_char*>(data);
					std::copy(name.cbegin(), name.cend(), str);
					*(str + len) = 0;
					break;
				}

				case RPR_IMAGE_WRAP:
				{
					*reinterpret_cast<rpr_uint*>(data) = img->GetProperty<rpr_uint>(RPR_IMAGE_WRAP);
					break;
				}

				case RPR_IMAGE_FILTER:
				{
					*reinterpret_cast<rpr_uint*>(data) = img->GetProperty<rpr_uint>(RPR_IMAGE_FILTER);
					break;
				}

				case RPR_IMAGE_GAMMA:
				{
					*reinterpret_cast<rpr_float*>(data) = img->GetProperty<rpr_float>(RPR_IMAGE_GAMMA);
					break;
				}

				case RPR_IMAGE_MIPMAP_ENABLED:
				{
					*reinterpret_cast<rpr_bool*>(data) = img->GetProperty<rpr_bool>(RPR_IMAGE_MIPMAP_ENABLED);
					break;
				}

				default:
					throw FrException(__FILE__,__LINE__,RPR_ERROR_INVALID_PARAMETER, "Invalid image info requested",image);
				}
			}
			else
			{
				throw FrException(__FILE__,__LINE__,RPR_ERROR_INVALID_PARAMETER, "",img);
			}
		}

		if (size_ret)
			*size_ret = requiredSize;
	}
	catch (FrException& e)
	{
		TRACE__FUNCTION_FAILED(img); return e.GetErrorCode();
	}

	return RPR_SUCCESS;
}

rpr_int rprShapeSetMaterial(rpr_shape shape, rpr_material_node material)
{
	s_logger.printTrace("status = ");
	TRACE__FUNCTION_OPEN;
	s_logger.TraceArg__rpr_shape(shape); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_material_node(material);
	s_logger.Trace__FunctionClose();

	API_PRINT("rprShapeSetMaterial", 1, shape);
	FrNode* node = static_cast<FrNode*>(shape);
	FrNode* nodeMaterial = static_cast<FrNode*>(material);

	try
	{
		CHECK_NOT_NULL(node);

		//we can't use Emissive material on Instance.
		//for the moment, just break here for debug purpose, in the future, we should return an error message ?
		#ifdef _DEBUG
		#ifdef _WIN32
		if ( node && material )
		{
			auto typeMaterial = nodeMaterial->GetProperty<rpr_material_node_type>(RPR_MATERIAL_NODE_TYPE);
			auto typeShape = node->GetProperty<rpr_material_node_type>(RPR_SHAPE_TYPE);
			if (  typeMaterial == RPR_MATERIAL_NODE_EMISSIVE 
				&& typeShape == RPR_SHAPE_TYPE_INSTANCE )
			{
				__debugbreak();
			}
			
		}
		#endif
		#endif



		MACRO__CHECK_ARGUMENT_TYPE_ALLSHAPES(shape);
		MACRO__CHECK_ARGUMENT_TYPE(material,Material);


		node->SetProperty(RPR_SHAPE_MATERIAL, static_cast<FrNode*>(material));
		return RPR_SUCCESS;
	}
	catch (FrException& e)
	{
		TRACE__FUNCTION_FAILED(node); return e.GetErrorCode();
	}
}


rpr_int rprShapeSetHeteroVolume(rpr_shape shape, rpr_hetero_volume volume)
{
	s_logger.printTrace("status = ");
	TRACE__FUNCTION_OPEN;
	s_logger.TraceArg__rpr_shape(shape); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_hetero_volume(volume);
	s_logger.Trace__FunctionClose();

	API_PRINT("rprShapeSetHeteroVolume", 1, shape);
	FrNode* node = static_cast<FrNode*>(shape);

	try
	{
		CHECK_NOT_NULL(node);
		MACRO__CHECK_ARGUMENT_TYPE_ALLSHAPES(shape);
		MACRO__CHECK_ARGUMENT_TYPE(volume,HeteroVolume);

		node->SetProperty(RPR_SHAPE_HETERO_VOLUME, static_cast<FrNode*>(volume));
		return RPR_SUCCESS;
	}
	catch (FrException& e)
	{
		TRACE__FUNCTION_FAILED(node); return e.GetErrorCode();
	}
}

////////////////////////////////////////////////////////////////////////////////////////
rpr_int rprShapeSetMaterialFaces(rpr_shape shape, rpr_material_node material, rpr_int* face_indices, size_t num_faces)
{
	unsigned long long data1_size = num_faces;
	s_logger.TraceArg_Prepare__DATA(face_indices, data1_size, "face_indices");
	s_logger.printTrace("status = ");
	TRACE__FUNCTION_OPEN
	s_logger.TraceArg__rpr_shape(shape); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_material_node(material); s_logger.TraceArg__COMMA();
	s_logger.TraceArg_Use__DATA_const_rpr_float_P("face_indices"); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__size_t(num_faces);
	s_logger.Trace__FunctionClose();

	API_PRINT("rprShapeSetMaterialFaces", 1, shape);
	FrNode* node = static_cast<FrNode*>(shape);
	FrNode* matNode = static_cast<FrNode*>(material);

	try
	{
		CHECK_NOT_NULL(node);

		std::unordered_map<rpr_int, FrNode*>& materialsPerFace = node->GetProperty<std::unordered_map<rpr_int, FrNode*>>(RPR_SHAPE_MATERIALS_PER_FACE);

		for (int i = 0; i < num_faces; i++)
		{
			materialsPerFace[face_indices[i]] = matNode;
		}

		node->SetProperty(RPR_SHAPE_MATERIALS_PER_FACE, materialsPerFace);
		return RPR_SUCCESS;
	}
	catch (FrException& e)
	{
		return e.GetErrorCode();
	}
}


rpr_int rprShapeSetVolumeMaterial(rpr_shape shape, rpr_material_node material)
{
	#ifdef RPR_FOR_BAIKAL
	return RPR_SUCCESS; // unsupported
	#endif

    s_logger.printTrace("status = ");
    TRACE__FUNCTION_OPEN;
    s_logger.TraceArg__rpr_shape(shape); s_logger.TraceArg__COMMA();
    s_logger.TraceArg__rpr_material_node(material);
    s_logger.Trace__FunctionClose();

    API_PRINT("rprShapeSetVolumeMaterial", 1, shape);
    FrNode* node = static_cast<FrNode*>(shape);

    try
    {
		CHECK_NOT_NULL(node);
		MACRO__CHECK_ARGUMENT_TYPE_ALLSHAPES(shape);
		MACRO__CHECK_ARGUMENT_TYPE(material,Material);


        node->SetProperty(RPR_SHAPE_VOLUME_MATERIAL, static_cast<FrNode*>(material));
        return RPR_SUCCESS;
    }
    catch (FrException& e)
    {
        TRACE__FUNCTION_FAILED(node); return e.GetErrorCode();
    }
}

rpr_int rprShapeSetLinearMotion(rpr_shape shape, rpr_float x, rpr_float y, rpr_float z)
{
	s_logger.printTrace("status = ");
	TRACE__FUNCTION_OPEN;
	s_logger.TraceArg__rpr_shape(shape); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_float(x); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_float(y); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_float(z);
	s_logger.Trace__FunctionClose();

	API_PRINT("rprShapeSetLinearMotion", 1, shape);
	FrNode* node = static_cast<FrNode*>(shape);

	try
	{
		CHECK_NOT_NULL(node);
		MACRO__CHECK_ARGUMENT_TYPE_ALLSHAPES(shape);


		node->SetProperty(RPR_SHAPE_LINEAR_MOTION, rpr_float4(x, y, z, 0.0f));
		return RPR_SUCCESS;
	}
	catch (FrException& e)
	{
		TRACE__FUNCTION_FAILED(node); return e.GetErrorCode();
	}
}

rpr_int rprCameraSetLinearMotion(rpr_camera camera, rpr_float x, rpr_float y, rpr_float z)
{
	s_logger.printTrace("status = ");
	TRACE__FUNCTION_OPEN;
	s_logger.TraceArg__rpr_camera(camera); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_float(x); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_float(y); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_float(z);
	s_logger.Trace__FunctionClose();

	API_PRINT("rprCameraSetLinearMotion", 1, camera);
	FrNode* node = static_cast<FrNode*>(camera);

	try
	{
		CHECK_NOT_NULL(node);
		MACRO__CHECK_ARGUMENT_TYPE(camera, Camera);


		node->SetProperty(RPR_CAMERA_LINEAR_MOTION, rpr_float4(x, y, z, 0.0f));
		return RPR_SUCCESS;
	}
	catch (FrException& e)
	{
		TRACE__FUNCTION_FAILED(node); return e.GetErrorCode();
	}
}

rpr_int rprCameraSetAngularMotion(rpr_camera camera, rpr_float x, rpr_float y, rpr_float z, rpr_float w)
{
	s_logger.printTrace("status = ");
	TRACE__FUNCTION_OPEN;
	s_logger.TraceArg__rpr_camera(camera); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_float(x); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_float(y); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_float(z); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_float(w);
	s_logger.Trace__FunctionClose();

	API_PRINT("rprCameraSetAngularMotion", 1, camera);
	FrNode* node = static_cast<FrNode*>(camera);

	try
	{
		CHECK_NOT_NULL(node);
		MACRO__CHECK_ARGUMENT_TYPE(camera, Camera);


		node->SetProperty(RPR_CAMERA_ANGULAR_MOTION, rpr_float4(x, y, z, w));
		return RPR_SUCCESS;
	}
	catch (FrException& e)
	{
		TRACE__FUNCTION_FAILED(node); return e.GetErrorCode();
	}
}

rpr_int rprShapeSetAngularMotion(rpr_shape shape, rpr_float x, rpr_float y, rpr_float z, rpr_float w)
{
	s_logger.printTrace("status = ");
	TRACE__FUNCTION_OPEN;
	s_logger.TraceArg__rpr_shape(shape); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_float(x); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_float(y); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_float(z); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_float(w);
	s_logger.Trace__FunctionClose();

	API_PRINT("rprShapeSetAngularMotion", 1, shape);
	FrNode* node = static_cast<FrNode*>(shape);

	try
	{
		CHECK_NOT_NULL(node);
		MACRO__CHECK_ARGUMENT_TYPE_ALLSHAPES(shape);


		float3 axis = float3(x, y, z);
		if (sqrt(axis.sqnorm()) < 1e-3f)
			throw FrException(__FILE__,__LINE__,RPR_ERROR_INVALID_PARAMETER, "",node);

		node->SetProperty(RPR_SHAPE_ANGULAR_MOTION, rpr_float4(x, y, z, w));
		return RPR_SUCCESS;
	}
	catch (FrException& e)
	{
		TRACE__FUNCTION_FAILED(node); return e.GetErrorCode();
	}
}

rpr_int rprShapeSetScaleMotion(rpr_shape shape, rpr_float x, rpr_float y, rpr_float z)
{
	s_logger.printTrace("status = ");
	TRACE__FUNCTION_OPEN;
	s_logger.TraceArg__rpr_shape(shape); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_float(x); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_float(y); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_float(z);
	s_logger.Trace__FunctionClose();

	API_PRINT("rprShapeSetScaleMotion", 1, shape);
	FrNode* node = static_cast<FrNode*>(shape);

	try
	{
		CHECK_NOT_NULL(node);
		MACRO__CHECK_ARGUMENT_TYPE_ALLSHAPES(shape);

		node->SetProperty(RPR_SHAPE_SCALE_MOTION, rpr_float4(x, y, z, 1.0f));
		return RPR_SUCCESS;
	}
	catch (FrException& e)
	{
		TRACE__FUNCTION_FAILED(node); return e.GetErrorCode();
	}
}

rpr_int rprShapeGetInfo(rpr_shape shape, rpr_shape_info info, size_t size, void* data, size_t* size_ret)
{
	s_logger.printTrace("//"); //  ignore getInfo for the trace
	TRACE__FUNCTION_OPEN;
	s_logger.Trace__FunctionClose();

	API_PRINT("rprShapeGetInfo", 1, shape);
	FrNode* node = static_cast<FrNode*>(shape);

	try
	{
		CHECK_NOT_NULL(node);
		MACRO__CHECK_ARGUMENT_TYPE_ALLSHAPES(shape);


		size_t requiredSize = node->GetPropertyTypeSize(info);

		if (info == RPR_OBJECT_NAME)
		{
			std::string name = node->GetProperty<std::string>(RPR_OBJECT_NAME);
			requiredSize = name.size() + 1;
		}

		if (data)
		{
			if (requiredSize <= size)
			{
				switch (info)
				{
				case RPR_SHAPE_TYPE:
					*reinterpret_cast<rpr_shape_type*>(data) = (node->GetType() == NodeTypes::Mesh) ? RPR_SHAPE_TYPE_MESH : RPR_SHAPE_TYPE_INSTANCE;
					break;

				case RPR_SHAPE_VIDMEM_USAGE:
					throw FrException(__FILE__,__LINE__,RPR_ERROR_UNIMPLEMENTED, "Memory usage request not yet implemented",shape);

				case RPR_SHAPE_TRANSFORM:
				{
					matrix& m = node->GetProperty<matrix>(RPR_SHAPE_TRANSFORM);
					memcpy(reinterpret_cast<rpr_float*>(data), &m.m00, sizeof(rpr_float) * 16);
					break;
				}

				case RPR_SHAPE_LINEAR_MOTION:
				{
					rpr_float* dest = reinterpret_cast<rpr_float*>(data);
					rpr_float4& linearMotion = node->GetProperty<rpr_float4>(RPR_SHAPE_LINEAR_MOTION);
					dest[0] = linearMotion[0];
					dest[1] = linearMotion[1];
					dest[2] = linearMotion[2];
					dest[3] = 0;
					break;
				}

				case RPR_SHAPE_ANGULAR_MOTION:
				{
					rpr_float* dest = reinterpret_cast<rpr_float*>(data);
					rpr_float4& angMotion = node->GetProperty<rpr_float4>(RPR_SHAPE_ANGULAR_MOTION);
					dest[0] = angMotion[0];
					dest[1] = angMotion[1];
					dest[2] = angMotion[2];
					dest[3] = angMotion[3];
					break;
				}

				case RPR_SHAPE_MATERIAL:
					*reinterpret_cast<rpr_material_node*>(data) = node->GetProperty<FrNode*>(RPR_SHAPE_MATERIAL);
					break;

				case RPR_SHAPE_VISIBILITY_FLAG:
					*reinterpret_cast<rpr_bool*>(data) = node->GetProperty<rpr_bool>(RPR_SHAPE_VISIBILITY_FLAG);
					break;

				case RPR_SHAPE_VISIBILITY_PRIMARY_ONLY_FLAG:
					*reinterpret_cast<rpr_bool*>(data) = node->GetProperty<rpr_bool>(RPR_SHAPE_VISIBILITY_PRIMARY_ONLY_FLAG);
					break;

				case RPR_SHAPE_VISIBILITY_IN_SPECULAR_FLAG:
					*reinterpret_cast<rpr_bool*>(data) = node->GetProperty<rpr_bool>(RPR_SHAPE_VISIBILITY_IN_SPECULAR_FLAG);
					break;

				case RPR_SHAPE_SHADOW_FLAG:
					*reinterpret_cast<rpr_bool*>(data) = node->GetProperty<rpr_bool>(RPR_SHAPE_SHADOW_FLAG);
					break;

				case RPR_SHAPE_SHADOW_CATCHER_FLAG:
					*reinterpret_cast<rpr_bool*>(data) = node->GetProperty<rpr_bool>(RPR_SHAPE_SHADOW_CATCHER_FLAG);
					break;

				case RPR_SHAPE_SUBDIVISION_FACTOR:
					*reinterpret_cast<rpr_uint*>(data) = node->GetProperty<rpr_uint>(RPR_SHAPE_SUBDIVISION_FACTOR);
					break;

				case RPR_SHAPE_SUBDIVISION_CREASEWEIGHT:
					*reinterpret_cast<rpr_float*>(data) = node->GetProperty<rpr_float>(RPR_SHAPE_SUBDIVISION_CREASEWEIGHT);
					break;

				case RPR_SHAPE_SUBDIVISION_BOUNDARYINTEROP:
					*reinterpret_cast<rpr_uint*>(data) = node->GetProperty<rpr_uint>(RPR_SHAPE_SUBDIVISION_BOUNDARYINTEROP);
					break;

				case RPR_SHAPE_DISPLACEMENT_SCALE:
					*reinterpret_cast<rpr_float2*>(data) = node->GetProperty<rpr_float2>(RPR_SHAPE_DISPLACEMENT_SCALE);
					break;

				case RPR_SHAPE_OBJECT_GROUP_ID:
					*reinterpret_cast<rpr_uint*>(data) = node->GetProperty<rpr_uint>(RPR_SHAPE_OBJECT_GROUP_ID);
					break;

				case RPR_SHAPE_LAYER_MASK:
					*reinterpret_cast<rpr_uint*>(data) = node->GetProperty<rpr_uint>(RPR_SHAPE_LAYER_MASK);
					break;

				case RPR_SHAPE_DISPLACEMENT_MATERIAL:
				{
					FrNode* mat = node->GetProperty<FrNode*>(RPR_SHAPE_DISPLACEMENT_MATERIAL);
					*reinterpret_cast<rpr_material_node*>(data) = mat;
					break;
				}

				case RPR_OBJECT_NAME:
				{
					std::string name = node->GetProperty<std::string>(RPR_OBJECT_NAME);
					auto len = name.size();
					auto str = reinterpret_cast<rpr_char*>(data);
					std::copy(name.cbegin(), name.cend(), str);
					*(str + len) = 0;
					break;
				}

				case RPR_SHAPE_HETERO_VOLUME:
				{
					FrNode* mat = node->GetProperty<FrNode*>(RPR_SHAPE_HETERO_VOLUME);
					*reinterpret_cast<rpr_hetero_volume*>(data) = mat;
					break;
				}

				default:
				{
					throw FrException(__FILE__,__LINE__,RPR_ERROR_INVALID_PARAMETER, "",node);
					break;
				}
				}
			}
			else
			{
				throw FrException(__FILE__,__LINE__,RPR_ERROR_INVALID_PARAMETER, "",node);
			}
		}

		if (size_ret)
			*size_ret = requiredSize;
	}
	catch (FrException& e)
	{
		TRACE__FUNCTION_FAILED(node); return e.GetErrorCode();
	}

	return RPR_SUCCESS;
}

rpr_int rprHeteroVolumeGetInfo(rpr_hetero_volume heterovol, rpr_hetero_volume_parameter hvol_info, size_t size, void * data, size_t * size_ret)
{
	s_logger.printTrace("//"); //  ignore getInfo for the trace
	TRACE__FUNCTION_OPEN;
	s_logger.Trace__FunctionClose();

	API_PRINT("rprHeteroVolumeGetInfo", 1, heterovol);
	FrNode* node = static_cast<FrNode*>(heterovol);

	try
	{
		CHECK_NOT_NULL(node);
		MACRO__CHECK_ARGUMENT_TYPE(heterovol,HeteroVolume);

		size_t requiredSize = node->GetPropertyTypeSize(hvol_info);

		if (hvol_info == RPR_OBJECT_NAME)
		{
			std::string name = node->GetProperty<std::string>(RPR_OBJECT_NAME);
			requiredSize = name.size() + 1;
		}
		else if (hvol_info == RPR_HETEROVOLUME_DATA)
		{
			requiredSize = node->GetProperty<size_t>(RPR_HETEROVOLUME_DATA_SIZEBYTE);
		}
		else if (hvol_info == RPR_HETEROVOLUME_INDICES)
		{
			rpr_hetero_volume_indices_topology indicesListTopology = node->GetProperty<rpr_hetero_volume_indices_topology>(RPR_HETEROVOLUME_INDICES_TOPOLOGY);
			size_t numberOfIndices = node->GetProperty<size_t>(RPR_HETEROVOLUME_INDICES_NUMBER);

			if ( indicesListTopology == RPR_HETEROVOLUME_INDICES_TOPOLOGY_I_U64 )
			{
				requiredSize = numberOfIndices*sizeof(uint64_t);
			}
			else if ( indicesListTopology == RPR_HETEROVOLUME_INDICES_TOPOLOGY_XYZ_U32 )
			{
				requiredSize = numberOfIndices*3*sizeof(uint32_t);
			}
			else if ( indicesListTopology == RPR_HETEROVOLUME_INDICES_TOPOLOGY_I_S64 )
			{
				requiredSize = numberOfIndices*sizeof(int64_t);
			}
			else if ( indicesListTopology == RPR_HETEROVOLUME_INDICES_TOPOLOGY_XYZ_S32 )
			{
				requiredSize = numberOfIndices*3*sizeof(int32_t);
			}
			else
			{
				requiredSize = 0;
				throw FrException(__FILE__,__LINE__,RPR_ERROR_INVALID_PARAMETER, "wrong indicesListTopology",node);
			}
		}

		if (data)
		{
			if (requiredSize <= size)
			{
				switch (hvol_info)
				{
				case RPR_HETEROVOLUME_SIZE_X:
					*reinterpret_cast<size_t*>(data) = node->GetProperty<size_t>(RPR_HETEROVOLUME_SIZE_X);
					break;
				case RPR_HETEROVOLUME_SIZE_Y:
					*reinterpret_cast<size_t*>(data) = node->GetProperty<size_t>(RPR_HETEROVOLUME_SIZE_Y);
					break;
				case RPR_HETEROVOLUME_SIZE_Z:
					*reinterpret_cast<size_t*>(data) = node->GetProperty<size_t>(RPR_HETEROVOLUME_SIZE_Z);
					break;
				case RPR_HETEROVOLUME_DATA:
				{
					auto& imgData = node->GetProperty< std::shared_ptr<unsigned char> >(RPR_HETEROVOLUME_DATA);
					memcpy(data,imgData.get(),requiredSize);
					break;
				}
				case RPR_HETEROVOLUME_DATA_SIZEBYTE:
					*reinterpret_cast<size_t*>(data) = node->GetProperty<size_t>(RPR_HETEROVOLUME_DATA_SIZEBYTE);
					break;
				case RPR_HETEROVOLUME_TRANSFORM:
				{
					matrix& m = node->GetProperty<matrix>(RPR_HETEROVOLUME_TRANSFORM);
					memcpy(reinterpret_cast<rpr_float*>(data), &m.m00, sizeof(rpr_float) * 16);
					break;
				}
				case RPR_HETEROVOLUME_INDICES:
				{
					auto& imgData = node->GetProperty< std::shared_ptr<unsigned char> >(RPR_HETEROVOLUME_INDICES);
					memcpy(data,imgData.get(),requiredSize);
					break;
				}
				case RPR_HETEROVOLUME_INDICES_NUMBER:
					*reinterpret_cast<size_t*>(data) = node->GetProperty<size_t>(RPR_HETEROVOLUME_INDICES_NUMBER);
					break;
				case RPR_HETEROVOLUME_INDICES_TOPOLOGY:
					*reinterpret_cast<rpr_hetero_volume_indices_topology*>(data) = node->GetProperty<rpr_hetero_volume_indices_topology>(RPR_HETEROVOLUME_INDICES_TOPOLOGY);
					break;
				case RPR_HETEROVOLUME_EMISSION:
					*reinterpret_cast<rpr_float3*>(data) = node->GetProperty<rpr_float3>(RPR_HETEROVOLUME_EMISSION);
					break;
				case RPR_HETEROVOLUME_ALBEDO:
					*reinterpret_cast<rpr_float3*>(data) = node->GetProperty<rpr_float3>(RPR_HETEROVOLUME_ALBEDO);
					break;
				case RPR_HETEROVOLUME_FILTER:
					*reinterpret_cast<rpr_hetero_volume_filter*>(data) = node->GetProperty<rpr_hetero_volume_filter>(RPR_HETEROVOLUME_FILTER);
					break;

				case RPR_OBJECT_NAME:
				{
					std::string name = node->GetProperty<std::string>(RPR_OBJECT_NAME);
					auto len = name.size();
					auto str = reinterpret_cast<rpr_char*>(data);
					std::copy(name.cbegin(), name.cend(), str);
					*(str + len) = 0;
					break;
				}

				default:
				{
					throw FrException(__FILE__,__LINE__,RPR_ERROR_INVALID_PARAMETER, "",node);
				}
				}
			}
			else
			{
				throw FrException(__FILE__,__LINE__,RPR_ERROR_INVALID_PARAMETER, "",node);
			}
		}

		if (size_ret)
			*size_ret = requiredSize;

	}
	catch (FrException& e)
	{
		TRACE__FUNCTION_FAILED(node); return e.GetErrorCode();
	}
	catch (FireSG::property_not_found_error&)
	{
		return RPR_ERROR_INVALID_PARAMETER;
	}
	catch (FireSG::property_type_error&)
	{
		return RPR_ERROR_INVALID_PARAMETER_TYPE;
	}


	return RPR_SUCCESS;
}

rpr_int rprMeshGetInfo(rpr_shape mesh, rpr_mesh_info mesh_info, size_t size, void* data, size_t* size_ret)
{
	s_logger.printTrace("//"); //  ignore getInfo for the trace
	TRACE__FUNCTION_OPEN;
	s_logger.Trace__FunctionClose();

	API_PRINT("frMeshGetInfo", 1, mesh);
	FrNode* node = static_cast<FrNode*>(mesh);

	try
	{
		CHECK_NOT_NULL(node);
		MACRO__CHECK_ARGUMENT_TYPE_ALLSHAPES(mesh);

		size_t requiredSize = node->GetPropertyTypeSize(mesh_info);

		if (mesh_info == RPR_OBJECT_NAME)
		{
			std::string name = node->GetProperty<std::string>(RPR_OBJECT_NAME);
			requiredSize = name.size() + 1;
		}

		rpr_uint uvDim = node->GetProperty<rpr_uint>(RPR_MESH_UV_DIM);

		switch (mesh_info)
		{
		case RPR_MESH_POLYGON_COUNT:
		case RPR_MESH_VERTEX_COUNT:
		case RPR_MESH_NORMAL_COUNT:
		case RPR_MESH_UV_COUNT:
		case RPR_MESH_UV2_COUNT:
		case RPR_MESH_VERTEX_STRIDE:
		case RPR_MESH_NORMAL_STRIDE:
		case RPR_MESH_UV_STRIDE:
		case RPR_MESH_UV2_STRIDE:
		case RPR_MESH_VERTEX_INDEX_STRIDE:
		case RPR_MESH_UV_INDEX_STRIDE:
		case RPR_MESH_UV2_INDEX_STRIDE:
		case RPR_MESH_NORMAL_INDEX_STRIDE:
			requiredSize = sizeof(size_t);
			break;

		case RPR_MESH_VERTEX_ARRAY:
			requiredSize = 3 * sizeof(rpr_float) * node->GetProperty<size_t>(RPR_MESH_VERTEX_COUNT);
			break;
		case RPR_MESH_NORMAL_ARRAY:
			requiredSize = 3 * sizeof(rpr_float) * node->GetProperty<size_t>(RPR_MESH_NORMAL_COUNT);
			break;
		case RPR_MESH_UV_ARRAY:
			requiredSize = uvDim * sizeof(rpr_float) * node->GetProperty<size_t>(RPR_MESH_UV_COUNT);
			break;
		case RPR_MESH_UV2_ARRAY:
			requiredSize = uvDim * sizeof(rpr_float) * node->GetProperty<size_t>(RPR_MESH_UV2_COUNT);
			break;

		case RPR_MESH_VERTEX_INDEX_ARRAY:
		case RPR_MESH_UV_INDEX_ARRAY:
		case RPR_MESH_UV2_INDEX_ARRAY:
		case RPR_MESH_NORMAL_INDEX_ARRAY:
		{
			rpr_int* faceVerts = node->GetProperty<rpr_int*>(RPR_MESH_NUM_FACE_VERTICES_ARRAY);
			size_t numFaces = node->GetProperty<size_t>(RPR_MESH_POLYGON_COUNT);
			size_t numIds = 0;
			for (int i = 0; i < numFaces; ++i)
				numIds += faceVerts[i];
			requiredSize = sizeof(rpr_int) * numIds;
			break;
		}

		case RPR_MESH_NUM_FACE_VERTICES_ARRAY:
			requiredSize = sizeof(rpr_int) * node->GetProperty<size_t>(RPR_MESH_POLYGON_COUNT);
			break;
		}


		if (data)
		{
			if (requiredSize <= size)
			{
				switch (mesh_info)
				{
				case RPR_MESH_POLYGON_COUNT:
				case RPR_MESH_VERTEX_COUNT:
				case RPR_MESH_NORMAL_COUNT:
				case RPR_MESH_UV_COUNT:
				case RPR_MESH_UV2_COUNT:
				case RPR_MESH_VERTEX_STRIDE:
				case RPR_MESH_NORMAL_STRIDE:
				case RPR_MESH_UV_STRIDE:
				case RPR_MESH_UV2_STRIDE:
				case RPR_MESH_VERTEX_INDEX_STRIDE:
				case RPR_MESH_UV_INDEX_STRIDE:
				case RPR_MESH_UV2_INDEX_STRIDE:
				case RPR_MESH_NORMAL_INDEX_STRIDE:
					*reinterpret_cast<size_t*>(data) = node->GetProperty<size_t>(mesh_info);
					break;

				case RPR_MESH_VERTEX_ARRAY:
				{
					auto& src = node->GetProperty<rpr_float*>(RPR_MESH_VERTEX_ARRAY);
					auto& cnt = node->GetProperty<size_t>(RPR_MESH_VERTEX_COUNT);
					auto& stride = node->GetProperty<size_t>(RPR_MESH_VERTEX_STRIDE);

					char* tmp = reinterpret_cast<char*>(src);
					float* dest = reinterpret_cast<float*>(data);
					for (int i = 0; i < cnt; ++i)
					{
						float* val = reinterpret_cast<float*>(tmp);
						*dest++ = *val;
						*dest++ = *(val + 1);
						*dest++ = *(val + 2);
						tmp += stride;
					}
					break;
				}
				case RPR_MESH_NORMAL_ARRAY:
				{
					auto& src = node->GetProperty<rpr_float*>(RPR_MESH_NORMAL_ARRAY);
					auto& cnt = node->GetProperty<size_t>(RPR_MESH_NORMAL_COUNT);
					auto& stride = node->GetProperty<size_t>(RPR_MESH_NORMAL_STRIDE);

					char* tmp = reinterpret_cast<char*>(src);
					float* dest = reinterpret_cast<float*>(data);
					for (int i = 0; i < cnt; ++i)
					{
						float* val = reinterpret_cast<float*>(tmp);
						*dest++ = *val;
						*dest++ = *(val + 1);
						*dest++ = *(val + 2);
						tmp += stride;
					}
					break;
				}
				case RPR_MESH_UV_ARRAY:
				{
					auto& src = node->GetProperty<rpr_float*>(RPR_MESH_UV_ARRAY);
					auto& cnt = node->GetProperty<size_t>(RPR_MESH_UV_COUNT);
					auto& stride = node->GetProperty<size_t>(RPR_MESH_UV_STRIDE);

					char* tmp = reinterpret_cast<char*>(src);
					float* dest = reinterpret_cast<float*>(data);
					for (int i = 0; i < cnt; ++i)
					{
						float* val = reinterpret_cast<float*>(tmp);
						
						for(int i=0; i<(int)uvDim; i++)
						{
							*dest++ = *(val + i);
						}
						
						tmp += stride;
					}
					break;
				}

				case RPR_MESH_UV2_ARRAY:
				{
					auto& src = node->GetProperty<rpr_float*>(RPR_MESH_UV2_ARRAY);
					auto& cnt = node->GetProperty<size_t>(RPR_MESH_UV2_COUNT);
					auto& stride = node->GetProperty<size_t>(RPR_MESH_UV2_STRIDE);

					char* tmp = reinterpret_cast<char*>(src);
					float* dest = reinterpret_cast<float*>(data);
					for (int i = 0; i < cnt; ++i)
					{
						float* val = reinterpret_cast<float*>(tmp);
						
						for(int i=0; i<(int)uvDim; i++)
						{
							*dest++ = *(val + i);
						}

						tmp += stride;
					}
					break;
				}

				case RPR_MESH_VERTEX_INDEX_ARRAY:
				{
					rpr_int* faceVerts = node->GetProperty<rpr_int*>(RPR_MESH_NUM_FACE_VERTICES_ARRAY);
					size_t numFaces = node->GetProperty<size_t>(RPR_MESH_POLYGON_COUNT);
					size_t numIds = 0;
					for (int i = 0; i < numFaces; ++i)
						numIds += faceVerts[i];

					auto& indices = node->GetProperty<rpr_int*>(RPR_MESH_VERTEX_INDEX_ARRAY);
					auto& stride = node->GetProperty<size_t>(RPR_MESH_VERTEX_INDEX_STRIDE);

					char* tmp = reinterpret_cast<char*>(indices);
					rpr_int* dest = reinterpret_cast<rpr_int*>(data);
					for (int i = 0; i < numIds; ++i)
					{
						rpr_int* val = reinterpret_cast<rpr_int*>(tmp);
						*dest++ = *val;
						tmp += stride;
					}
					break;
				}
				case RPR_MESH_UV_INDEX_ARRAY:
				{
					rpr_int* faceVerts = node->GetProperty<rpr_int*>(RPR_MESH_NUM_FACE_VERTICES_ARRAY);
					size_t numFaces = node->GetProperty<size_t>(RPR_MESH_POLYGON_COUNT);
					size_t numIds = 0;
					for (int i = 0; i < numFaces; ++i)
						numIds += faceVerts[i];

					auto& indices = node->GetProperty<rpr_int*>(RPR_MESH_UV_INDEX_ARRAY);
					auto& stride = node->GetProperty<size_t>(RPR_MESH_UV_INDEX_STRIDE);

					char* tmp = reinterpret_cast<char*>(indices);
					rpr_int* dest = reinterpret_cast<rpr_int*>(data);
					for (int i = 0; i < numIds; ++i)
					{
						rpr_int* val = reinterpret_cast<rpr_int*>(tmp);
						*dest++ = *val;
						tmp += stride;
					}
					break;
				}

				case RPR_MESH_UV2_INDEX_ARRAY:
				{
					rpr_int* faceVerts = node->GetProperty<rpr_int*>(RPR_MESH_NUM_FACE_VERTICES_ARRAY);
					size_t numFaces = node->GetProperty<size_t>(RPR_MESH_POLYGON_COUNT);
					size_t numIds = 0;
					for (int i = 0; i < numFaces; ++i)
						numIds += faceVerts[i];

					auto& indices = node->GetProperty<rpr_int*>(RPR_MESH_UV2_INDEX_ARRAY);
					auto& stride = node->GetProperty<size_t>(RPR_MESH_UV2_INDEX_STRIDE);

					char* tmp = reinterpret_cast<char*>(indices);
					rpr_int* dest = reinterpret_cast<rpr_int*>(data);
					for (int i = 0; i < numIds; ++i)
					{
						rpr_int* val = reinterpret_cast<rpr_int*>(tmp);
						*dest++ = *val;
						tmp += stride;
					}
					break;
				}

				case RPR_MESH_NORMAL_INDEX_ARRAY:
				{
					rpr_int* faceVerts = node->GetProperty<rpr_int*>(RPR_MESH_NUM_FACE_VERTICES_ARRAY);
					size_t numFaces = node->GetProperty<size_t>(RPR_MESH_POLYGON_COUNT);
					size_t numIds = 0;
					for (int i = 0; i < numFaces; ++i)
						numIds += faceVerts[i];

					auto& indices = node->GetProperty<rpr_int*>(RPR_MESH_NORMAL_INDEX_ARRAY);
					auto& stride = node->GetProperty<size_t>(RPR_MESH_NORMAL_INDEX_STRIDE);

					char* tmp = reinterpret_cast<char*>(indices);
					rpr_int* dest = reinterpret_cast<rpr_int*>(data);
					for (int i = 0; i < numIds; ++i)
					{
						rpr_int* val = reinterpret_cast<rpr_int*>(tmp);
						*dest++ = *val;
						tmp += stride;
					}
					break;
				}
				case RPR_MESH_NUM_FACE_VERTICES_ARRAY:
				{
					auto& numVerticesArray = node->GetProperty<rpr_int*>(RPR_MESH_NUM_FACE_VERTICES_ARRAY);
					size_t polygonCount = node->GetProperty<size_t>(RPR_MESH_POLYGON_COUNT);
					int* dest = reinterpret_cast<int*>(data);
					for(size_t i=0; i<polygonCount; i++)
					{
						*dest++ = numVerticesArray[i];
					}
					break;
				}

				case RPR_MESH_UV_DIM:
				{
					*reinterpret_cast<rpr_uint*>(data) = node->GetProperty<rpr_uint>(mesh_info);
					break;
				}

				default:
				{
					rpr_int retCode = rprShapeGetInfo(mesh, mesh_info, size, data, size_ret);
					if ( retCode != RPR_SUCCESS )
						throw FrException(__FILE__,__LINE__,retCode, "",node);
					else
						return retCode;
				}
				}
			}
			else
			{
				throw FrException(__FILE__,__LINE__,RPR_ERROR_INVALID_PARAMETER, "",node);
			}
		}

		if (size_ret)
			*size_ret = requiredSize;
	}
	catch (FrException& e)
	{
		TRACE__FUNCTION_FAILED(node); return e.GetErrorCode();
	}
	catch (FireSG::property_not_found_error&)
	{
		return RPR_ERROR_INVALID_PARAMETER;
	}
	catch (FireSG::property_type_error&)
	{
		return RPR_ERROR_INVALID_PARAMETER_TYPE;
	}


	return RPR_SUCCESS;
}

rpr_int rprMeshPolygonGetInfo(rpr_shape mesh, size_t polygon_index, rpr_mesh_polygon_info polygon_info, size_t size, void* data, size_t* size_ret)
{
	s_logger.printTrace("//"); //  ignore getInfo for the trace
	TRACE__FUNCTION_OPEN;
	s_logger.Trace__FunctionClose();

	API_PRINT("frMeshPolygonGetInfo", 1, mesh);
	FrNode* node = static_cast<FrNode*>(mesh);

	try
	{
		CHECK_NOT_NULL(node);
		MACRO__CHECK_ARGUMENT_TYPE_ALLSHAPES(mesh);



		switch (polygon_info)
		{
		case RPR_MESH_POLYGON_VERTEX_COUNT:
		{
			size_t requiredSize = sizeof(rpr_int);
			if (requiredSize <= size)
			{
				rpr_int* numVerticesArray = node->GetProperty<rpr_int*>(RPR_MESH_NUM_FACE_VERTICES_ARRAY);
				*reinterpret_cast<rpr_int*>(data) = numVerticesArray[polygon_index];

				if (size_ret)
					*size_ret = requiredSize;
			}
			else
			{
				throw FrException(__FILE__,__LINE__,RPR_ERROR_INVALID_PARAMETER, "",node);
			}
			break;
		}

		default:
			throw FrException(__FILE__,__LINE__,RPR_ERROR_INVALID_PARAMETER, "Invalid mesh polygon info requested",mesh);
		}
	}
	catch (FrException& e)
	{
		TRACE__FUNCTION_FAILED(node); return e.GetErrorCode();
	}
	catch (FireSG::property_not_found_error&)
	{
		return RPR_ERROR_INVALID_PARAMETER;
	}
	catch (FireSG::property_type_error&)
	{
		return RPR_ERROR_INVALID_PARAMETER_TYPE;
	}

	return RPR_SUCCESS;
}

/*
rpr_int rprMeshPolygonVertexGetInfo(rpr_shape mesh, size_t polygon_index, size_t vertex_index, rpr_mesh_polygon_vertex_info polygon_vertex_info, size_t size, void* data, size_t* size_ret)
{
	s_logger.printTrace("//"); //  ignore getInfo for the trace
	TRACE__FUNCTION_OPEN;
	s_logger.Trace__FunctionClose();

	API_PRINT("frMeshPolygonVertexGetInfo", 1, mesh);
	FrNode* node = static_cast<FrNode*>(mesh);
	
	try
	{
		CHECK_NOT_NULL(node);

		size_t requiredSize = 0;
		switch (polygon_vertex_info)
		{
		case RPR_MESH_POLYGON_VERTEX_POSITION:
		case RPR_MESH_POLYGON_VERTEX_NORMAL:
		case RPR_MESH_POLYGON_VERTEX_TEXCOORD:
			requiredSize = sizeof(rpr_float) * 3;
			break;

		default:
			throw FrException(__FILE__,__LINE__,RPR_ERROR_INVALID_PARAMETER, "Invalid mesh polygon vrtex info requested");
		}

		if (requiredSize <= size)
		{
			switch (polygon_vertex_info)
			{
			case RPR_MESH_POLYGON_VERTEX_POSITION:
			{
				auto& numVerticesArray = node->GetProperty<std::vector<rpr_int>>(RPR_MESH_NUM_VERTICES_ARRAY);
				auto& attribIndexArray = node->GetProperty<std::vector<rpr_int>>(RPR_MESH_VERTEX_INDEX_ARRAY);
				auto& positions = node->GetProperty<std::vector<rpr_float3>>(RPR_MESH_VERTEX_ARRAY);
				rpr_float* dest = reinterpret_cast<rpr_float*>(data);
				*dest++ = positions[vertex_index];
				break;
			}

			case RPR_MESH_POLYGON_VERTEX_NORMAL:
			{
				break;
			}

			case RPR_MESH_POLYGON_VERTEX_TEXCOORD:
			{
				break;
			}
			}
		}
		else
		{
			throw FrException(__FILE__,__LINE__,RPR_ERROR_INVALID_PARAMETER, "",node);
		}
	}
	catch(FrException& e)
	{
		TRACE__FUNCTION_FAILED(context); return e.GetErrorCode();
	}

	return RPR_SUCCESS;
}
*/

/*
rpr_int rprMeshPolygonVertexSetAttribute3f(rpr_shape mesh, size_t polygon_index, size_t vertex_index, rpr_mesh_polygon_vertex_info polygon_vertex_info, rpr_float x, rpr_float y, rpr_float z)
{
	s_logger.printTrace("status = ");
	TRACE__FUNCTION_OPEN;
	s_logger.TraceArg__rpr_shape(mesh); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__size_t(polygon_index); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__size_t(vertex_index); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_mesh_polygon_vertex_info(polygon_vertex_info); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_float(x); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_float(y); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_float(z);
	s_logger.Trace__FunctionClose();

	API_PRINT("frMeshPolygonVertexSetAttribute3f", 1, mesh);
	FrNode* node = static_cast<FrNode*>(mesh);
	
	try
	{
	CHECK_NOT_NULL(node);
	auto& polygons = node->GetProperty<std::vector<polygon>>("Polygons");

	switch (polygon_vertex_info)
	{
	case RPR_MESH_POLYGON_VERTEX_POSITION:
	{
	auto& positions = node->GetProperty<std::vector<rpr_float3>>("Positions");
	positions[polygons[polygon_index].p[vertex_index]] = { x, y, z };
	break;
	}

	case RPR_MESH_POLYGON_VERTEX_NORMAL:
	{
	auto& normals = node->GetProperty<std::vector<rpr_float3>>("Normals");
	normals[polygons[polygon_index].p[vertex_index]] = { x, y, z };
	break;
	}

	case RPR_MESH_POLYGON_VERTEX_TEXCOORD:
	{
	auto& texCoords = node->GetProperty<std::vector<rpr_float3>>("TexCoords");
	texCoords[polygons[polygon_index].p[vertex_index]] = { x, y, z };
	break;
	}
	}
	}
	catch(FrException& e)
	{
	TRACE__FUNCTION_FAILED(context); return e.GetErrorCode();
	}

	return RPR_SUCCESS;
}
*/


rpr_int rprInstanceGetBaseShape(rpr_shape shape, rpr_shape* out_shape)
{
	s_logger.printTrace("//"); //  ignore getInfo for the trace
	TRACE__FUNCTION_OPEN;
	s_logger.Trace__FunctionClose();

	API_PRINT("frInstanceGetBaseShape", 1, shape);
	FrNode* node = static_cast<FrNode*>(shape);

	try
	{
		CHECK_NOT_NULL(node);

		MACRO__CHECK_ARGUMENT_TYPE_ALLSHAPES(shape);


		*out_shape = static_cast<rpr_shape>(node->GetProperty<FrNode*>(RPR_INSTANCE_PARENT_SHAPE));
		return RPR_SUCCESS;
	}
	catch (FrException& e)
	{
		TRACE__FUNCTION_FAILED(node); return e.GetErrorCode();
	}
}


rpr_int rprLightGetInfo(rpr_light light, rpr_light_info info, size_t size, void* data, size_t* size_ret)
{
	s_logger.printTrace("//"); //  ignore getInfo for the trace
	TRACE__FUNCTION_OPEN;
	s_logger.Trace__FunctionClose();

	API_PRINT("rprLightGetInfo", 1, light);
	FrNode* node = static_cast<FrNode*>(light);

	try
	{
		CHECK_NOT_NULL(node);

		MACRO__CHECK_ARGUMENT_TYPE_ALLLIGHTS(light);



		size_t requiredSize = 0;

		if ( info == RPR_ENVIRONMENT_LIGHT_PORTAL_COUNT || info == RPR_SKY_LIGHT_PORTAL_COUNT )
		{
			requiredSize = sizeof(size_t);
		}
		else if ( info == RPR_ENVIRONMENT_LIGHT_PORTAL_LIST || info == RPR_SKY_LIGHT_PORTAL_LIST )
		{
			requiredSize = sizeof(rpr_shape) * node->GetProperty<FrShapeList>(info).size();
		}
		else
		{
			requiredSize = node->GetPropertyTypeSize(info);
		}


		if (info == RPR_OBJECT_NAME)
		{
			std::string name = node->GetProperty<std::string>(RPR_OBJECT_NAME);
			requiredSize = name.size() + 1;
		}

		if (data)
		{
			if (requiredSize <= size)
			{
				switch (info)
				{
				case RPR_LIGHT_TYPE:
				{
					NodeTypes nodeType = node->GetType();
					switch (nodeType)
					{
					case NodeTypes::PointLight:
						*reinterpret_cast<rpr_light_type*>(data) = RPR_LIGHT_TYPE_POINT;
						return RPR_SUCCESS;

					case NodeTypes::DirectionalLight:
						*reinterpret_cast<rpr_light_type*>(data) = RPR_LIGHT_TYPE_DIRECTIONAL;
						return RPR_SUCCESS;

					case NodeTypes::SpotLight:
						*reinterpret_cast<rpr_light_type*>(data) = RPR_LIGHT_TYPE_SPOT;
						return RPR_SUCCESS;

					case NodeTypes::EnvironmentLight:
						*reinterpret_cast<rpr_light_type*>(data) = RPR_LIGHT_TYPE_ENVIRONMENT;
						return RPR_SUCCESS;

					case NodeTypes::SkyLight:
						*reinterpret_cast<rpr_light_type*>(data) = RPR_LIGHT_TYPE_SKY;
						return RPR_SUCCESS;

					case NodeTypes::IESLight:
						*reinterpret_cast<rpr_light_type*>(data) = RPR_LIGHT_TYPE_IES;
						return RPR_SUCCESS;

					default:
						throw FrException(__FILE__,__LINE__,RPR_ERROR_INVALID_PARAMETER, "Invalid light type",light);
					}
				}

				case RPR_LIGHT_TRANSFORM:
				{
					matrix& m = node->GetProperty<matrix>(RPR_LIGHT_TRANSFORM);
					memcpy(reinterpret_cast<rpr_float*>(data), &m.m00, sizeof(rpr_float) * 16);
					return RPR_SUCCESS;
				}

				case RPR_LIGHT_GROUP_ID:
				{
					rpr_uint m = node->GetProperty<rpr_uint>(RPR_LIGHT_GROUP_ID);
					memcpy(reinterpret_cast<rpr_uint*>(data), &m, sizeof(rpr_uint));
					return RPR_SUCCESS;
				}
				
				case RPR_OBJECT_NAME:
				{
					std::string name = node->GetProperty<std::string>(RPR_OBJECT_NAME);
					auto len = name.size();
					auto str = reinterpret_cast<rpr_char*>(data);
					std::copy(name.cbegin(), name.cend(), str);
					*(str + len) = 0;
					break;
				}

				case RPR_POINT_LIGHT_RADIANT_POWER:
				{
					float3& v = node->GetProperty<float3>(RPR_POINT_LIGHT_RADIANT_POWER);
					memcpy(reinterpret_cast<rpr_float*>(data), &v.x, sizeof(float3));
					return RPR_SUCCESS;
				}

				case RPR_SPOT_LIGHT_RADIANT_POWER:
				{
					float3& v = node->GetProperty<float3>(RPR_SPOT_LIGHT_RADIANT_POWER);
					memcpy(reinterpret_cast<rpr_float*>(data), &v.x, sizeof(float3));
					return RPR_SUCCESS;
				}

				case RPR_SKY_LIGHT_DIRECTION:
				{
					float3& v = node->GetProperty<float3>(RPR_SKY_LIGHT_DIRECTION);
					memcpy(reinterpret_cast<rpr_float*>(data), &v.x, sizeof(float3));
					return RPR_SUCCESS;
				}

				case RPR_DIRECTIONAL_LIGHT_RADIANT_POWER:
				{
					float3& v = node->GetProperty<float3>(RPR_DIRECTIONAL_LIGHT_RADIANT_POWER);
					memcpy(reinterpret_cast<rpr_float*>(data), &v.x, sizeof(float3));
					return RPR_SUCCESS;
				}

				case RPR_DIRECTIONAL_LIGHT_SHADOW_SOFTNESS:
				{
					rpr_float& v = node->GetProperty<rpr_float>(RPR_DIRECTIONAL_LIGHT_SHADOW_SOFTNESS);
					memcpy(reinterpret_cast<rpr_float*>(data), &v, sizeof(rpr_float));
					return RPR_SUCCESS;
				}

				case RPR_SPOT_LIGHT_CONE_SHAPE:
				{
					float2& v = node->GetProperty<float2>(RPR_SPOT_LIGHT_CONE_SHAPE);
					memcpy(reinterpret_cast<rpr_float*>(data), &v.x, sizeof(float2));
					return RPR_SUCCESS;
				}

				case RPR_ENVIRONMENT_LIGHT_IMAGE:
				{
					FrNode* img = node->GetProperty<FrNode*>(RPR_ENVIRONMENT_LIGHT_IMAGE);
					*reinterpret_cast<rpr_image*>(data) = img;
					return RPR_SUCCESS;
				}

				case RPR_ENVIRONMENT_LIGHT_INTENSITY_SCALE:
				{
					float& f = node->GetProperty<float>(RPR_ENVIRONMENT_LIGHT_INTENSITY_SCALE);
					*reinterpret_cast<rpr_float*>(data) = f;
					return RPR_SUCCESS;
				}

				case RPR_SKY_LIGHT_SCALE:
				{
					float& f = node->GetProperty<float>(RPR_SKY_LIGHT_SCALE);
					*reinterpret_cast<rpr_float*>(data) = f;
					return RPR_SUCCESS;
				}

				case RPR_SKY_LIGHT_TURBIDITY:
				{
					float& f = node->GetProperty<float>(RPR_SKY_LIGHT_TURBIDITY);
					*reinterpret_cast<rpr_float*>(data) = f;
					return RPR_SUCCESS;
				}

				case RPR_SKY_LIGHT_ALBEDO:
				{
					float& f = node->GetProperty<float>(RPR_SKY_LIGHT_ALBEDO);
					*reinterpret_cast<rpr_float*>(data) = f;
					return RPR_SUCCESS;
				}

				case RPR_IES_LIGHT_IMAGE_DESC:
				{
					rpr_ies_image_desc& desc = node->GetProperty<rpr_ies_image_desc>(RPR_IES_LIGHT_IMAGE_DESC);
					*reinterpret_cast<rpr_ies_image_desc*>(data) = desc;
					return RPR_SUCCESS;
				}

				case RPR_IES_LIGHT_RADIANT_POWER:
				{
					float3& v = node->GetProperty<float3>(RPR_IES_LIGHT_RADIANT_POWER);
					memcpy(reinterpret_cast<rpr_float*>(data), &v.x, sizeof(float3));
					return RPR_SUCCESS;
				}

				case RPR_ENVIRONMENT_LIGHT_PORTAL_COUNT:
				{
					*reinterpret_cast<size_t*>(data) = node->GetProperty<FrShapeList>(RPR_ENVIRONMENT_LIGHT_PORTAL_LIST).size();
					return RPR_SUCCESS;
				}

				case RPR_SKY_LIGHT_PORTAL_COUNT:
				{
					*reinterpret_cast<size_t*>(data) = node->GetProperty<FrShapeList>(RPR_SKY_LIGHT_PORTAL_LIST).size();
					return RPR_SUCCESS;
				}

				case RPR_ENVIRONMENT_LIGHT_PORTAL_LIST:
				case RPR_SKY_LIGHT_PORTAL_LIST:
				{
					auto& nodeList = node->GetProperty<FrShapeList>(info);
					FrNode** dest = reinterpret_cast<FrNode**>(data);
					for (auto& n : nodeList)
					{
						*dest++ = n;
					}

					return RPR_SUCCESS;
				}

				default:
					throw FrException(__FILE__,__LINE__,RPR_ERROR_INVALID_PARAMETER, "Invalid light info requested",light);
				}
			}
			else
			{
				throw FrException(__FILE__,__LINE__,RPR_ERROR_INVALID_PARAMETER, "Invalid light info size requested",light);
			}
		}

		if (size_ret)
			*size_ret = requiredSize;
	}
	catch (FrException& e)
	{
		TRACE__FUNCTION_FAILED(node); return e.GetErrorCode();
	}

	return RPR_SUCCESS;

}

rpr_int rprContextCreatePointLight(rpr_context context, rpr_light* out_light)
{
	//in case of issues during creation, it's safer to return a null pointer, rather than let the pointer undefined.
	if ( out_light )
		*out_light = nullptr;
	


	s_logger.printTrace("//PointLight creation\r\n");
	s_logger.Trace__FlushAllFiles(); // in case a crash, ensure trace informs the creation

	API_PRINT("rprContextCreatePointLight", 1, context);
	FrNode* ctx = static_cast<FrNode*>(context);

	try
	{
		CHECK_NOT_NULL(ctx);
		MACRO__CHECK_ARGUMENT_TYPE(context,Context);


		std::shared_ptr<FrSceneGraph> sceneGraph = ctx->GetProperty< std::shared_ptr<FrSceneGraph> >(FR_SCENEGRAPH);
		std::shared_ptr<FrRendererEncalps> renderer = ctx->GetProperty< std::shared_ptr<FrRendererEncalps> >(RPR_CONTEXT_ACTIVE_PLUGIN);
		FrNode* node = sceneGraph->CreateNode(NodeTypes::PointLight, [&](FrNode* n) {
			n->SetProperty(FR_NODE_CONTEXT, ctx);
            n->AddProperty(FR_SCENEGRAPH, sceneGraph);
			n->AddProperty(RPR_CONTEXT_ACTIVE_PLUGIN, renderer);
		});

		*out_light = node;

		s_logger.Trace__NewFrObjectCreated(Logger::TRACE_ADDRESS_TYPE__LIGHT, *out_light);
		s_logger.Trace__DeclareFRobject("rpr_light","light_0x", *out_light);
		s_logger.printTrace("status = ");
		TRACE__FUNCTION_OPEN
		s_logger.TraceArg__rpr_context(context); s_logger.TraceArg__COMMA();
		s_logger.printTrace("&light_0x%p", *out_light);
		s_logger.Trace__FunctionClose();
		s_logger.printTrace("status = rprObjectSetName (light_0x%p,\"light_0x%p\"); RPRTRACE_CHECK// added by tracing for debug\r\n",  *out_light ,  *out_light);

		return RPR_SUCCESS;
	}
	catch (FrException& e)
	{
		TRACE__FUNCTION_FAILED(context); return e.GetErrorCode();
	}
}

rpr_int rprContextCreateDirectionalLight(rpr_context context, rpr_light* out_light)
{
	//in case of issues during creation, it's safer to return a null pointer, rather than let the pointer undefined.
	if ( out_light )
		*out_light = nullptr;
	

	s_logger.printTrace("//DirectionalLight creation\r\n");
	s_logger.Trace__FlushAllFiles(); // in case a crash, ensure trace informs the creation

	API_PRINT("rprContextCreateDirectionalLight", 1, context);
	FrNode* ctx = static_cast<FrNode*>(context);

	try
	{
		CHECK_NOT_NULL(ctx);
		MACRO__CHECK_ARGUMENT_TYPE(context,Context);


		std::shared_ptr<FrSceneGraph> sceneGraph = ctx->GetProperty< std::shared_ptr<FrSceneGraph> >(FR_SCENEGRAPH);
		std::shared_ptr<FrRendererEncalps> renderer = ctx->GetProperty< std::shared_ptr<FrRendererEncalps> >(RPR_CONTEXT_ACTIVE_PLUGIN);
		FrNode* node = sceneGraph->CreateNode(NodeTypes::DirectionalLight, [&](FrNode* n) {
			n->SetProperty(FR_NODE_CONTEXT, ctx);
            n->AddProperty(FR_SCENEGRAPH, sceneGraph);
			n->AddProperty(RPR_CONTEXT_ACTIVE_PLUGIN, renderer);
		});

		*out_light = node;

		s_logger.Trace__NewFrObjectCreated(Logger::TRACE_ADDRESS_TYPE__LIGHT, *out_light);
		s_logger.Trace__DeclareFRobject("rpr_light", "light_0x", *out_light);
		s_logger.printTrace("status = ");
		TRACE__FUNCTION_OPEN
		s_logger.TraceArg__rpr_context(context); s_logger.TraceArg__COMMA();
		s_logger.printTrace("&light_0x%p", *out_light);
		s_logger.Trace__FunctionClose();
		s_logger.printTrace("status = rprObjectSetName (light_0x%p,\"light_0x%p\"); RPRTRACE_CHECK// added by tracing for debug\r\n",  *out_light ,  *out_light);

		return RPR_SUCCESS;
	}
	catch (FrException& e)
	{
		TRACE__FUNCTION_FAILED(context); return e.GetErrorCode();
	}
}

rpr_int rprDirectionalLightSetShadowSoftness(rpr_light light, rpr_float coeff)
{
	s_logger.printTrace("status = ");
	TRACE__FUNCTION_OPEN;
	s_logger.TraceArg__rpr_light(light); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_float(coeff); 
	s_logger.Trace__FunctionClose();

	API_PRINT("rprDirectionalLightSetShadowSoftness", 1, light);
	FrNode* node = static_cast<FrNode*>(light);

	try
	{
		CHECK_NOT_NULL(node);
		MACRO__CHECK_ARGUMENT_TYPE(light,DirectionalLight);


		node->SetProperty(RPR_DIRECTIONAL_LIGHT_SHADOW_SOFTNESS, coeff);
		return RPR_SUCCESS;
	}
	catch (FrException& e)
	{
		TRACE__FUNCTION_FAILED(node); return e.GetErrorCode();
	}
}

rpr_int rprDirectionalLightSetRadiantPower3f(rpr_light light, rpr_float r, rpr_float g, rpr_float b)
{
	s_logger.printTrace("status = ");
	TRACE__FUNCTION_OPEN;
	s_logger.TraceArg__rpr_light(light); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_float(r); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_float(g); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_float(b);
	s_logger.Trace__FunctionClose();

	API_PRINT("rprDirectionalLightSetRadiantPower3f", 1, light);
	FrNode* node = static_cast<FrNode*>(light);

	try
	{
		CHECK_NOT_NULL(node);
		MACRO__CHECK_ARGUMENT_TYPE(light,DirectionalLight);

		MACRO__CHECK_ARGUMENT_FLOAT_NAN(r,light);
		MACRO__CHECK_ARGUMENT_FLOAT_NAN(g,light);
		MACRO__CHECK_ARGUMENT_FLOAT_NAN(b,light);

		node->SetProperty(RPR_DIRECTIONAL_LIGHT_RADIANT_POWER, rpr_float3{ r, g, b });
		return RPR_SUCCESS;
	}
	catch (FrException& e)
	{
		TRACE__FUNCTION_FAILED(node); return e.GetErrorCode();
	}
}

rpr_int rprContextCreateSpotLight(rpr_context context, rpr_light* out_light)
{
	//in case of issues during creation, it's safer to return a null pointer, rather than let the pointer undefined.
	if ( out_light )
		*out_light = nullptr;
	

	s_logger.printTrace("//SpotLight creation\r\n");
	s_logger.Trace__FlushAllFiles(); // in case a crash, ensure trace informs the creation

	API_PRINT("rprContextCreateSpotLight", 1, context);
	FrNode* ctx = static_cast<FrNode*>(context);

	try
	{
		CHECK_NOT_NULL(ctx);
		MACRO__CHECK_ARGUMENT_TYPE(context,Context);


		std::shared_ptr<FrSceneGraph> sceneGraph = ctx->GetProperty< std::shared_ptr<FrSceneGraph> >(FR_SCENEGRAPH);
		std::shared_ptr<FrRendererEncalps> renderer = ctx->GetProperty< std::shared_ptr<FrRendererEncalps> >(RPR_CONTEXT_ACTIVE_PLUGIN);
		FrNode* node = sceneGraph->CreateNode(NodeTypes::SpotLight, [&](FrNode* n) {
			n->SetProperty(FR_NODE_CONTEXT, ctx);
            n->AddProperty(FR_SCENEGRAPH, sceneGraph);
			n->AddProperty(RPR_CONTEXT_ACTIVE_PLUGIN, renderer);
		});

		*out_light = node;

		s_logger.Trace__NewFrObjectCreated(Logger::TRACE_ADDRESS_TYPE__LIGHT, *out_light);
		s_logger.Trace__DeclareFRobject("rpr_light", "light_0x", *out_light);
		s_logger.printTrace("status = ");
		TRACE__FUNCTION_OPEN
		s_logger.TraceArg__rpr_context(context); s_logger.TraceArg__COMMA();
		s_logger.printTrace("&light_0x%p", *out_light);
		s_logger.Trace__FunctionClose();
		s_logger.printTrace("status = rprObjectSetName (light_0x%p,\"light_0x%p\"); RPRTRACE_CHECK// added by tracing for debug\r\n",  *out_light ,  *out_light);

		return RPR_SUCCESS;
	}
	catch (FrException& e)
	{
		TRACE__FUNCTION_FAILED(context); return e.GetErrorCode();
	}
}

rpr_int rprSpotLightSetRadiantPower3f(rpr_light light, rpr_float r, rpr_float g, rpr_float b)
{
	s_logger.printTrace("status = ");
	TRACE__FUNCTION_OPEN;
	s_logger.TraceArg__rpr_light(light); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_float(r); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_float(g); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_float(b);
	s_logger.Trace__FunctionClose();

	API_PRINT("rprSpotLightSetRadiantPower3f", 1, light);
	FrNode* node = static_cast<FrNode*>(light);

	try
	{
		CHECK_NOT_NULL(node);
		MACRO__CHECK_ARGUMENT_TYPE(light,SpotLight);

		MACRO__CHECK_ARGUMENT_FLOAT_NAN(r,light);
		MACRO__CHECK_ARGUMENT_FLOAT_NAN(g,light);
		MACRO__CHECK_ARGUMENT_FLOAT_NAN(b,light);

		node->SetProperty(RPR_SPOT_LIGHT_RADIANT_POWER, rpr_float3{r,g,b});
		return RPR_SUCCESS;
	}
	catch (FrException& e)
	{
		TRACE__FUNCTION_FAILED(node); return e.GetErrorCode();
	}
}

rpr_int rprPointLightSetRadiantPower3f(rpr_light light, rpr_float r, rpr_float g, rpr_float b)
{
	s_logger.printTrace("status = ");
	TRACE__FUNCTION_OPEN;
	s_logger.TraceArg__rpr_light(light); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_float(r); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_float(g); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_float(b);
	s_logger.Trace__FunctionClose();

	API_PRINT("rprSpotLightSetRadiantPower3f", 1, light);
	FrNode* node = static_cast<FrNode*>(light);

	try
	{
		CHECK_NOT_NULL(node);
		MACRO__CHECK_ARGUMENT_TYPE(light,PointLight);

		MACRO__CHECK_ARGUMENT_FLOAT_NAN(r,light);
		MACRO__CHECK_ARGUMENT_FLOAT_NAN(g,light);
		MACRO__CHECK_ARGUMENT_FLOAT_NAN(b,light);

		node->SetProperty(RPR_POINT_LIGHT_RADIANT_POWER, rpr_float3{ r, g, b });
		return RPR_SUCCESS;
	}
	catch (FrException& e)
	{
		TRACE__FUNCTION_FAILED(node); return e.GetErrorCode();
	}
}

rpr_int rprSpotLightSetConeShape(rpr_light light, rpr_float iangle, rpr_float oangle)
{
	s_logger.printTrace("status = ");
	TRACE__FUNCTION_OPEN;
	s_logger.TraceArg__rpr_light(light); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_float(iangle); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_float(oangle);
	s_logger.Trace__FunctionClose();

	API_PRINT("rprSpotLightSetConeShape", 1, light);
	FrNode* node = static_cast<FrNode*>(light);

	try
	{
		CHECK_NOT_NULL(node);
		MACRO__CHECK_ARGUMENT_TYPE(light,SpotLight);


		node->SetProperty(RPR_SPOT_LIGHT_CONE_SHAPE, rpr_float2(iangle, oangle));
		return RPR_SUCCESS;
	}
	catch (FrException& e)
	{
		TRACE__FUNCTION_FAILED(node); return e.GetErrorCode();
	}
}

rpr_int rprContextCreateEnvironmentLight(rpr_context context, rpr_light* out_light)
{
	//in case of issues during creation, it's safer to return a null pointer, rather than let the pointer undefined.
	if ( out_light )
		*out_light = nullptr;
	

	s_logger.printTrace("//EnvironmentLight creation\r\n");
	s_logger.Trace__FlushAllFiles(); // in case a crash, ensure trace informs the creation


	API_PRINT("rprContextCreateEnvironmentLight", 1, context);
	FrNode* ctx = static_cast<FrNode*>(context);

	try
	{
		CHECK_NOT_NULL(ctx);
		MACRO__CHECK_ARGUMENT_TYPE(context,Context);


		std::shared_ptr<FrSceneGraph> sceneGraph = ctx->GetProperty< std::shared_ptr<FrSceneGraph> >(FR_SCENEGRAPH);
		std::shared_ptr<FrRendererEncalps> renderer = ctx->GetProperty< std::shared_ptr<FrRendererEncalps> >(RPR_CONTEXT_ACTIVE_PLUGIN);
		FrNode* node = sceneGraph->CreateNode(NodeTypes::EnvironmentLight, [&](FrNode* n) {
			n->SetProperty(FR_NODE_CONTEXT, ctx);
            n->AddProperty(FR_SCENEGRAPH, sceneGraph);
			n->AddProperty(RPR_CONTEXT_ACTIVE_PLUGIN, renderer);
		});

		*out_light = node;

		s_logger.Trace__NewFrObjectCreated(Logger::TRACE_ADDRESS_TYPE__LIGHT, *out_light);
		s_logger.Trace__DeclareFRobject("rpr_light", "light_0x", *out_light);
		s_logger.printTrace("status = ");
		TRACE__FUNCTION_OPEN
		s_logger.TraceArg__rpr_context(context); s_logger.TraceArg__COMMA();
		s_logger.printTrace("&light_0x%p", *out_light);
		s_logger.Trace__FunctionClose();
		s_logger.printTrace("status = rprObjectSetName (light_0x%p,\"light_0x%p\"); RPRTRACE_CHECK// added by tracing for debug\r\n",  *out_light ,  *out_light);

		return RPR_SUCCESS;
	}
	catch (FrException& e)
	{
		TRACE__FUNCTION_FAILED(context); return e.GetErrorCode();
	}
}

extern RPR_API_ENTRY rpr_int rprHeteroVolumeSetTransform(
	rpr_hetero_volume heteroVolume, 
	rpr_bool transpose, 
	rpr_float const * transform)
{
	s_logger.TraceArg_Prepare__rpr_float_P16(transform);
	s_logger.printTrace("status = ");
	TRACE__FUNCTION_OPEN;
	s_logger.TraceArg__rpr_hetero_volume(heteroVolume); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_bool(transpose); s_logger.TraceArg__COMMA();
	s_logger.TraceArg_Use__rpr_float_P16(transform);
	s_logger.Trace__FunctionClose();

	API_PRINT("rprHeteroVolumeSetTransform", 1, heteroVolume);
	FrNode* node = static_cast<FrNode*>(heteroVolume);

	try
	{
		CHECK_NOT_NULL(node);
		MACRO__CHECK_ARGUMENT_TYPE(heteroVolume,HeteroVolume);

		for(int i=0; i<16; i++)
		{
			//if we give NAN here, we may have a TDR during rendering
			MACRO__CHECK_ARGUMENT_FLOAT_NAN(transform[i],heteroVolume);
			MACRO__CHECK_ARGUMENT_FLOAT_INF(transform[i],heteroVolume);
		}

		matrix m(transform[0],  transform[1],  transform[2],  transform[3],
			     transform[4],  transform[5],  transform[6],  transform[7],
			     transform[8],  transform[9],  transform[10], transform[11],
			     transform[12], transform[13], transform[14], transform[15]);


		#ifdef RPR_FOR_BAIKAL
		if (!transpose)
		#else
		if (transpose)
		#endif
			m = m.transpose();

		node->SetProperty(RPR_HETEROVOLUME_TRANSFORM, m);

		return RPR_SUCCESS;
	}
	catch (FrException& e)
	{
		TRACE__FUNCTION_FAILED(node); return e.GetErrorCode();
	}
}

rpr_int rprHeteroVolumeSetEmission(rpr_hetero_volume heteroVolume, rpr_float r, rpr_float g, rpr_float b)
{
	s_logger.printTrace("status = ");
	TRACE__FUNCTION_OPEN;
	s_logger.TraceArg__rpr_hetero_volume(heteroVolume); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_float(r); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_float(g); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_float(b); 
	s_logger.Trace__FunctionClose();

	API_PRINT("rprHeteroVolumeSetEmission", 1, heteroVolume);
	FrNode* node = static_cast<FrNode*>(heteroVolume);

	try
	{
		CHECK_NOT_NULL(node);
		MACRO__CHECK_ARGUMENT_TYPE(heteroVolume,HeteroVolume);

		node->SetProperty(RPR_HETEROVOLUME_EMISSION,  rpr_float3{ r,g,b } );

		return RPR_SUCCESS;
	}
	catch (FrException& e)
	{
		TRACE__FUNCTION_FAILED(node); return e.GetErrorCode();
	}
}

rpr_int rprHeteroVolumeSetAlbedo(rpr_hetero_volume heteroVolume, rpr_float r, rpr_float g, rpr_float b)
{
	s_logger.printTrace("status = ");
	TRACE__FUNCTION_OPEN;
	s_logger.TraceArg__rpr_hetero_volume(heteroVolume); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_float(r); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_float(g); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_float(b); 
	s_logger.Trace__FunctionClose();

	API_PRINT("rprHeteroVolumeSetAlbedo", 1, heteroVolume);
	FrNode* node = static_cast<FrNode*>(heteroVolume);

	try
	{
		CHECK_NOT_NULL(node);
		MACRO__CHECK_ARGUMENT_TYPE(heteroVolume,HeteroVolume);

		node->SetProperty(RPR_HETEROVOLUME_ALBEDO,  rpr_float3{ r,g,b } );

		return RPR_SUCCESS;
	}
	catch (FrException& e)
	{
		TRACE__FUNCTION_FAILED(node); return e.GetErrorCode();
	}
}

rpr_int rprHeteroVolumeSetFilter(rpr_hetero_volume heteroVolume, rpr_hetero_volume_filter filter)
{
	s_logger.printTrace("status = ");
	TRACE__FUNCTION_OPEN;
	s_logger.TraceArg__rpr_hetero_volume(heteroVolume); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_hetero_volume_filter(filter); 
	s_logger.Trace__FunctionClose();

	API_PRINT("rprHeteroVolumeSetFilter", 1, heteroVolume);
	FrNode* node = static_cast<FrNode*>(heteroVolume);

	try
	{
		CHECK_NOT_NULL(node);
		MACRO__CHECK_ARGUMENT_TYPE(heteroVolume,HeteroVolume);

		node->SetProperty(RPR_HETEROVOLUME_FILTER,  filter );

		return RPR_SUCCESS;
	}
	catch (FrException& e)
	{
		TRACE__FUNCTION_FAILED(node); return e.GetErrorCode();
	}
}


rpr_int rprContextCreateHeteroVolume(
									rpr_context context, 
									rpr_hetero_volume * out_heteroVolume, 
									size_t gridSizeX , size_t gridSizeY, size_t gridSizeZ,
									
									const void * indicesList, size_t numberOfIndices,
									rpr_hetero_volume_indices_topology indicesListTopology,

									const void * gridData, size_t gridData_SizeByte,
									rpr_uint gridDataTopology
									)
{
	//in case of issues during creation, it's safer to return a null pointer, rather than let the pointer undefined.
	if ( out_heteroVolume )
		*out_heteroVolume = nullptr;
	

	if (s_logger.IsTraceActivated())
	{
		s_logger.printTrace("\r\n//Hetero Volume creation\r\n");
		s_logger.printTrace("//size in data file for this heteroVolume: %llu\r\n", gridData_SizeByte);
		s_logger.TraceArg_Prepare__DATA(indicesList, numberOfIndices*sizeof(size_t), "pData1");
		s_logger.TraceArg_Prepare__DATA(gridData, gridData_SizeByte, "pData2");
		s_logger.Trace__FlushAllFiles(); // in case a crash, ensure trace informs the creation
	}

	API_PRINT("rprContextCreateHeteroVolume", 1, context);
	FrNode* ctx = static_cast<FrNode*>(context);

	try
	{
		CHECK_NOT_NULL(ctx);

		//check size :
		if ( gridData_SizeByte != numberOfIndices*4*sizeof(float) )
		{
			throw FrException(__FILE__,__LINE__,RPR_ERROR_INVALID_PARAMETER, "wrong gridData_SizeByte",context);
		}


		size_t indicesBufferSize = 0;
		if ( indicesListTopology == RPR_HETEROVOLUME_INDICES_TOPOLOGY_I_U64 )
		{
			indicesBufferSize = numberOfIndices*sizeof(uint64_t);
		}
		else if ( indicesListTopology == RPR_HETEROVOLUME_INDICES_TOPOLOGY_XYZ_U32 )
		{
			indicesBufferSize = numberOfIndices*3*sizeof(uint32_t);
		}
		else if ( indicesListTopology == RPR_HETEROVOLUME_INDICES_TOPOLOGY_I_S64 )
		{
			indicesBufferSize = numberOfIndices*sizeof(int64_t);
		}
		else if ( indicesListTopology == RPR_HETEROVOLUME_INDICES_TOPOLOGY_XYZ_S32 )
		{
			indicesBufferSize = numberOfIndices*3*sizeof(int32_t);
		}
		else
		{
			throw FrException(__FILE__,__LINE__,RPR_ERROR_INVALID_PARAMETER, "wrong indicesListTopology",context);
		}


		MACRO__CHECK_ARGUMENT_TYPE(context,Context);

		std::shared_ptr<FrSceneGraph> sceneGraph = ctx->GetProperty< std::shared_ptr<FrSceneGraph> >(FR_SCENEGRAPH);
		std::shared_ptr<FrRendererEncalps> renderer = ctx->GetProperty< std::shared_ptr<FrRendererEncalps> >(RPR_CONTEXT_ACTIVE_PLUGIN);

		std::shared_ptr<unsigned char> sharedDataPtr(new unsigned char[gridData_SizeByte], std::default_delete<unsigned char[]>());
		memcpy(sharedDataPtr.get(), gridData, gridData_SizeByte);

		std::shared_ptr<unsigned char> sharedIndicesPtr(new unsigned char[indicesBufferSize], std::default_delete<unsigned char[]>());
		memcpy(sharedIndicesPtr.get(), indicesList, indicesBufferSize);

		FrNode* node = sceneGraph->CreateNode(NodeTypes::HeteroVolume, [&](FrNode* n) {
			n->SetProperty(FR_NODE_CONTEXT, ctx);
            n->AddProperty(FR_SCENEGRAPH, sceneGraph);
			n->AddProperty(RPR_CONTEXT_ACTIVE_PLUGIN, renderer);

			n->SetProperty(RPR_HETEROVOLUME_SIZE_X,    gridSizeX);
			n->SetProperty(RPR_HETEROVOLUME_SIZE_Y,    gridSizeY);
			n->SetProperty(RPR_HETEROVOLUME_SIZE_Z,    gridSizeZ);
			n->SetProperty(RPR_HETEROVOLUME_DATA,      std::move(sharedDataPtr));
			n->SetProperty(RPR_HETEROVOLUME_DATA_SIZEBYTE, gridData_SizeByte);
			n->SetProperty(RPR_HETEROVOLUME_INDICES,      std::move(sharedIndicesPtr));
			n->SetProperty(RPR_HETEROVOLUME_INDICES_NUMBER, numberOfIndices);
			n->SetProperty(RPR_HETEROVOLUME_INDICES_TOPOLOGY, indicesListTopology);
		});

		*out_heteroVolume = node;

		s_logger.Trace__NewFrObjectCreated(Logger::TRACE_ADDRESS_TYPE__HETEROVOLUME, *out_heteroVolume);
		s_logger.Trace__DeclareFRobject("rpr_hetero_volume", "heterovolume_0x", *out_heteroVolume);
		s_logger.printTrace("status = ");
		TRACE__FUNCTION_OPEN
		s_logger.TraceArg__rpr_context(context); s_logger.TraceArg__COMMA();
		s_logger.printTrace("&heterovolume_0x%p", *out_heteroVolume);  s_logger.TraceArg__COMMA();
		s_logger.TraceArg__size_t(gridSizeX); s_logger.TraceArg__COMMA();
		s_logger.TraceArg__size_t(gridSizeY); s_logger.TraceArg__COMMA();
		s_logger.TraceArg__size_t(gridSizeZ); s_logger.TraceArg__COMMA();
		s_logger.TraceArg_Use__DATA_const_void_P("pData1"); s_logger.TraceArg__COMMA();
		s_logger.TraceArg__size_t(numberOfIndices); s_logger.TraceArg__COMMA();
		s_logger.TraceArg__rpr_hetero_volume_indices_topology(indicesListTopology); s_logger.TraceArg__COMMA();
		s_logger.TraceArg_Use__DATA_const_void_P("pData2"); s_logger.TraceArg__COMMA();
		s_logger.TraceArg__size_t(gridData_SizeByte); s_logger.TraceArg__COMMA();
		s_logger.TraceArg__rpr_uint(gridDataTopology); 
		s_logger.Trace__FunctionClose();
		s_logger.printTrace("status = rprObjectSetName (heterovolume_0x%p,\"heterovolume_0x%p\"); RPRTRACE_CHECK// added by tracing for debug\r\n",  *out_heteroVolume ,  *out_heteroVolume);

		return RPR_SUCCESS;
	}
	catch (FrException& e)
	{
		TRACE__FUNCTION_FAILED(context); return e.GetErrorCode();
	}
}


rpr_int rprContextCreateComposite(rpr_context context,  rpr_composite_type in_type, rpr_composite* out_composite)
{
	//in case of issues during creation, it's safer to return a null pointer, rather than let the pointer undefined.
	if ( out_composite )
		*out_composite = nullptr;
	

	s_logger.printTrace("//Composite creation\r\n");
	s_logger.Trace__FlushAllFiles(); // in case a crash, ensure trace informs the creation

	API_PRINT("rprContextCreateComposite", 1, context);
	FrNode* ctx = static_cast<FrNode*>(context);

	try
	{
		CHECK_NOT_NULL(ctx);

		MACRO__CHECK_ARGUMENT_TYPE(context,Context);


		std::shared_ptr<FrSceneGraph> sceneGraph = ctx->GetProperty< std::shared_ptr<FrSceneGraph> >(FR_SCENEGRAPH);
		std::shared_ptr<FrRendererEncalps> renderer = ctx->GetProperty< std::shared_ptr<FrRendererEncalps> >(RPR_CONTEXT_ACTIVE_PLUGIN);
		FrNode* node = sceneGraph->CreateNode(NodeTypes::Composite, [&](FrNode* n) 
		{
			switch (in_type)
			{
			case RPR_COMPOSITE_GAMMA_CORRECTION:
				n->AddProperty<FrNode*>(RPR_COMPOSITE_GAMMA_CORRECTION_INPUT_COLOR, nullptr);
				break;
			case RPR_COMPOSITE_NORMALIZE:
				n->AddProperty<FrNode*>(RPR_COMPOSITE_NORMALIZE_INPUT_COLOR, nullptr);
				n->AddProperty<FrNode*>(RPR_COMPOSITE_NORMALIZE_INPUT_SHADOWCATCHER, nullptr);
				break;
			case RPR_COMPOSITE_LERP_VALUE:
				n->AddProperty<FrNode*>(RPR_COMPOSITE_LERP_VALUE_INPUT_COLOR0, nullptr);
				n->AddProperty<FrNode*>(RPR_COMPOSITE_LERP_VALUE_INPUT_COLOR1, nullptr);
				n->AddProperty<FrNode*>(RPR_COMPOSITE_LERP_VALUE_INPUT_WEIGHT, nullptr);
				break;
			case RPR_COMPOSITE_ARITHMETIC:
				n->AddProperty<FrNode*>(RPR_COMPOSITE_ARITHMETIC_INPUT_COLOR0, nullptr);
				n->AddProperty<FrNode*>(RPR_COMPOSITE_ARITHMETIC_INPUT_COLOR1, nullptr);
				n->AddProperty<rpr_material_node_arithmetic_operation>(RPR_COMPOSITE_ARITHMETIC_INPUT_OP, RPR_MATERIAL_NODE_OP_ADD);
				break;
			case RPR_COMPOSITE_FRAMEBUFFER:
				n->AddProperty<FrNode*>(RPR_COMPOSITE_FRAMEBUFFER_INPUT_FB, nullptr);
				break;
			case RPR_COMPOSITE_LUT:
				n->AddProperty<FrNode*>(RPR_COMPOSITE_LUT_INPUT_LUT, nullptr);
				n->AddProperty<FrNode*>(RPR_COMPOSITE_LUT_INPUT_COLOR, nullptr);
				break;
			case RPR_COMPOSITE_CONSTANT:
				n->AddProperty<rpr_float4>(RPR_COMPOSITE_CONSTANT_INPUT_VALUE, rpr_float4(0.0f ,0.0f ,0.0f ,0.0f ) );
				break;
			}


			n->SetProperty(FR_NODE_CONTEXT, ctx);
            n->AddProperty(FR_SCENEGRAPH, sceneGraph);
			n->AddProperty(RPR_CONTEXT_ACTIVE_PLUGIN, renderer);
			n->SetProperty(RPR_COMPOSITE_TYPE, in_type);
		});

		*out_composite = node;

		s_logger.Trace__NewFrObjectCreated(Logger::TRACE_ADDRESS_TYPE__COMPOSITE, *out_composite);
		s_logger.Trace__DeclareFRobject("rpr_composite", "composite_0x", *out_composite);
		s_logger.printTrace("status = ");
		TRACE__FUNCTION_OPEN
		s_logger.TraceArg__rpr_context(context); s_logger.TraceArg__COMMA();
		s_logger.TraceArg__rpr_composite_type(in_type); s_logger.TraceArg__COMMA();
		s_logger.printTrace("&composite_0x%p", *out_composite);
		s_logger.Trace__FunctionClose();
		s_logger.printTrace("status = rprObjectSetName (composite_0x%p,\"composite_0x%p\"); RPRTRACE_CHECK// added by tracing for debug\r\n",  *out_composite ,  *out_composite);

		return RPR_SUCCESS;
	}
	catch (FrException& e)
	{
		TRACE__FUNCTION_FAILED(context); return e.GetErrorCode();
	}
}

extern RPR_API_ENTRY rpr_int rprContextCreateLUTFromFile(rpr_context context, const rpr_char * fileLutPath,   rpr_lut* out_lut)
{
	//in case of issues during creation, it's safer to return a null pointer, rather than let the pointer undefined.
	if ( out_lut )
		*out_lut = nullptr;
	

	s_logger.printTrace("//LUT creation\r\n");
	s_logger.Trace__FlushAllFiles(); // in case a crash, ensure trace informs the creation

	API_PRINT("rprContextCreateLUTFromFile", 1, context);
	FrNode* ctx = static_cast<FrNode*>(context);

	try
	{
		CHECK_NOT_NULL(ctx);

		MACRO__CHECK_ARGUMENT_TYPE(context,Context);

		std::shared_ptr<FrSceneGraph> sceneGraph = ctx->GetProperty< std::shared_ptr<FrSceneGraph> >(FR_SCENEGRAPH);
		std::shared_ptr<FrRendererEncalps> renderer = ctx->GetProperty< std::shared_ptr<FrRendererEncalps> >(RPR_CONTEXT_ACTIVE_PLUGIN);
		FrNode* node = sceneGraph->CreateNode(NodeTypes::LUT, [&](FrNode* n) 
		{
			n->SetProperty(RPR_LUT_FILENAME, std::string(fileLutPath));
			n->SetProperty(RPR_LUT_DATA, std::string(""));
			n->SetProperty(FR_NODE_CONTEXT, ctx);
            n->AddProperty(FR_SCENEGRAPH, sceneGraph);
			n->AddProperty(RPR_CONTEXT_ACTIVE_PLUGIN, renderer);
		});

		*out_lut = node;

		s_logger.Trace__NewFrObjectCreated(Logger::TRACE_ADDRESS_TYPE__LUT, *out_lut);
		s_logger.Trace__DeclareFRobject("rpr_lut", "lut_0x", *out_lut);
		s_logger.printTrace("status = ");
		TRACE__FUNCTION_OPEN
		s_logger.TraceArg__rpr_context(context); s_logger.TraceArg__COMMA();
		s_logger.TraceArg__rpr_char_P(fileLutPath); s_logger.TraceArg__COMMA();
		s_logger.printTrace("&lut_0x%p", *out_lut);
		s_logger.Trace__FunctionClose();
		s_logger.printTrace("status = rprObjectSetName (lut_0x%p,\"lut_0x%p\"); RPRTRACE_CHECK// added by tracing for debug\r\n",  *out_lut ,  *out_lut );

		return RPR_SUCCESS;
	}
	catch (FrException& e)
	{
		TRACE__FUNCTION_FAILED(context); return e.GetErrorCode();
	}
}

extern RPR_API_ENTRY rpr_int rprContextCreateLUTFromData(rpr_context context, const rpr_char * lutData,   rpr_lut* out_lut)
{
	//in case of issues during creation, it's safer to return a null pointer, rather than let the pointer undefined.
	if ( out_lut )
		*out_lut = nullptr;
	

	s_logger.printTrace("//LUT creation\r\n");
	s_logger.Trace__FlushAllFiles(); // in case a crash, ensure trace informs the creation

	API_PRINT("rprContextCreateLUTFromData", 1, context);
	FrNode* ctx = static_cast<FrNode*>(context);

	try
	{
		CHECK_NOT_NULL(ctx);

		MACRO__CHECK_ARGUMENT_TYPE(context,Context);

		std::shared_ptr<FrSceneGraph> sceneGraph = ctx->GetProperty< std::shared_ptr<FrSceneGraph> >(FR_SCENEGRAPH);
		std::shared_ptr<FrRendererEncalps> renderer = ctx->GetProperty< std::shared_ptr<FrRendererEncalps> >(RPR_CONTEXT_ACTIVE_PLUGIN);
		FrNode* node = sceneGraph->CreateNode(NodeTypes::LUT, [&](FrNode* n) 
		{
			n->SetProperty(RPR_LUT_FILENAME, std::string(""));
			n->SetProperty(RPR_LUT_DATA, std::string(lutData));
			n->SetProperty(FR_NODE_CONTEXT, ctx);
            n->AddProperty(FR_SCENEGRAPH, sceneGraph);
			n->AddProperty(RPR_CONTEXT_ACTIVE_PLUGIN, renderer);
		});

		*out_lut = node;

		s_logger.Trace__NewFrObjectCreated(Logger::TRACE_ADDRESS_TYPE__LUT, *out_lut);
		s_logger.Trace__DeclareFRobject("rpr_lut", "lut_0x", *out_lut);
		s_logger.printTrace("status = ");
		TRACE__FUNCTION_OPEN
		s_logger.TraceArg__rpr_context(context); s_logger.TraceArg__COMMA();
		s_logger.TraceArg__rpr_char_P(lutData); s_logger.TraceArg__COMMA();
		s_logger.printTrace("&lut_0x%p", *out_lut);
		s_logger.Trace__FunctionClose();
		s_logger.printTrace("status = rprObjectSetName (lut_0x%p,\"lut_0x%p\"); RPRTRACE_CHECK// added by tracing for debug\r\n",  *out_lut ,  *out_lut );

		return RPR_SUCCESS;
	}
	catch (FrException& e)
	{
		TRACE__FUNCTION_FAILED(context); return e.GetErrorCode();
	}
}

extern RPR_API_ENTRY rpr_int rprCompositeSetInputFb( rpr_composite composite, const rpr_char* inputName, rpr_framebuffer input )
{
	
	s_logger.printTrace("status = ");
	TRACE__FUNCTION_OPEN
	s_logger.TraceArg__rpr_composite(composite); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_char_P(inputName); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_framebuffer(input); 
	s_logger.Trace__FunctionClose();

	API_PRINT("rprCompositeSetInputFB", 1, composite);

	try
	{
		CHECK_NOT_NULL(static_cast<FrNode*>(composite));
		MACRO__CHECK_ARGUMENT_TYPE(composite,Composite);
		MACRO__CHECK_ARGUMENT_TYPE(input,FrameBuffer);


		// Convert named string to key and call overloaded method which takes the key.
		std::string lcname = inputName;
		std::transform(lcname.begin(), lcname.end(), lcname.begin(), ::tolower);
		auto itr = CompositeParameterNamesToKeys.find(lcname);
		if (itr != CompositeParameterNamesToKeys.cend())
		{
			FrNode* composite_frnode = static_cast<FrNode*>(composite);
			FrNode* input_frnode = static_cast<FrNode*>(input);
			composite_frnode->SetProperty(itr->second, input_frnode);
		}
		else
		{
			FrNode* composite_frnode = static_cast<FrNode*>(composite);
			throw FrException(__FILE__,__LINE__,RPR_ERROR_INVALID_PARAMETER, "",composite_frnode);
		}

		return RPR_SUCCESS;
	}
	catch (FrException& e)
	{
		TRACE__FUNCTION_FAILED(composite); return e.GetErrorCode();
	}

}

extern RPR_API_ENTRY rpr_int rprCompositeSetInputC( rpr_composite composite, const rpr_char* inputName, rpr_composite input )
{
	
	s_logger.printTrace("status = ");
	TRACE__FUNCTION_OPEN
	s_logger.TraceArg__rpr_composite(composite); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_char_P(inputName); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_composite(input); 
	s_logger.Trace__FunctionClose();

	API_PRINT("rprCompositeSetInputFB", 1, composite);

	try
	{
		CHECK_NOT_NULL(static_cast<FrNode*>(composite));
		MACRO__CHECK_ARGUMENT_TYPE(composite,Composite);
		MACRO__CHECK_ARGUMENT_TYPE(input,Composite);


		// Convert named string to key and call overloaded method which takes the key.
		std::string lcname = inputName;
		std::transform(lcname.begin(), lcname.end(), lcname.begin(), ::tolower);
		auto itr = CompositeParameterNamesToKeys.find(lcname);
		if (itr != CompositeParameterNamesToKeys.cend())
		{
			FrNode* composite_frnode = static_cast<FrNode*>(composite);
			FrNode* input_frnode = static_cast<FrNode*>(input);
			composite_frnode->SetProperty(itr->second, input_frnode);
		}
		else
		{
			FrNode* composite_frnode = static_cast<FrNode*>(composite);
			throw FrException(__FILE__,__LINE__,RPR_ERROR_INVALID_PARAMETER, "",composite_frnode);
		}

		return RPR_SUCCESS;

	}
	catch (FrException& e)
	{
		TRACE__FUNCTION_FAILED(composite); return e.GetErrorCode();
	}

}


extern RPR_API_ENTRY rpr_int rprCompositeSetInputLUT( rpr_composite composite, const rpr_char* inputName, rpr_lut input )
{
	
	s_logger.printTrace("status = ");
	TRACE__FUNCTION_OPEN
	s_logger.TraceArg__rpr_composite(composite); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_char_P(inputName); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_lut(input); 
	s_logger.Trace__FunctionClose();

	API_PRINT("rprCompositeSetInputLUT", 1, composite);

	try
	{
		CHECK_NOT_NULL(static_cast<FrNode*>(composite));
		MACRO__CHECK_ARGUMENT_TYPE(composite,Composite);
		MACRO__CHECK_ARGUMENT_TYPE(input,LUT);


		// Convert named string to key and call overloaded method which takes the key.
		std::string lcname = inputName;
		std::transform(lcname.begin(), lcname.end(), lcname.begin(), ::tolower);
		auto itr = CompositeParameterNamesToKeys.find(lcname);
		if (itr != CompositeParameterNamesToKeys.cend())
		{
			FrNode* composite_frnode = static_cast<FrNode*>(composite);
			FrNode* input_frnode = static_cast<FrNode*>(input);
			composite_frnode->SetProperty(itr->second, input_frnode);
		}
		else
		{
			FrNode* composite_frnode = static_cast<FrNode*>(composite);
			throw FrException(__FILE__,__LINE__,RPR_ERROR_INVALID_PARAMETER, "",composite_frnode);
		}

		return RPR_SUCCESS;

	}
	catch (FrException& e)
	{
		TRACE__FUNCTION_FAILED(composite); return e.GetErrorCode();
	}

}

extern RPR_API_ENTRY rpr_int rprCompositeSetInput4f( rpr_composite composite, const rpr_char* inputName, float x, float y, float z, float w )
{
	
	s_logger.printTrace("status = ");
	TRACE__FUNCTION_OPEN
	s_logger.TraceArg__rpr_composite(composite); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_char_P(inputName); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_float(x); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_float(y); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_float(z); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_float(w);
	s_logger.Trace__FunctionClose();

	API_PRINT("rprCompositeSetInput4F", 1, composite);

	try
	{
		CHECK_NOT_NULL(static_cast<FrNode*>(composite));
		MACRO__CHECK_ARGUMENT_TYPE(composite,Composite);

		// Convert named string to key and call overloaded method which takes the key.
		std::string lcname = inputName;
		std::transform(lcname.begin(), lcname.end(), lcname.begin(), ::tolower);
		auto itr = CompositeParameterNamesToKeys.find(lcname);
		if (itr != CompositeParameterNamesToKeys.cend())
		{
			FrNode* composite_frnode = static_cast<FrNode*>(composite);
			composite_frnode->SetProperty(itr->second, rpr_float4(x,y,z,w));
		}
		else
		{
			FrNode* composite_frnode = static_cast<FrNode*>(composite);
			throw FrException(__FILE__,__LINE__,RPR_ERROR_INVALID_PARAMETER, "",composite_frnode);
		}

		return RPR_SUCCESS;

	}
	catch (FrException& e)
	{
		TRACE__FUNCTION_FAILED(composite); return e.GetErrorCode();
	}
}

rpr_int rprCompositeSetInput1u( rpr_composite composite, const rpr_char* inputName, unsigned int key )
{
	s_logger.printTrace("status = ");
	TRACE__FUNCTION_OPEN
	s_logger.TraceArg__rpr_composite(composite); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_char_P(inputName); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_uint(key);
	s_logger.Trace__FunctionClose();

	API_PRINT("rprCompositeSetInput1u", 1, composite);

	try
	{
		FrNode* composite_frnode = static_cast<FrNode*>(composite);
		CHECK_NOT_NULL(composite_frnode);
		
		// throw because this function is doing nothing.
		throw FrException(__FILE__,__LINE__,RPR_ERROR_INVALID_PARAMETER, "",composite_frnode);
	}
	catch (FrException& e)
	{
		TRACE__FUNCTION_FAILED(composite); return e.GetErrorCode();
	}

}

rpr_int rprCompositeSetInputOp( rpr_composite composite, const rpr_char* inputName, rpr_material_node_arithmetic_operation op )
{

	s_logger.printTrace("status = ");
	TRACE__FUNCTION_OPEN
	s_logger.TraceArg__rpr_composite(composite); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_char_P(inputName); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_material_node_arithmetic_operation(op); 
	s_logger.Trace__FunctionClose();

	API_PRINT("rprCompositeSetInputOp", 1, composite);

	try
	{
		CHECK_NOT_NULL(static_cast<FrNode*>(composite));
		MACRO__CHECK_ARGUMENT_TYPE(composite,Composite);

		// Convert named string to key and call overloaded method which takes the key.
		std::string lcname = inputName;
		std::transform(lcname.begin(), lcname.end(), lcname.begin(), ::tolower);
		auto itr = CompositeParameterNamesToKeys.find(lcname);
		if (itr != CompositeParameterNamesToKeys.cend())
		{
			FrNode* composite_frnode = static_cast<FrNode*>(composite);
			composite_frnode->SetProperty(itr->second, op);
		}
		else
		{
			FrNode* composite_frnode = static_cast<FrNode*>(composite);
			throw FrException(__FILE__,__LINE__,RPR_ERROR_INVALID_PARAMETER, "",composite_frnode);
		}

		return RPR_SUCCESS;

	}
	catch (FrException& e)
	{
		TRACE__FUNCTION_FAILED(composite); return e.GetErrorCode();
	}

}

rpr_int rprCompositeCompute( rpr_composite composite, rpr_framebuffer fb )
{
	s_logger.printTrace("status = ");
	TRACE__FUNCTION_OPEN
	s_logger.TraceArg__rpr_composite(composite); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_framebuffer(fb);  
	s_logger.Trace__FunctionClose();

	API_PRINT("rprCompositeCompute", 1, composite);

	try
	{
		CHECK_NOT_NULL(static_cast<FrNode*>(composite));
		MACRO__CHECK_ARGUMENT_TYPE(composite,Composite);
		MACRO__CHECK_ARGUMENT_TYPE(fb,FrameBuffer);

		FrNode* node = static_cast<FrNode*>(fb);
		FrNode* ctx = node->GetProperty<FrNode*>(FR_NODE_CONTEXT);

		std::shared_ptr<FrRendererEncalps> renderer = ctx->GetProperty<  std::shared_ptr<FrRendererEncalps>  >(RPR_CONTEXT_ACTIVE_PLUGIN);
		if (!renderer->m_FrRenderer)
			throw FrException(__FILE__,__LINE__,RPR_ERROR_INVALID_OBJECT, "No active compute API set",composite);

		renderer->m_FrRenderer->ResolveComposite(static_cast<FrNode*>(composite), static_cast<FrNode*>(fb) );
		
		return RPR_SUCCESS;

	}
	catch (FrException& e)
	{
		TRACE__FUNCTION_FAILED(composite); return e.GetErrorCode();
	}

}


rpr_int rprEnvironmentLightSetImage(rpr_light env_light, rpr_image image)
{
	s_logger.printTrace("status = ");
	TRACE__FUNCTION_OPEN;
	s_logger.TraceArg__rpr_light(env_light); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_image(image);
	s_logger.Trace__FunctionClose();

	API_PRINT("rprEnvironmentLightSetImage", 1, env_light);
	FrNode* node = static_cast<FrNode*>(env_light);

	try
	{
		CHECK_NOT_NULL(node);
		MACRO__CHECK_ARGUMENT_TYPE(env_light,EnvironmentLight);
		MACRO__CHECK_ARGUMENT_TYPE(image,Image);


		node->SetProperty(RPR_ENVIRONMENT_LIGHT_IMAGE, static_cast<FrNode*>(image));
		return RPR_SUCCESS;
	}
	catch (FrException& e)
	{
		TRACE__FUNCTION_FAILED(node); return e.GetErrorCode();
	}
}



rpr_int rprEnvironmentLightSetIntensityScale(rpr_light env_light, rpr_float intensity_scale)
{
	s_logger.printTrace("status = ");
	TRACE__FUNCTION_OPEN;
	s_logger.TraceArg__rpr_light(env_light); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_float(intensity_scale);
	s_logger.Trace__FunctionClose();

	API_PRINT("rprEnvironmentLightSetIntensityScale", 1, env_light);
	FrNode* node = static_cast<FrNode*>(env_light);

	try
	{
		CHECK_NOT_NULL(node);
		MACRO__CHECK_ARGUMENT_TYPE(env_light,EnvironmentLight);


		node->SetProperty(RPR_ENVIRONMENT_LIGHT_INTENSITY_SCALE, intensity_scale);
		return RPR_SUCCESS;
	}
	catch (FrException& e)
	{
		TRACE__FUNCTION_FAILED(node); return e.GetErrorCode();
	}
}

rpr_int rprContextCreateSkyLight(rpr_context context, rpr_light* out_light)
{
	//in case of issues during creation, it's safer to return a null pointer, rather than let the pointer undefined.
	if ( out_light )
		*out_light = nullptr;
	

	s_logger.printTrace("//SkyLight creation\r\n");
	s_logger.Trace__FlushAllFiles(); // in case a crash, ensure trace informs the creation


	API_PRINT("rprContextCreateSkyLight", 1, context);
	FrNode* ctx = static_cast<FrNode*>(context);

	try
	{
		CHECK_NOT_NULL(ctx);
		MACRO__CHECK_ARGUMENT_TYPE(context,Context);


		std::shared_ptr<FrSceneGraph> sceneGraph = ctx->GetProperty< std::shared_ptr<FrSceneGraph> >(FR_SCENEGRAPH);
		std::shared_ptr<FrRendererEncalps> renderer = ctx->GetProperty< std::shared_ptr<FrRendererEncalps> >(RPR_CONTEXT_ACTIVE_PLUGIN);
		FrNode* node = sceneGraph->CreateNode(NodeTypes::SkyLight, [&](FrNode* n) {
			n->SetProperty(FR_NODE_CONTEXT, ctx);
            n->AddProperty(FR_SCENEGRAPH, sceneGraph);
			n->AddProperty(RPR_CONTEXT_ACTIVE_PLUGIN, renderer);
		});

		*out_light = node;

		s_logger.Trace__NewFrObjectCreated(Logger::TRACE_ADDRESS_TYPE__LIGHT, *out_light);
		s_logger.Trace__DeclareFRobject("rpr_light", "light_0x", *out_light);
		s_logger.printTrace("status = ");
		TRACE__FUNCTION_OPEN
		s_logger.TraceArg__rpr_context(context); s_logger.TraceArg__COMMA();
		s_logger.printTrace("&light_0x%p", *out_light);
		s_logger.Trace__FunctionClose();
		s_logger.printTrace("status = rprObjectSetName (light_0x%p,\"light_0x%p\"); RPRTRACE_CHECK// added by tracing for debug\r\n",  *out_light ,  *out_light);

		return RPR_SUCCESS;
	}
	catch (FrException& e)
	{
		TRACE__FUNCTION_FAILED(context); return e.GetErrorCode();
	}
}

rpr_int rprSkyLightSetTurbidity(rpr_light skylight, rpr_float turbidity)
{
	s_logger.printTrace("status = ");
	TRACE__FUNCTION_OPEN;
	s_logger.TraceArg__rpr_light(skylight); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_float(turbidity);
	s_logger.Trace__FunctionClose();

	API_PRINT("rprSkyLightSetTurbidity", 1, skylight);
	FrNode* node = static_cast<FrNode*>(skylight);

	try
	{
		CHECK_NOT_NULL(node);
		MACRO__CHECK_ARGUMENT_TYPE(skylight,SkyLight);


		node->SetProperty(RPR_SKY_LIGHT_TURBIDITY, turbidity);
		return RPR_SUCCESS;
	}
	catch (FrException& e)
	{
		TRACE__FUNCTION_FAILED(node); return e.GetErrorCode();
	}
}

rpr_int rprSkyLightSetAlbedo(rpr_light skylight, rpr_float albedo)
{
	s_logger.printTrace("status = ");
	TRACE__FUNCTION_OPEN;
	s_logger.TraceArg__rpr_light(skylight); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_float(albedo);
	s_logger.Trace__FunctionClose();

	API_PRINT("rprSkyLightSetAlbedo", 1, skylight);
	FrNode* node = static_cast<FrNode*>(skylight);

	try
	{
		CHECK_NOT_NULL(node);
		MACRO__CHECK_ARGUMENT_TYPE(skylight,SkyLight);


		node->SetProperty(RPR_SKY_LIGHT_ALBEDO, albedo);
		return RPR_SUCCESS;
	}
	catch (FrException& e)
	{
		TRACE__FUNCTION_FAILED(node); return e.GetErrorCode();
	}
}

rpr_int rprSkyLightSetScale(rpr_light skylight, rpr_float scale)
{
	s_logger.printTrace("status = ");
	TRACE__FUNCTION_OPEN;
	s_logger.TraceArg__rpr_light(skylight); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_float(scale);
	s_logger.Trace__FunctionClose();

	API_PRINT("rprSkyLightSetScale", 1, skylight);
	FrNode* node = static_cast<FrNode*>(skylight);

	try
	{
		CHECK_NOT_NULL(node);
		MACRO__CHECK_ARGUMENT_TYPE(skylight,SkyLight);


		node->SetProperty(RPR_SKY_LIGHT_SCALE, scale);
		return RPR_SUCCESS;
	}
	catch (FrException& e)
	{
		TRACE__FUNCTION_FAILED(node); return e.GetErrorCode();
	}

}

rpr_int rprSkyLightSetDirection(rpr_light skylight, rpr_float x, rpr_float y, rpr_float z )
{
	s_logger.printTrace("status = ");
	TRACE__FUNCTION_OPEN;
	s_logger.TraceArg__rpr_light(skylight); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_float(x); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_float(y); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_float(z); 
	s_logger.Trace__FunctionClose();

	API_PRINT("rprSkyLightSetDirection", 1, skylight);
	FrNode* node = static_cast<FrNode*>(skylight);

	try
	{
		CHECK_NOT_NULL(node);
		MACRO__CHECK_ARGUMENT_TYPE(skylight,SkyLight);


		node->SetProperty(RPR_SKY_LIGHT_DIRECTION, rpr_float3{x,y,z});
		return RPR_SUCCESS;
	}
	catch (FrException& e)
	{
		TRACE__FUNCTION_FAILED(node); return e.GetErrorCode();
	}
}

rpr_int rprEnvironmentLightAttachPortal(rpr_scene scene, rpr_light env_light, rpr_shape portal)
{
	s_logger.printTrace("status = ");
	TRACE__FUNCTION_OPEN;
	s_logger.TraceArg__rpr_scene(scene); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_light(env_light); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_shape(portal);
	s_logger.Trace__FunctionClose();

	API_PRINT("rprEnvironmentLightAttachPortal", 1, env_light);
	FrNode* node = static_cast<FrNode*>(env_light);
	FrNode* sh = static_cast<FrNode*>(portal);
	FrNode* sceneNode = static_cast<FrNode*>(scene);

	try
	{
		CHECK_NOT_NULL(sceneNode);
		MACRO__CHECK_ARGUMENT_TYPE(env_light,EnvironmentLight);
		MACRO__CHECK_ARGUMENT_TYPE_ALLSHAPES(portal);


		auto& shapeList = node->GetProperty<FrShapeList>(RPR_ENVIRONMENT_LIGHT_PORTAL_LIST);
		if (shapeList.find(sh) == shapeList.end())
		{
			shapeList.insert(sh);
			ListChangedArgs_Scene args(sceneNode, FireSG::ListChangedArgs::Op::ItemAdded, reinterpret_cast<void*>(sh));
			node->PropertyChanged(RPR_ENVIRONMENT_LIGHT_PORTAL_LIST, reinterpret_cast<void*>(&args));
		}

		return RPR_SUCCESS;
	}
	catch (FrException& e)
	{
		TRACE__FUNCTION_FAILED(node); return e.GetErrorCode();
	}
}

rpr_int rprEnvironmentLightDetachPortal(rpr_scene scene, rpr_light env_light, rpr_shape portal)
{
	s_logger.printTrace("status = ");
	TRACE__FUNCTION_OPEN
	s_logger.TraceArg__rpr_light(env_light); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_shape(portal);
	s_logger.Trace__FunctionClose();

	API_PRINT("rprEnvironmentLightDetachPortal", 1, env_light);
	FrNode* node = static_cast<FrNode*>(env_light);
	FrNode* sh = static_cast<FrNode*>(portal);
	FrNode* sceneNode = static_cast<FrNode*>(scene);

	try
	{
		CHECK_NOT_NULL(sceneNode);
		MACRO__CHECK_ARGUMENT_TYPE(env_light,EnvironmentLight);
		MACRO__CHECK_ARGUMENT_TYPE_ALLSHAPES(portal);


		auto& shapeList = node->GetProperty<FrShapeList>(RPR_ENVIRONMENT_LIGHT_PORTAL_LIST);
		auto itr = shapeList.find(sh);
		if (itr != shapeList.end())
		{
			shapeList.erase(itr);
			ListChangedArgs_Scene args(sceneNode, FireSG::ListChangedArgs::Op::ItemRemoved, reinterpret_cast<void*>(sh));
			node->PropertyChanged(RPR_ENVIRONMENT_LIGHT_PORTAL_LIST, reinterpret_cast<void*>(&args));
		}
	}
	catch (FrException& e)
	{
		TRACE__FUNCTION_FAILED(node); return e.GetErrorCode();
	}

	return RPR_SUCCESS;
}

rpr_int rprSkyLightAttachPortal(rpr_scene scene, rpr_light env_light, rpr_shape portal)
{
	s_logger.printTrace("status = ");
	TRACE__FUNCTION_OPEN;
	s_logger.TraceArg__rpr_light(env_light); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_shape(portal);
	s_logger.Trace__FunctionClose();

	API_PRINT("rprEnvironmentLightAttachPortal", 1, env_light);
	FrNode* node = static_cast<FrNode*>(env_light);
	FrNode* sh = static_cast<FrNode*>(portal);
	FrNode* sceneNode = static_cast<FrNode*>(scene);

	try
	{
		CHECK_NOT_NULL(sceneNode);
		MACRO__CHECK_ARGUMENT_TYPE(env_light,SkyLight);
		MACRO__CHECK_ARGUMENT_TYPE_ALLSHAPES(portal);


		auto& shapeList = node->GetProperty<FrShapeList>(RPR_SKY_LIGHT_PORTAL_LIST);
		if (shapeList.find(sh) == shapeList.end())
		{
			shapeList.insert(sh);
			ListChangedArgs_Scene args(sceneNode, FireSG::ListChangedArgs::Op::ItemAdded, reinterpret_cast<void*>(sh) );
			node->PropertyChanged(RPR_SKY_LIGHT_PORTAL_LIST, reinterpret_cast<void*>(&args));
		}

		return RPR_SUCCESS;
	}
	catch (FrException& e)
	{
		TRACE__FUNCTION_FAILED(node); return e.GetErrorCode();
	}
}

rpr_int rprSkyLightDetachPortal(rpr_scene scene, rpr_light env_light, rpr_shape portal)
{
	s_logger.printTrace("status = ");
	TRACE__FUNCTION_OPEN
	s_logger.TraceArg__rpr_light(env_light); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_shape(portal);
	s_logger.Trace__FunctionClose();

	API_PRINT("rprSkyLightDetachPortal", 1, env_light);
	FrNode* node = static_cast<FrNode*>(env_light);
	FrNode* sh = static_cast<FrNode*>(portal);
	FrNode* sceneNode = static_cast<FrNode*>(scene);

	try
	{
		CHECK_NOT_NULL(sceneNode);
		MACRO__CHECK_ARGUMENT_TYPE(env_light,SkyLight);
		MACRO__CHECK_ARGUMENT_TYPE_ALLSHAPES(portal);


		auto& shapeList = node->GetProperty<FrShapeList>(RPR_SKY_LIGHT_PORTAL_LIST);
		auto itr = shapeList.find(sh);
		if (itr != shapeList.end())
		{
			shapeList.erase(itr);
			ListChangedArgs_Scene args(sceneNode, FireSG::ListChangedArgs::Op::ItemRemoved, reinterpret_cast<void*>(sh));
			node->PropertyChanged(RPR_SKY_LIGHT_PORTAL_LIST, reinterpret_cast<void*>(&args));
		}
	}
	catch (FrException& e)
	{
		TRACE__FUNCTION_FAILED(node); return e.GetErrorCode();
	}

	return RPR_SUCCESS;
}



rpr_int rprSceneAttachShape(rpr_scene scene, rpr_shape shape)
{
	s_logger.printTrace("status = ");
	TRACE__FUNCTION_OPEN;
	s_logger.TraceArg__rpr_scene(scene); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_shape(shape);
	s_logger.Trace__FunctionClose();

	API_PRINT("rprSceneAttachShape", 1, scene);
	FrNode* node = static_cast<FrNode*>(scene);
	FrNode* sh = static_cast<FrNode*>(shape);

	try
	{
		CHECK_NOT_NULL(node);
		CHECK_NOT_NULL(sh);
		MACRO__CHECK_ARGUMENT_TYPE(scene,Scene);
		MACRO__CHECK_ARGUMENT_TYPE_ALLSHAPES(shape);


		auto& shapeList = node->GetProperty<FrShapeList>(RPR_SCENE_SHAPE_LIST);
		if (shapeList.find(sh) == shapeList.end())
		{
			shapeList.insert(sh);

			FireSG::ListChangedArgs args(FireSG::ListChangedArgs::Op::ItemAdded, reinterpret_cast<void*>(sh));
			node->PropertyChanged(RPR_SCENE_SHAPE_LIST, reinterpret_cast<void*>(&args));
		}
	}
	catch (FrException& e)
	{
		TRACE__FUNCTION_FAILED(node); return e.GetErrorCode();
	}

	return RPR_SUCCESS;
}

rpr_int rprSceneAttachHeteroVolume(rpr_scene scene, rpr_hetero_volume heteroVolume)
{
	s_logger.printTrace("status = ");
	TRACE__FUNCTION_OPEN;
	s_logger.TraceArg__rpr_scene(scene); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_hetero_volume(heteroVolume);
	s_logger.Trace__FunctionClose();

	API_PRINT("rprSceneAttachHeteroVolume", 1, scene);
	FrNode* node = static_cast<FrNode*>(scene);
	FrNode* sh = static_cast<FrNode*>(heteroVolume);

	try
	{
		CHECK_NOT_NULL(node);
		CHECK_NOT_NULL(sh);
		MACRO__CHECK_ARGUMENT_TYPE(scene,Scene);
		MACRO__CHECK_ARGUMENT_TYPE(heteroVolume,HeteroVolume);

		auto& heterovolumeList = node->GetProperty<FrHeteroVolumeList>(RPR_SCENE_HETEROVOLUME_LIST);
		if (heterovolumeList.find(sh) == heterovolumeList.end())
		{
			heterovolumeList.insert(sh);

			FireSG::ListChangedArgs args(FireSG::ListChangedArgs::Op::ItemAdded, reinterpret_cast<void*>(sh));
			node->PropertyChanged(RPR_SCENE_HETEROVOLUME_LIST, reinterpret_cast<void*>(&args));
		}
	}
	catch (FrException& e)
	{
		TRACE__FUNCTION_FAILED(node); return e.GetErrorCode();
	}

	return RPR_SUCCESS;
}


rpr_int rprSceneDetachShape(rpr_scene scene, rpr_shape shape)
{
	s_logger.printTrace("status = ");
	TRACE__FUNCTION_OPEN;
	s_logger.TraceArg__rpr_scene(scene); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_shape(shape);
	s_logger.Trace__FunctionClose();

	API_PRINT("rprSceneDetachShape", 1, scene);
	FrNode* node = static_cast<FrNode*>(scene);
	FrNode* sh = static_cast<FrNode*>(shape);

	try
	{
		CHECK_NOT_NULL(node);
		CHECK_NOT_NULL(sh);
		MACRO__CHECK_ARGUMENT_TYPE(scene,Scene);
		MACRO__CHECK_ARGUMENT_TYPE_ALLSHAPES(shape);


		auto& shapeList = node->GetProperty<FrShapeList>(RPR_SCENE_SHAPE_LIST);
		auto itr = shapeList.find(sh);
		if (itr != shapeList.end())
		{
			shapeList.erase(itr);
			FireSG::ListChangedArgs args(FireSG::ListChangedArgs::Op::ItemRemoved, reinterpret_cast<void*>(sh));
			node->PropertyChanged(RPR_SCENE_SHAPE_LIST, reinterpret_cast<void*>(&args));
		}
	}
	catch (FrException& e)
	{
		TRACE__FUNCTION_FAILED(node); return e.GetErrorCode();
	}

	return RPR_SUCCESS;
}

rpr_int rprSceneDetachHeteroVolume(rpr_scene scene, rpr_hetero_volume heteroVolume)
{
	s_logger.printTrace("status = ");
	TRACE__FUNCTION_OPEN;
	s_logger.TraceArg__rpr_scene(scene); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_hetero_volume(heteroVolume);
	s_logger.Trace__FunctionClose();

	API_PRINT("rprSceneDetachHeteroVolume", 1, scene);
	FrNode* node = static_cast<FrNode*>(scene);
	FrNode* sh = static_cast<FrNode*>(heteroVolume);

	try
	{
		CHECK_NOT_NULL(node);
		CHECK_NOT_NULL(sh);
		MACRO__CHECK_ARGUMENT_TYPE(scene,Scene);
		MACRO__CHECK_ARGUMENT_TYPE(heteroVolume,HeteroVolume);


		auto& heteroVolumeList = node->GetProperty<FrHeteroVolumeList>(RPR_SCENE_HETEROVOLUME_LIST);
		auto itr = heteroVolumeList.find(sh);
		if (itr != heteroVolumeList.end())
		{
			heteroVolumeList.erase(itr);
			FireSG::ListChangedArgs args(FireSG::ListChangedArgs::Op::ItemRemoved, reinterpret_cast<void*>(sh));
			node->PropertyChanged(RPR_SCENE_HETEROVOLUME_LIST, reinterpret_cast<void*>(&args));
		}
	}
	catch (FrException& e)
	{
		TRACE__FUNCTION_FAILED(node); return e.GetErrorCode();
	}

	return RPR_SUCCESS;
}


rpr_int rprSceneAttachLight(rpr_scene scene, rpr_light light)
{
	s_logger.printTrace("status = ");
	TRACE__FUNCTION_OPEN;
	s_logger.TraceArg__rpr_scene(scene); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_light(light);
	s_logger.Trace__FunctionClose();

	API_PRINT("rprSceneAttachLight", 1, scene);
	FrNode* node = static_cast<FrNode*>(scene);
	FrNode* l = static_cast<FrNode*>(light);

	try
	{
		CHECK_NOT_NULL(node);
		CHECK_NOT_NULL(l);
		MACRO__CHECK_ARGUMENT_TYPE(scene,Scene);
		MACRO__CHECK_ARGUMENT_TYPE_ALLLIGHTS(light);


		if ( !node || !l )
			throw FrException(__FILE__,__LINE__,RPR_ERROR_INVALID_PARAMETER, "null ptr",scene);

		auto& lights = node->GetProperty<FrLightList>(RPR_SCENE_LIGHT_LIST);
		if (lights.find(l) == lights.end())
		{
			lights.insert(l);
			FireSG::ListChangedArgs args(FireSG::ListChangedArgs::Op::ItemAdded, reinterpret_cast<void*>(l));
			node->PropertyChanged(RPR_SCENE_LIGHT_LIST, reinterpret_cast<void*>(&args));
		}
	}
	catch (FrException& e)
	{
		TRACE__FUNCTION_FAILED(node); return e.GetErrorCode();
	}

	return RPR_SUCCESS;
}

rpr_int rprSceneDetachLight(rpr_scene scene, rpr_light light)
{
	s_logger.printTrace("status = ");
	TRACE__FUNCTION_OPEN;
	s_logger.TraceArg__rpr_scene(scene); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_light(light);
	s_logger.Trace__FunctionClose();

	API_PRINT("rprSceneDetachLight", 1, scene);
	FrNode* node = static_cast<FrNode*>(scene);
	FrNode* l = static_cast<FrNode*>(light);

	try
	{
		CHECK_NOT_NULL(node);
		CHECK_NOT_NULL(l);
		MACRO__CHECK_ARGUMENT_TYPE(scene,Scene);
		MACRO__CHECK_ARGUMENT_TYPE_ALLLIGHTS(light);


		auto& lights = node->GetProperty<FrLightList>(RPR_SCENE_LIGHT_LIST);
		auto itr = lights.find(l);
		if (itr != lights.end())
		{
			lights.erase(itr);
			FireSG::ListChangedArgs args(FireSG::ListChangedArgs::Op::ItemRemoved, reinterpret_cast<void*>(l));
			node->PropertyChanged(RPR_SCENE_LIGHT_LIST, reinterpret_cast<void*>(&args));
		}
	}
	catch (FrException& e)
	{
		TRACE__FUNCTION_FAILED(node); return e.GetErrorCode();
	}

	return RPR_SUCCESS;
}

rpr_int rprSceneGetInfo(rpr_scene scene, rpr_scene_info info, size_t size, void* data, size_t* size_ret)
{
	s_logger.printTrace("//"); //  ignore getInfo for the trace
	TRACE__FUNCTION_OPEN;
	s_logger.Trace__FunctionClose();

	API_PRINT("rprSceneGetInfo", 1, scene);
	FrNode* node = static_cast<FrNode*>(scene);

	try
	{
		CHECK_NOT_NULL(node);
		MACRO__CHECK_ARGUMENT_TYPE(scene,Scene);


		size_t requiredSize = 0;
		switch (info)
		{
		case RPR_SCENE_SHAPE_COUNT:
		case RPR_SCENE_LIGHT_COUNT:
		case RPR_SCENE_HETEROVOLUME_COUNT:
			requiredSize = sizeof(size_t);
			break;

		case RPR_SCENE_SHAPE_LIST:
			requiredSize = sizeof(rpr_shape) * node->GetProperty<FrShapeList>(RPR_SCENE_SHAPE_LIST).size();
			break;

		case RPR_SCENE_LIGHT_LIST:
			requiredSize = sizeof(rpr_light) * node->GetProperty<FrLightList>(RPR_SCENE_LIGHT_LIST).size();
			break;

		case RPR_SCENE_HETEROVOLUME_LIST:
			requiredSize = sizeof(rpr_hetero_volume) * node->GetProperty<FrHeteroVolumeList>(RPR_SCENE_HETEROVOLUME_LIST).size();
			break;

		case RPR_OBJECT_NAME:
			{
				std::string name = node->GetProperty<std::string>(RPR_OBJECT_NAME);
				requiredSize = name.size() + 1;
				break;
			}
		case RPR_SCENE_AABB:
		{
			requiredSize = sizeof(fr_float) * 6;
			break;
		}

		default:
			throw FrException(__FILE__,__LINE__,RPR_ERROR_INVALID_PARAMETER, "Invalid scene info requested",scene);
		}

		if (data)
		{
			if (requiredSize <= size)
			{
				switch (info)
				{
				case RPR_SCENE_SHAPE_COUNT:
					*reinterpret_cast<size_t*>(data) = node->GetProperty<FrShapeList>(RPR_SCENE_SHAPE_LIST).size();
					break;

				case RPR_SCENE_LIGHT_COUNT:
					*reinterpret_cast<size_t*>(data) = node->GetProperty<FrLightList>(RPR_SCENE_LIGHT_LIST).size();
					break;

				case RPR_SCENE_HETEROVOLUME_COUNT:
					*reinterpret_cast<size_t*>(data) = node->GetProperty<FrHeteroVolumeList>(RPR_SCENE_HETEROVOLUME_LIST).size();
					break;

				case RPR_SCENE_SHAPE_LIST:
				case RPR_SCENE_LIGHT_LIST:
				case RPR_SCENE_HETEROVOLUME_LIST:
				{
					auto& nodeList = node->GetProperty<FrShapeList>(info);
					FrNode** dest = reinterpret_cast<FrNode**>(data);
					for (auto& n : nodeList)
					{
						*dest++ = n;
					}
					break;
				}
				case RPR_OBJECT_NAME:
				{
					std::string name = node->GetProperty<std::string>(RPR_OBJECT_NAME);
					auto len = name.size();
					auto str = reinterpret_cast<rpr_char*>(data);
					std::copy(name.cbegin(), name.cend(), str);
					*(str + len) = 0;
					break;
				}
				case RPR_SCENE_AABB:
				{
					FrNode* ctx = node->GetProperty<FrNode*>(FR_NODE_CONTEXT);
					std::shared_ptr<FrRendererEncalps> renderer = ctx->GetProperty<  std::shared_ptr<FrRendererEncalps>  >(RPR_CONTEXT_ACTIVE_PLUGIN);
					renderer->m_FrRenderer->GetSceneAABB((fr_float*)data);
					break;
				}
				default:
				{
					throw FrException(__FILE__,__LINE__,RPR_ERROR_INVALID_PARAMETER, "",node);
					break;
				}
				}
			}
			else
			{
				throw FrException(__FILE__,__LINE__,RPR_ERROR_INVALID_PARAMETER, "",node);
			}
		}

		if (size_ret)
		{
			*size_ret = requiredSize;
		}
	}
	catch (FrException& e)
	{
		TRACE__FUNCTION_FAILED(node); return e.GetErrorCode();
	}

	return RPR_SUCCESS;
}


rpr_int rprSceneSetEnvironmentOverride(rpr_scene scene, rpr_environment_override env_override, rpr_light light)
{
    s_logger.printTrace("status = ");
    TRACE__FUNCTION_OPEN;
    s_logger.TraceArg__rpr_scene(scene); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_environment_override(env_override); s_logger.TraceArg__COMMA();
    s_logger.TraceArg__rpr_light(light);
    s_logger.Trace__FunctionClose();

    API_PRINT("rprSceneSetEnvironmentOverride", 1, scene);
    FrNode* node = static_cast<FrNode*>(scene);

    try
    {
		CHECK_NOT_NULL(node);
		MACRO__CHECK_ARGUMENT_TYPE(scene,Scene);
		MACRO__CHECK_ARGUMENT_TYPE_ALLLIGHTS(light);


        node->SetProperty(env_override, static_cast<FrNode*>(light));
        return RPR_SUCCESS;
    }
    catch (FrException& e)
    {
        TRACE__FUNCTION_FAILED(node); return e.GetErrorCode();
    }
}

rpr_int rprSceneGetEnvironmentOverride(rpr_scene scene, rpr_environment_override env_override, rpr_light* out_light)
{
	s_logger.printTrace("//"); //  ignore getInfo for the trace
    s_logger.printTrace("status = ");
    TRACE__FUNCTION_OPEN;
    s_logger.TraceArg__rpr_scene(scene); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_environment_override(env_override); s_logger.TraceArg__COMMA();
    s_logger.Trace__FunctionClose();

    API_PRINT("rprSceneGetEnvironmentOverride", 1, scene);
    FrNode* node = static_cast<FrNode*>(scene);

    try
    {
		CHECK_NOT_NULL(node);
		MACRO__CHECK_ARGUMENT_TYPE(scene,Scene);


        *out_light = static_cast<rpr_image>(node->GetProperty<FrNode*>(env_override));
        return RPR_SUCCESS;
    }
    catch (FrException& e)
    {
        TRACE__FUNCTION_FAILED(node); return e.GetErrorCode();
    }
}

rpr_int rprSceneSetBackgroundImage(rpr_scene scene, rpr_image image)
{
    s_logger.printTrace("status = ");
    TRACE__FUNCTION_OPEN;
    s_logger.TraceArg__rpr_scene(scene); s_logger.TraceArg__COMMA();
    s_logger.TraceArg__rpr_image(image);
    s_logger.Trace__FunctionClose();

    API_PRINT("rprSceneSetBackgroundImage", 1, scene);
    FrNode* node = static_cast<FrNode*>(scene);

    try
    {
		CHECK_NOT_NULL(node);
		MACRO__CHECK_ARGUMENT_TYPE(scene,Scene);
		MACRO__CHECK_ARGUMENT_TYPE(image,Image);


        node->SetProperty(RPR_SCENE_BACKGROUND_IMAGE, static_cast<FrNode*>(image));
        return RPR_SUCCESS;
    }
    catch (FrException& e)
    {
        TRACE__FUNCTION_FAILED(node); return e.GetErrorCode();
    }
}

rpr_int rprSceneGetBackgroundImage(rpr_scene scene, rpr_image* out_image)
{
	s_logger.printTrace("//"); //  ignore getInfo for the trace
    s_logger.printTrace("status = ");
    TRACE__FUNCTION_OPEN;
    s_logger.TraceArg__rpr_scene(scene); s_logger.TraceArg__COMMA();
    s_logger.TraceArg__STATUS();
    s_logger.Trace__FunctionClose();

    FrNode* node = static_cast<FrNode*>(scene);

    try
    {
		CHECK_NOT_NULL(node);
		MACRO__CHECK_ARGUMENT_TYPE(scene,Scene);


        *out_image = static_cast<rpr_image>(node->GetProperty<FrNode*>(RPR_SCENE_BACKGROUND_IMAGE));
        return RPR_SUCCESS;
    }
    catch (FrException& e)
    {
        TRACE__FUNCTION_FAILED(node); return e.GetErrorCode();
    }
}

rpr_int rprSceneSetCamera(rpr_scene scene, rpr_camera camera)
{
	s_logger.printTrace("status = ");
	TRACE__FUNCTION_OPEN;
	s_logger.TraceArg__rpr_scene(scene); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_camera(camera);
	s_logger.Trace__FunctionClose();

	API_PRINT("rprSceneSetCamera", 1, scene);
	FrNode* c = static_cast<FrNode*>(camera);
	FrNode* s = static_cast<FrNode*>(scene);

	try
	{
		CHECK_NOT_NULL(s);
		MACRO__CHECK_ARGUMENT_TYPE(scene,Scene);
		MACRO__CHECK_ARGUMENT_TYPE(camera,Camera);


		s->SetProperty(RPR_SCENE_CAMERA, static_cast<FrNode*>(c));
		return RPR_SUCCESS;
	}
	catch (FrException& e)
	{
		TRACE__FUNCTION_FAILED(c); return e.GetErrorCode();
	}
}


rpr_int rprSceneGetCamera(rpr_scene scene, rpr_camera* out_camera)
{
	s_logger.printTrace("//"); //  ignore getInfo for the trace
	TRACE__FUNCTION_OPEN;
	s_logger.Trace__FunctionClose();

	API_PRINT("rprSceneGetCamera", 1, scene);
	FrNode* node = static_cast<FrNode*>(scene);

	try
	{
		CHECK_NOT_NULL(node);
		MACRO__CHECK_ARGUMENT_TYPE(scene,Scene);


		*out_camera = static_cast<rpr_camera>(node->GetProperty<FrNode*>(RPR_SCENE_CAMERA));
		return RPR_SUCCESS;
	}
	catch (FrException& e)
	{
		TRACE__FUNCTION_FAILED(node); return e.GetErrorCode();
	}
}

rpr_int rprFrameBufferGetInfo(rpr_framebuffer framebuffer, rpr_framebuffer_info info, size_t size, void* data, size_t* size_ret)
{
	s_logger.printTrace("//"); //  ignore getInfo for the trace
	TRACE__FUNCTION_OPEN;
	s_logger.Trace__FunctionClose();

	API_PRINT("rprFrameBufferGetInfo", 1, framebuffer);
	FrNode* node = static_cast<FrNode*>(framebuffer);
	
	try
	{
		CHECK_NOT_NULL(node);
		MACRO__CHECK_ARGUMENT_TYPE(framebuffer,FrameBuffer);

		FrNode* ctx = node->GetProperty<FrNode*>(FR_NODE_CONTEXT);

		size_t requiredSize = 0;
		switch (info)
		{
		case RPR_FRAMEBUFFER_FORMAT:
		case RPR_FRAMEBUFFER_DESC:
			requiredSize = node->GetPropertyTypeSize(info);
			break;

		case RPR_FRAMEBUFFER_DATA:
		{
			rpr_framebuffer_format& format = node->GetProperty<rpr_framebuffer_format>(RPR_FRAMEBUFFER_FORMAT);
			rpr_framebuffer_desc& desc = node->GetProperty<rpr_framebuffer_desc>(RPR_FRAMEBUFFER_DESC);
			requiredSize = desc.fb_width * desc.fb_height * GetFormatElementSize(format.type, format.num_components);
			break;
		}

		case RPR_OBJECT_NAME:
		{
			std::string name = node->GetProperty<std::string>(RPR_OBJECT_NAME);
			requiredSize = name.size() + 1;
			break;
		}

		case RPR_CL_MEM_OBJECT:
		{
			std::shared_ptr<FrRendererEncalps> renderer = ctx->GetProperty<  std::shared_ptr<FrRendererEncalps>  >(RPR_CONTEXT_ACTIVE_PLUGIN);
			requiredSize = renderer->m_FrRenderer->GetInfoSize(node, RPR_CL_MEM_OBJECT);
			break;
		}

		default:
			throw FrException(__FILE__,__LINE__,RPR_ERROR_INVALID_PARAMETER, "Invalid framebuffer info requested",framebuffer);
		}

		if (data)
		{
			if (requiredSize <= size)
			{
				switch (info)
				{
				case RPR_FRAMEBUFFER_FORMAT:
					*reinterpret_cast<rpr_framebuffer_format*>(data) = node->GetProperty<rpr_framebuffer_format>(RPR_FRAMEBUFFER_FORMAT);
					break;

				case RPR_FRAMEBUFFER_DESC:
					*reinterpret_cast<rpr_framebuffer_desc*>(data) = node->GetProperty<rpr_framebuffer_desc>(RPR_FRAMEBUFFER_DESC);
					break;

				case RPR_FRAMEBUFFER_DATA:
				{
					rpr_framebuffer_format& format = node->GetProperty<rpr_framebuffer_format>(RPR_FRAMEBUFFER_FORMAT);
					rpr_framebuffer_desc& desc = node->GetProperty<rpr_framebuffer_desc>(RPR_FRAMEBUFFER_DESC);


					if (format.type == RPR_COMPONENT_TYPE_FLOAT32 || format.type == RPR_COMPONENT_TYPE_FLOAT16)
					{
						FrNode* ctx = node->GetProperty<FrNode*>(FR_NODE_CONTEXT);
						std::shared_ptr<FrRendererEncalps> renderer = ctx->GetProperty<  std::shared_ptr<FrRendererEncalps>  >(RPR_CONTEXT_ACTIVE_PLUGIN);
						renderer->m_FrRenderer->FrameBufferGetData(node, data);
					}
					else
					{
						throw FrException(__FILE__,__LINE__,RPR_ERROR_UNSUPPORTED_IMAGE_FORMAT, "TahoeFrameBuffer: Unsupported framebuffer format",framebuffer);
					}

					break;
				}

				case RPR_OBJECT_NAME:
				{
					std::string name = node->GetProperty<std::string>(RPR_OBJECT_NAME);
					auto len = name.size();
					auto str = reinterpret_cast<rpr_char*>(data);
					std::copy(name.cbegin(), name.cend(), str);
					*(str + len) = 0;
					break;
				}

				case RPR_CL_MEM_OBJECT:
				{
					std::shared_ptr<FrRendererEncalps> renderer = ctx->GetProperty<  std::shared_ptr<FrRendererEncalps>  >(RPR_CONTEXT_ACTIVE_PLUGIN);
					renderer->m_FrRenderer->GetInfo(node, RPR_CL_MEM_OBJECT, data);
					break;
				}

				default:
					throw FrException(__FILE__,__LINE__,RPR_ERROR_INVALID_PARAMETER, "Invalid framebuffer info requested",framebuffer);
				}
			}
			else
			{
				throw FrException(__FILE__,__LINE__,RPR_ERROR_INVALID_PARAMETER, "",ctx);
			}
		}

		if (size_ret)
			*size_ret = requiredSize;
	}
	catch (FrException& e)
	{
		TRACE__FUNCTION_FAILED(node); return e.GetErrorCode();
	}

	return RPR_SUCCESS;
}

rpr_int rprShapeSetTransform(rpr_shape shape, rpr_bool transpose, const rpr_float* transform)
{
	s_logger.TraceArg_Prepare__rpr_float_P16(transform);
	s_logger.printTrace("status = ");
	TRACE__FUNCTION_OPEN;
	s_logger.TraceArg__rpr_shape(shape); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_bool(transpose); s_logger.TraceArg__COMMA();
	s_logger.TraceArg_Use__rpr_float_P16(transform);
	s_logger.Trace__FunctionClose();

	API_PRINT("rprShapeSetTransform", 1, shape);
	FrNode* node = static_cast<FrNode*>(shape);

	try
	{
		CHECK_NOT_NULL(node);
		MACRO__CHECK_ARGUMENT_TYPE_ALLSHAPES(shape);

		for(int i=0; i<16; i++)
		{
			//if we give NAN here, we may have a TDR during rendering
			MACRO__CHECK_ARGUMENT_FLOAT_NAN(transform[i],shape);
			MACRO__CHECK_ARGUMENT_FLOAT_INF(transform[i],shape);
		}

		matrix m(transform[0], transform[1], transform[2], transform[3],
			transform[4], transform[5], transform[6], transform[7],
			transform[8], transform[9], transform[10], transform[11],
			transform[12], transform[13], transform[14], transform[15]
		);

		#ifdef RPR_FOR_BAIKAL
		if (!transpose)
		#else
		if (transpose)
		#endif
			m = m.transpose();

		node->SetProperty(RPR_SHAPE_TRANSFORM, m);

		return RPR_SUCCESS;
	}
	catch (FrException& e)
	{
		TRACE__FUNCTION_FAILED(node); return e.GetErrorCode();
	}
}

rpr_int rprShapeSetSubdivisionFactor(rpr_shape shape, rpr_uint factor)
{
	s_logger.printTrace("status = ");
	TRACE__FUNCTION_OPEN;
	s_logger.TraceArg__rpr_shape(shape); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_uint(factor);
	s_logger.Trace__FunctionClose();

	API_PRINT("rprShapeSetSubdivisionFactor", 1, shape);
	FrNode* node = static_cast<FrNode*>(shape);

	try
	{
		CHECK_NOT_NULL(node);
		MACRO__CHECK_ARGUMENT_TYPE_ALLSHAPES(shape);


		node->SetProperty(RPR_SHAPE_SUBDIVISION_FACTOR, factor);
		return RPR_SUCCESS;
	}
	catch (FrException& e)
	{
		TRACE__FUNCTION_FAILED(node); return e.GetErrorCode();
	}
}

rpr_int rprShapeSetSubdivisionCreaseWeight(rpr_shape shape, rpr_float factor)
{
	s_logger.printTrace("status = ");
	TRACE__FUNCTION_OPEN;
	s_logger.TraceArg__rpr_shape(shape); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_float(factor);
	s_logger.Trace__FunctionClose();

	API_PRINT("rprShapeSetSubdivisionCreaseWeight", 1, shape);
	FrNode* node = static_cast<FrNode*>(shape);

	try
	{
		CHECK_NOT_NULL(node);
		MACRO__CHECK_ARGUMENT_TYPE_ALLSHAPES(shape);


		node->SetProperty(RPR_SHAPE_SUBDIVISION_CREASEWEIGHT, factor);
		return RPR_SUCCESS;
	}
	catch (FrException& e)
	{
		TRACE__FUNCTION_FAILED(node); return e.GetErrorCode();
	}
}

rpr_int rprShapeSetSubdivisionBoundaryInterop(rpr_shape shape, rpr_subdiv_boundary_interfop_type type)
{
	s_logger.printTrace("status = ");
	TRACE__FUNCTION_OPEN;
	s_logger.TraceArg__rpr_shape(shape); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_subdiv_boundary_interfop_type(type);
	s_logger.Trace__FunctionClose();

	API_PRINT("rprShapeSetSubdivisionBoundaryInterop", 1, shape);
	FrNode* node = static_cast<FrNode*>(shape);

	try
	{
		CHECK_NOT_NULL(node);
		MACRO__CHECK_ARGUMENT_TYPE_ALLSHAPES(shape);


		node->SetProperty(RPR_SHAPE_SUBDIVISION_BOUNDARYINTEROP, type);
		return RPR_SUCCESS;
	}
	catch (FrException& e)
	{
		TRACE__FUNCTION_FAILED(node); return e.GetErrorCode();
	}
}



rpr_int rprShapeAutoAdaptSubdivisionFactor(rpr_shape shape, rpr_framebuffer framebuffer, rpr_camera camera, rpr_int factor)
{
	s_logger.printTrace("status = ");
	TRACE__FUNCTION_OPEN
	s_logger.TraceArg__rpr_shape(shape); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_framebuffer(framebuffer); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_camera(camera); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_int(factor);
	s_logger.Trace__FunctionClose();

	API_PRINT("rprShapeAutoAdaptSubdivisionFactor",0);

	try
	{
		CHECK_NOT_NULL(static_cast<FrNode*>(shape));
		CHECK_NOT_NULL(static_cast<FrNode*>(framebuffer));
		CHECK_NOT_NULL(static_cast<FrNode*>(camera));

		MACRO__CHECK_ARGUMENT_TYPE_ALLSHAPES(shape);
		MACRO__CHECK_ARGUMENT_TYPE(framebuffer,FrameBuffer);
		MACRO__CHECK_ARGUMENT_TYPE(camera,Camera);

		if ( !shape || !framebuffer || !camera )
			throw FrException(__FILE__,__LINE__,RPR_ERROR_INVALID_PARAMETER, "null ptr",shape);

		FrNode* shapeRPR = static_cast<FrNode*>(shape);
		FrNode* shapeRPR_ctx = shapeRPR->GetProperty<FrNode*>(FR_NODE_CONTEXT);

		FrNode* framebufferRPR = static_cast<FrNode*>(framebuffer);
		FrNode* framebufferRPR_ctx = framebufferRPR->GetProperty<FrNode*>(FR_NODE_CONTEXT);

		FrNode* cameraRPR = static_cast<FrNode*>(camera);
		FrNode* cameraRPR_ctx = cameraRPR->GetProperty<FrNode*>(FR_NODE_CONTEXT);

		//check all elements are in same context
		if (   !shapeRPR_ctx 
			||  shapeRPR_ctx != framebufferRPR_ctx
			||  shapeRPR_ctx != cameraRPR_ctx)
			throw FrException(__FILE__,__LINE__,RPR_ERROR_INVALID_PARAMETER, "different context ptr",shape);

		std::shared_ptr<FrRendererEncalps> renderer = shapeRPR_ctx->GetProperty< std::shared_ptr<FrRendererEncalps> >(RPR_CONTEXT_ACTIVE_PLUGIN);
		
		if (!renderer->m_FrRenderer) 
			throw FrException(__FILE__,__LINE__,RPR_ERROR_INVALID_PARAMETER, "No active compute API set",shapeRPR_ctx);

		renderer->m_FrRenderer->AdaptSubdiv(shapeRPR , cameraRPR, framebufferRPR, factor);
		return RPR_SUCCESS;
	}
	catch (FrException& e)
	{
		TRACE__FUNCTION_FAILED(0); return e.GetErrorCode();
	}
}




rpr_int rprImageSetWrap(rpr_image image, rpr_image_wrap_type type)
{
	s_logger.printTrace("status = ");
	TRACE__FUNCTION_OPEN;
	s_logger.TraceArg__rpr_image(image); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_image_wrap_type(type);
	s_logger.Trace__FunctionClose();

	API_PRINT("rprImageSetWrap", 1, image);
	FrNode* node = static_cast<FrNode*>(image);

	try
	{
		CHECK_NOT_NULL(node);
		MACRO__CHECK_ARGUMENT_TYPE(image,Image);


		node->SetProperty(RPR_IMAGE_WRAP, type);
		return RPR_SUCCESS;
	}
	catch (FrException& e)
	{
		TRACE__FUNCTION_FAILED(node); return e.GetErrorCode();
	}
}

rpr_int rprImageSetFilter(rpr_image image, rpr_image_filter_type type)
{
	s_logger.printTrace("status = ");
	TRACE__FUNCTION_OPEN;
	s_logger.TraceArg__rpr_image(image); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_image_filter_type(type);
	s_logger.Trace__FunctionClose();

	API_PRINT("rprImageSetFilter", 1, image);
	FrNode* node = static_cast<FrNode*>(image);

	try
	{
		CHECK_NOT_NULL(node);
		MACRO__CHECK_ARGUMENT_TYPE(image,Image);


		node->SetProperty(RPR_IMAGE_FILTER, type);
		return RPR_SUCCESS;
	}
	catch (FrException& e)
	{
		TRACE__FUNCTION_FAILED(node); return e.GetErrorCode();
	}
}

rpr_int rprImageSetGamma(rpr_image image, rpr_float gamma)
{
	s_logger.printTrace("status = ");
	TRACE__FUNCTION_OPEN;
	s_logger.TraceArg__rpr_image(image); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_float(gamma);
	s_logger.Trace__FunctionClose();

	API_PRINT("rprImageSetGamma", 1, image);
	FrNode* node = static_cast<FrNode*>(image);

	try
	{
		CHECK_NOT_NULL(node);
		MACRO__CHECK_ARGUMENT_TYPE(image,Image);

		node->SetProperty(RPR_IMAGE_GAMMA, gamma);
		return RPR_SUCCESS;
	}
	catch (FrException& e)
	{
		TRACE__FUNCTION_FAILED(node); return e.GetErrorCode();
	}
}

rpr_int rprImageSetMipmapEnabled(rpr_image image, rpr_bool enabled)
{
	s_logger.printTrace("status = ");
	TRACE__FUNCTION_OPEN;
	s_logger.TraceArg__rpr_image(image); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_bool(enabled);
	s_logger.Trace__FunctionClose();

	API_PRINT("rprImageSetMipmapEnabled", 1, image);
	FrNode* node = static_cast<FrNode*>(image);

	try
	{
		CHECK_NOT_NULL(node);
		MACRO__CHECK_ARGUMENT_TYPE(image,Image);

		node->SetProperty(RPR_IMAGE_MIPMAP_ENABLED, enabled);
		return RPR_SUCCESS;
	}
	catch (FrException& e)
	{
		TRACE__FUNCTION_FAILED(node); return e.GetErrorCode();
	}
}


rpr_int rprShapeSetDisplacementScale(rpr_shape shape, rpr_float minscale, rpr_float maxscale)
{
	s_logger.printTrace("status = ");
	TRACE__FUNCTION_OPEN;
	s_logger.TraceArg__rpr_shape(shape); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_float(minscale); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_float(maxscale);
	s_logger.Trace__FunctionClose();

	API_PRINT("rprShapeSetDisplacementScale", 1, shape);
	FrNode* node = static_cast<FrNode*>(shape);

	try
	{
		CHECK_NOT_NULL(node);
		MACRO__CHECK_ARGUMENT_TYPE_ALLSHAPES(shape);


		node->SetProperty(RPR_SHAPE_DISPLACEMENT_SCALE, rpr_float2{ minscale, maxscale });
		return RPR_SUCCESS;
	}
	catch (FrException& e)
	{
		TRACE__FUNCTION_FAILED(node); return e.GetErrorCode();
	}
}

rpr_int rprShapeSetObjectGroupID(rpr_shape shape, rpr_uint objectGroupID )
{
	s_logger.printTrace("status = ");
	TRACE__FUNCTION_OPEN;
	s_logger.TraceArg__rpr_shape(shape); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_uint(objectGroupID); 
	s_logger.Trace__FunctionClose();

	API_PRINT("rprShapeSetObjectGroupID", 1, shape);
	FrNode* node = static_cast<FrNode*>(shape);

	try
	{
		CHECK_NOT_NULL(node);
		MACRO__CHECK_ARGUMENT_TYPE_ALLSHAPES(shape);


		node->SetProperty(RPR_SHAPE_OBJECT_GROUP_ID, objectGroupID);
		return RPR_SUCCESS;
	}
	catch (FrException& e)
	{
		TRACE__FUNCTION_FAILED(node); return e.GetErrorCode();
	}
}

rpr_int rprShapeSetLayerMask(rpr_shape shape, rpr_uint layerID )
{
	s_logger.printTrace("status = ");
	TRACE__FUNCTION_OPEN;
	s_logger.TraceArg__rpr_shape(shape); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_uint(layerID); 
	s_logger.Trace__FunctionClose();

	API_PRINT("rprShapeSetLayerMask", 1, shape);
	FrNode* node = static_cast<FrNode*>(shape);

	try
	{
		CHECK_NOT_NULL(node);
		MACRO__CHECK_ARGUMENT_TYPE_ALLSHAPES(shape);

		node->SetProperty(RPR_SHAPE_LAYER_MASK, layerID);
		return RPR_SUCCESS;
	}
	catch (FrException& e)
	{
		TRACE__FUNCTION_FAILED(node); return e.GetErrorCode();
	}
}

rpr_int rprShapeSetDisplacementMaterial(rpr_shape shape, rpr_material_node material)
{
	s_logger.printTrace("status = ");
	TRACE__FUNCTION_OPEN;
	s_logger.TraceArg__rpr_shape(shape); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_material_node(material);
	s_logger.Trace__FunctionClose();

	API_PRINT("rprShapeSetDisplacementMaterial", 1, shape);
	FrNode* node = static_cast<FrNode*>(shape);

	try
	{
		CHECK_NOT_NULL(node);
		MACRO__CHECK_ARGUMENT_TYPE_ALLSHAPES(shape);
		MACRO__CHECK_ARGUMENT_TYPE(material,Material);

		node->SetProperty(RPR_SHAPE_DISPLACEMENT_MATERIAL, static_cast<FrNode*>(material));

		return RPR_SUCCESS;
	}
	catch (FrException& e)
	{
		TRACE__FUNCTION_FAILED(node); return e.GetErrorCode();
	}
}


rpr_int rprShapeSetVisibility(rpr_shape shape, rpr_bool visible)
{
	s_logger.printTrace("status = ");
	TRACE__FUNCTION_OPEN;
	s_logger.TraceArg__rpr_shape(shape); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_bool(visible);
	s_logger.Trace__FunctionClose();

	API_PRINT("rprShapeSetVisibilityFlag", 1, shape);
	FrNode* node = static_cast<FrNode*>(shape);

	try
	{
		CHECK_NOT_NULL(node);
		MACRO__CHECK_ARGUMENT_TYPE_ALLSHAPES(shape);


		//internally, setting  RPR_SHAPE_VISIBILITY_FLAG   will set all visibility flags :
		// IN_SPECULAR_FLAG, PRIMARY_ONLY_FLAG + all the internal flags not exposed.
		//so we adjust flags here, just in order to have correct getters.
		node->SetProperty(RPR_SHAPE_VISIBILITY_IN_SPECULAR_FLAG,visible);
		node->SetProperty(RPR_SHAPE_VISIBILITY_PRIMARY_ONLY_FLAG,visible);


		node->SetProperty(RPR_SHAPE_VISIBILITY_FLAG, visible);
		return RPR_SUCCESS;
	}
	catch (FrException& e)
	{
		TRACE__FUNCTION_FAILED(node); return e.GetErrorCode();
	}

}

////////////////////////////////////////////////////////////////////////////////////////
RPR_API_ENTRY rpr_int rprShapeSetVisibilityPrimaryOnly(rpr_shape shape, rpr_bool visible)
{
	s_logger.printTrace("status = ");
	TRACE__FUNCTION_OPEN
	s_logger.TraceArg__rpr_shape(shape); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_bool(visible);
	s_logger.Trace__FunctionClose();

	API_PRINT("rprShapeSetVisibilityPrimaryOnly", 1, shape);
	FrNode* node = static_cast<FrNode*>(shape);

	try
	{
		CHECK_NOT_NULL(node);
		MACRO__CHECK_ARGUMENT_TYPE_ALLSHAPES(shape);


		node->SetProperty(RPR_SHAPE_VISIBILITY_PRIMARY_ONLY_FLAG, visible);
		return RPR_SUCCESS;
	}
	catch (FrException& e)
	{
		TRACE__FUNCTION_FAILED(node); return e.GetErrorCode();
	}

}

////////////////////////////////////////////////////////////////////////////////////////
RPR_API_ENTRY rpr_int rprShapeSetVisibilityInSpecular(rpr_shape shape, rpr_bool visible)
{
	s_logger.printTrace("status = ");
	TRACE__FUNCTION_OPEN
	s_logger.TraceArg__rpr_shape(shape); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_bool(visible);
	s_logger.Trace__FunctionClose();

	API_PRINT("rprShapeSetVisibilityInSpecular", 1, shape);
	FrNode* node = static_cast<FrNode*>(shape);

	try
	{
		CHECK_NOT_NULL(node);
		MACRO__CHECK_ARGUMENT_TYPE_ALLSHAPES(shape);


		node->SetProperty(RPR_SHAPE_VISIBILITY_IN_SPECULAR_FLAG, visible);
		return RPR_SUCCESS;
	}
	catch (FrException& e)
	{
		TRACE__FUNCTION_FAILED(node); return e.GetErrorCode();
	}

}


////////////////////////////////////////////////////////////////////////////////////////
RPR_API_ENTRY rpr_int rprShapeSetShadowCatcher(rpr_shape shape, rpr_bool shadowcatcher)
{
	s_logger.printTrace("status = ");
	TRACE__FUNCTION_OPEN
	s_logger.TraceArg__rpr_shape(shape); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_bool(shadowcatcher);
	s_logger.Trace__FunctionClose();

	API_PRINT("rprShapeSetShadowCatcher", 1, shape);
	FrNode* node = static_cast<FrNode*>(shape);

	try
	{
		CHECK_NOT_NULL(node);
		MACRO__CHECK_ARGUMENT_TYPE_ALLSHAPES(shape);


		node->SetProperty(RPR_SHAPE_SHADOW_CATCHER_FLAG, shadowcatcher);
		return RPR_SUCCESS;
	}
	catch (FrException& e)
	{
		TRACE__FUNCTION_FAILED(node); return e.GetErrorCode();
	}
}


rpr_int rprShapeSetShadow(rpr_shape shape, rpr_bool casts_shadow)
{
	s_logger.printTrace("status = ");
	TRACE__FUNCTION_OPEN;
	s_logger.TraceArg__rpr_shape(shape); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_bool(casts_shadow);
	s_logger.Trace__FunctionClose();

	API_PRINT("rprShapeSetShadowFlag", 1, shape);
	FrNode* node = static_cast<FrNode*>(shape);

	try
	{
		CHECK_NOT_NULL(node);
		MACRO__CHECK_ARGUMENT_TYPE_ALLSHAPES(shape);


		node->SetProperty(RPR_SHAPE_SHADOW_FLAG, casts_shadow);
		return RPR_SUCCESS;
	}
	catch (FrException& e)
	{
		TRACE__FUNCTION_FAILED(node); return e.GetErrorCode();
	}
}

rpr_int rprLightSetTransform(rpr_light light, rpr_bool transpose, const rpr_float* transform)
{
	s_logger.TraceArg_Prepare__rpr_float_P16(transform);
	s_logger.printTrace("status = ");
	TRACE__FUNCTION_OPEN;
	s_logger.TraceArg__rpr_light(light); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_bool(transpose); s_logger.TraceArg__COMMA();
	s_logger.TraceArg_Use__rpr_float_P16(transform);
	s_logger.Trace__FunctionClose();

	API_PRINT("rprLightSetTransform", 1, light);
	FrNode* node = static_cast<FrNode*>(light);

	try
	{
		CHECK_NOT_NULL(node);
		MACRO__CHECK_ARGUMENT_TYPE_ALLLIGHTS(light);


		matrix m(transform[0], transform[1], transform[2], transform[3],
			transform[4], transform[5], transform[6], transform[7],
			transform[8], transform[9], transform[10], transform[11],
			transform[12], transform[13], transform[14], transform[15]
		);

		#ifdef RPR_FOR_BAIKAL
		if (!transpose)
		#else
		if (transpose)
		#endif
			m = m.transpose();

		node->SetProperty(RPR_LIGHT_TRANSFORM, m);

		return RPR_SUCCESS;
	}
	catch (FrException& e)
	{
		TRACE__FUNCTION_FAILED(node); return e.GetErrorCode();
	}
}

rpr_int rprLightSetGroupId(rpr_light light,  rpr_uint groupId)
{
	
	s_logger.printTrace("status = ");
	TRACE__FUNCTION_OPEN;
	s_logger.TraceArg__rpr_light(light); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_uint(groupId);
	s_logger.Trace__FunctionClose();

	API_PRINT("rprLightSetTransform", 1, light);
	FrNode* node = static_cast<FrNode*>(light);

	try
	{
		if ( groupId >= 4 && groupId != (rpr_uint)-1	)
			throw FrException(__FILE__,__LINE__,RPR_ERROR_INVALID_OBJECT, "invalid groupId", node );

		MACRO__CHECK_ARGUMENT_TYPE_ALLLIGHTS(light);
		node->SetProperty(RPR_LIGHT_GROUP_ID, groupId);
		return RPR_SUCCESS;
	}
	catch (FrException& e)
	{
		TRACE__FUNCTION_FAILED(node); return e.GetErrorCode();
	}
}

rpr_int rprFrameBufferSaveToFile(rpr_framebuffer frame_buffer, rpr_char const* file_path)
{
	s_logger.printTrace("status = ");
	TRACE__FUNCTION_OPEN;
	s_logger.TraceArg__rpr_framebuffer(frame_buffer); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_char_P(file_path);
	s_logger.Trace__FunctionClose();

	API_PRINT("rprFrameBufferSaveToFile", 1, frame_buffer);
	FrNode* node = static_cast<FrNode*>(frame_buffer);
	
	try
	{
		CHECK_NOT_NULL(node);
		MACRO__CHECK_ARGUMENT_TYPE(frame_buffer,FrameBuffer);

		FrNode* ctx = node->GetProperty<FrNode*>(FR_NODE_CONTEXT);

		std::shared_ptr<FrRendererEncalps> renderer = ctx->GetProperty<std::shared_ptr<FrRendererEncalps>>(RPR_CONTEXT_ACTIVE_PLUGIN);
		if (!renderer->m_FrRenderer)
			throw FrException(__FILE__,__LINE__,RPR_ERROR_INVALID_OBJECT, "No active compute API set",frame_buffer);

		renderer->m_FrRenderer->FrameBufferSaveToFile(node, file_path);
	}
	catch (FrException& e)
	{
		TRACE__FUNCTION_FAILED(node); return e.GetErrorCode();
	}

	return RPR_SUCCESS;
}

rpr_int rprFrameBufferClear(rpr_framebuffer frame_buffer)
{
	s_logger.printTrace("status = ");
	TRACE__FUNCTION_OPEN;
	s_logger.TraceArg__rpr_framebuffer(frame_buffer);
	s_logger.Trace__FunctionClose();

	API_PRINT("rprFrameBufferClear", 1, frame_buffer);
	FrNode* node = static_cast<FrNode*>(frame_buffer);
	
	try
	{
		CHECK_NOT_NULL(node);
		MACRO__CHECK_ARGUMENT_TYPE(frame_buffer,FrameBuffer);

		FrNode* ctx = node->GetProperty<FrNode*>(FR_NODE_CONTEXT);

		std::shared_ptr<FrRendererEncalps> renderer = ctx->GetProperty<  std::shared_ptr<FrRendererEncalps>   >(RPR_CONTEXT_ACTIVE_PLUGIN);
		if (!renderer->m_FrRenderer)
			throw FrException(__FILE__,__LINE__,RPR_ERROR_INVALID_OBJECT, "No active compute API set",frame_buffer);

		renderer->m_FrRenderer->FrameBufferClear(node);
	}
	catch (FrException& e)
	{
		TRACE__FUNCTION_FAILED(node); return e.GetErrorCode();
	}

	return RPR_SUCCESS;
}

rpr_int rprSceneClear(rpr_scene scene)
{
	s_logger.printTrace("status = ");
	TRACE__FUNCTION_OPEN;
	s_logger.TraceArg__rpr_scene(scene);
	s_logger.Trace__FunctionClose();

	API_PRINT("rprSceneClear", 1, scene);
	FrNode* node = static_cast<FrNode*>(scene);

	try
	{
		CHECK_NOT_NULL(node);
		MACRO__CHECK_ARGUMENT_TYPE(scene,Scene);


		auto shapes = node->GetProperty<std::set<FrNode*>>(RPR_SCENE_SHAPE_LIST);
		auto lights = node->GetProperty<std::set<FrNode*>>(RPR_SCENE_LIGHT_LIST);
		auto heterovolumes = node->GetProperty<std::set<FrNode*>>(RPR_SCENE_HETEROVOLUME_LIST);

		for (auto& s : shapes)
		{
			s_logger.PauseTracing(); // pause tracing, because calling exposed traced API function internally
			rprSceneDetachShape(scene, s);
			s_logger.ContinueTracing();
		}

		for (auto& l : lights)
		{
			s_logger.PauseTracing(); // pause tracing, because calling exposed traced API function internally
			rprSceneDetachLight(scene, l);
			s_logger.ContinueTracing();
		}

		for (auto& h : heterovolumes)
		{
			s_logger.PauseTracing(); // pause tracing, because calling exposed traced API function internally
			rprSceneDetachHeteroVolume(scene, h);
			s_logger.ContinueTracing();
		}

		s_logger.PauseTracing(); // pause tracing, because calling exposed traced API function internally
		rprSceneSetCamera(scene,0);
		s_logger.ContinueTracing();
	}
	catch (FrException& e)
	{
		TRACE__FUNCTION_FAILED(node); return e.GetErrorCode();
	}

	return RPR_SUCCESS;
}

rpr_int rprContextCreateFramebufferFromGLTexture2D(rpr_context context, rpr_GLenum target, rpr_GLint miplevel, rpr_GLuint texture, rpr_framebuffer* out_fb)
{
	//in case of issues during creation, it's safer to return a null pointer, rather than let the pointer undefined.
	if ( out_fb )
		*out_fb = nullptr;
	

	s_logger.printTrace("//FramebufferFromGLTexture2D creation\r\n");
	s_logger.Trace__FlushAllFiles(); // in case a crash, ensure trace informs the creation


	API_PRINT("rprContextCreateFramebufferFromGLTexture2D", 1, context);
	FrNode* ctx = static_cast<FrNode*>(context);

	try
	{
		CHECK_NOT_NULL(ctx);
		MACRO__CHECK_ARGUMENT_TYPE(context,Context);


		std::shared_ptr<FrSceneGraph> sceneGraph = ctx->GetProperty< std::shared_ptr<FrSceneGraph> >(FR_SCENEGRAPH);
		FrNode* node = sceneGraph->CreateNode(NodeTypes::FrameBuffer, [&](FrNode* node) {
			sceneGraph->DisableCallbacks();
			node->SetProperty(FR_NODE_CONTEXT, ctx);
			node->SetProperty(RPR_FRAMEBUFFER_GL_TARGET, target);
			node->SetProperty(RPR_FRAMEBUFFER_GL_MIPLEVEL, miplevel);
			node->SetProperty(RPR_FRAMEBUFFER_GL_TEXTURE, texture);
            node->AddProperty(FR_SCENEGRAPH, sceneGraph);
			sceneGraph->EnableCallbacks();
		});

		*out_fb = node;

		s_logger.Trace__NewFrObjectCreated(Logger::TRACE_ADDRESS_TYPE__FRAMEBUFFER, *out_fb);
		s_logger.Trace__DeclareFRobject("rpr_framebuffer", "framebuffer_0x", *out_fb);
		s_logger.printTrace("status = ");
		TRACE__FUNCTION_OPEN
		s_logger.TraceArg__rpr_context(context); s_logger.TraceArg__COMMA();
		s_logger.TraceArg__uint(target); s_logger.TraceArg__COMMA();
		s_logger.TraceArg__int(miplevel); s_logger.TraceArg__COMMA();
		s_logger.TraceArg__uint(texture); s_logger.TraceArg__COMMA();
		s_logger.printTrace("&framebuffer_0x%p", *out_fb);
		s_logger.Trace__FunctionClose();
		s_logger.printTrace("status = rprObjectSetName (framebuffer_0x%p,\"framebuffer_0x%p\"); RPRTRACE_CHECK// added by tracing for debug\r\n",  *out_fb ,  *out_fb);

		return RPR_SUCCESS;
	}
	catch (FrException& e)
	{
		TRACE__FUNCTION_FAILED(context); return e.GetErrorCode();
	}
}

rpr_int rprContextResolveFrameBuffer(rpr_context context, rpr_framebuffer src_frame_buffer, rpr_framebuffer dst_frame_buffer, rpr_bool normalizeOnly)
{
	s_logger.printTrace("status = ");
	TRACE__FUNCTION_OPEN;
	s_logger.TraceArg__rpr_context(context); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_framebuffer(src_frame_buffer); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_framebuffer(dst_frame_buffer);
	s_logger.Trace__FunctionClose();

	API_PRINT("rprContextResolveFrameBuffer", 1, context);
	FrNode* ctx = static_cast<FrNode*>(context);

	try
	{
		CHECK_NOT_NULL(ctx);
		CHECK_NOT_NULL(static_cast<FrNode*>(src_frame_buffer));
		CHECK_NOT_NULL(static_cast<FrNode*>(dst_frame_buffer));

		MACRO__CHECK_ARGUMENT_TYPE(context,Context);
		MACRO__CHECK_ARGUMENT_TYPE(src_frame_buffer,FrameBuffer);
		MACRO__CHECK_ARGUMENT_TYPE(dst_frame_buffer,FrameBuffer);


		std::shared_ptr<FrRendererEncalps> renderer = ctx->GetProperty<  std::shared_ptr<FrRendererEncalps>  >(RPR_CONTEXT_ACTIVE_PLUGIN);
		if (!renderer->m_FrRenderer)
			throw FrException(__FILE__,__LINE__,RPR_ERROR_INVALID_OBJECT, "No active compute API set",context);

		renderer->m_FrRenderer->ResolveFrameBuffer(static_cast<FrNode*>(src_frame_buffer), static_cast<FrNode*>(dst_frame_buffer), normalizeOnly);

		return RPR_SUCCESS;
	}
	catch (FrException& e)
	{
		TRACE__FUNCTION_FAILED(context); return e.GetErrorCode();
	}
}


extern RPR_API_ENTRY rpr_int rprContextCreateMaterialSystem(rpr_context in_context, rpr_material_system_type in_type, rpr_material_system* out_matsys)
{
	//in case of issues during creation, it's safer to return a null pointer, rather than let the pointer undefined.
	if ( out_matsys )
		*out_matsys = nullptr;
	

	s_logger.printTrace("//MaterialSystem creation\r\n");
	s_logger.Trace__FlushAllFiles(); // in case a crash, ensure trace informs the creation

	API_PRINT("rprContextCreateMaterialSystem", 1, in_context);

	FrNode* ctx = static_cast<FrNode*>(in_context);

	try
	{
		CHECK_NOT_NULL(ctx);
		MACRO__CHECK_ARGUMENT_TYPE(in_context,Context);


		std::shared_ptr<FrSceneGraph> sceneGraph = ctx->GetProperty< std::shared_ptr<FrSceneGraph> >(FR_SCENEGRAPH);
		std::shared_ptr<FrRendererEncalps> renderer = ctx->GetProperty< std::shared_ptr<FrRendererEncalps> >(RPR_CONTEXT_ACTIVE_PLUGIN);
		FrNode* node = sceneGraph->CreateNode(NodeTypes::MaterialSystem, [&](FrNode* n) {
			n->SetProperty(FR_NODE_CONTEXT, ctx);
			n->SetProperty(RPR_MATERIAL_NODE_SYSTEM, in_type);
            n->AddProperty(FR_SCENEGRAPH, sceneGraph);
			n->AddProperty(RPR_CONTEXT_ACTIVE_PLUGIN, renderer);
		});

		*out_matsys = node;

		s_logger.Trace__NewFrObjectCreated(Logger::TRACE_ADDRESS_TYPE__MATERIALSYSTEM, *out_matsys);
		s_logger.Trace__DeclareFRobject("rpr_material_system", "materialsystem_0x", *out_matsys);
		s_logger.printTrace("status = ");
		TRACE__FUNCTION_OPEN
		s_logger.TraceArg__rpr_context(in_context); s_logger.TraceArg__COMMA();
		s_logger.TraceArg__rpr_material_system_type(in_type); s_logger.TraceArg__COMMA();
		s_logger.printTrace("&materialsystem_0x%p", *out_matsys);
		s_logger.Trace__FunctionClose();
		s_logger.printTrace("status = rprObjectSetName (materialsystem_0x%p,\"materialsystem_0x%p\"); RPRTRACE_CHECK// added by tracing for debug\r\n",  *out_matsys ,  *out_matsys);

		return RPR_SUCCESS;
	}
	catch (FrException& e)
	{
		TRACE__FUNCTION_FAILED(ctx); return e.GetErrorCode();
	}
}

extern RPR_API_ENTRY rpr_int rprMaterialSystemGetSize(rpr_context in_context, rpr_uint* out_size)
{
	s_logger.printTrace("//"); //  ignore getInfo for the trace
	s_logger.printTrace("status = ");
	TRACE__FUNCTION_OPEN
	s_logger.Trace__FunctionClose();

	FrNode* ctx = static_cast<FrNode*>(in_context);

	try
	{
		CHECK_NOT_NULL(ctx);

		if (!out_size)
			throw FrException(__FILE__,__LINE__,RPR_ERROR_INVALID_PARAMETER, "",ctx);

		std::shared_ptr<FrRendererEncalps> renderer = ctx->GetProperty<  std::shared_ptr<FrRendererEncalps>  >(RPR_CONTEXT_ACTIVE_PLUGIN);
		if (!renderer->m_FrRenderer) throw FrException(__FILE__,__LINE__,RPR_ERROR_INVALID_PARAMETER, "No active compute API set",ctx);
		
		*out_size = renderer->m_FrRenderer->GetMaterialSystemSize();
		
		return RPR_SUCCESS;
	}
	catch (FrException& e)
	{
		TRACE__FUNCTION_FAILED(ctx); return e.GetErrorCode();
	}
}

#ifdef RPR_FOR_BAIKAL
#define RPR_TYPE_OF_MATERIAL_NODE std::shared_ptr<MaterialNode>
#define RPR_TYPE_OF_MATERIAL_NODE_INIT std::shared_ptr<MaterialNode>(new MaterialNode())
#else
#define RPR_TYPE_OF_MATERIAL_NODE FrNode*
#define RPR_TYPE_OF_MATERIAL_NODE_INIT nullptr
#endif


rpr_int rprMaterialSystemCreateNode(rpr_material_system in_matsys, rpr_material_node_type in_type, rpr_material_node* out_node)
{
	//in case of issues during creation, it's safer to return a null pointer, rather than let the pointer undefined.
	if ( out_node )
		*out_node = nullptr;
	

	API_PRINT("rprMaterialSystemCreateNode", 1, in_matsys);

	FrNode* ctx = nullptr;

	FrNode* materialSystem = static_cast<FrNode*>(in_matsys);

	if ( materialSystem )
		ctx = materialSystem->GetProperty<FrNode*>(FR_NODE_CONTEXT);

	try
	{
		CHECK_NOT_NULL(materialSystem);

		MACRO__CHECK_ARGUMENT_TYPE(in_matsys,MaterialSystem);

		std::shared_ptr<FrSceneGraph> sceneGraph = ctx->GetProperty< std::shared_ptr<FrSceneGraph> >(FR_SCENEGRAPH);
		std::shared_ptr<FrRendererEncalps> renderer = ctx->GetProperty< std::shared_ptr<FrRendererEncalps> >(RPR_CONTEXT_ACTIVE_PLUGIN);

		if (in_type != RPR_MATERIAL_NODE_STANDARD)
		{
			FrNode* node = sceneGraph->CreateNode(NodeTypes::Material, [&](FrNode* n) {

			// Material properties have to be handled on a per rpr_material_node_type basis.
			// Else all rpr_material_node_types would have the superset of properties which would be problematic.
			// These added properties are marked as mutable in order to properly support the material system.
			// This means the type associated with each property can be modified at runtime.
			switch (in_type)
			{
			case RPR_MATERIAL_NODE_DIFFUSE:
				nbMaterialInputCount[in_type] = 0;
				n->AddProperty<RPR_TYPE_OF_MATERIAL_NODE>(RPR_MATERIAL_INPUT_COLOR, RPR_TYPE_OF_MATERIAL_NODE_INIT); nbMaterialInputCount[in_type]++;
				n->AddProperty<RPR_TYPE_OF_MATERIAL_NODE>(RPR_MATERIAL_INPUT_NORMAL, RPR_TYPE_OF_MATERIAL_NODE_INIT); nbMaterialInputCount[in_type]++;
				n->AddProperty<RPR_TYPE_OF_MATERIAL_NODE>(RPR_MATERIAL_INPUT_ROUGHNESS, RPR_TYPE_OF_MATERIAL_NODE_INIT); nbMaterialInputCount[in_type]++;
				break;

			case RPR_MATERIAL_NODE_ORENNAYAR:
				nbMaterialInputCount[in_type] = 0;
				n->AddProperty<RPR_TYPE_OF_MATERIAL_NODE>(RPR_MATERIAL_INPUT_COLOR, RPR_TYPE_OF_MATERIAL_NODE_INIT); nbMaterialInputCount[in_type]++;
				n->AddProperty<RPR_TYPE_OF_MATERIAL_NODE>(RPR_MATERIAL_INPUT_NORMAL, RPR_TYPE_OF_MATERIAL_NODE_INIT); nbMaterialInputCount[in_type]++;
				n->AddProperty<RPR_TYPE_OF_MATERIAL_NODE>(RPR_MATERIAL_INPUT_ROUGHNESS, RPR_TYPE_OF_MATERIAL_NODE_INIT); nbMaterialInputCount[in_type]++;
				break;

			case RPR_MATERIAL_NODE_MICROFACET:
				nbMaterialInputCount[in_type] = 0;
				n->AddProperty<RPR_TYPE_OF_MATERIAL_NODE>(RPR_MATERIAL_INPUT_COLOR, RPR_TYPE_OF_MATERIAL_NODE_INIT); nbMaterialInputCount[in_type]++;
				n->AddProperty<RPR_TYPE_OF_MATERIAL_NODE>(RPR_MATERIAL_INPUT_NORMAL, RPR_TYPE_OF_MATERIAL_NODE_INIT); nbMaterialInputCount[in_type]++;
				n->AddProperty<RPR_TYPE_OF_MATERIAL_NODE>(RPR_MATERIAL_INPUT_IOR, RPR_TYPE_OF_MATERIAL_NODE_INIT); nbMaterialInputCount[in_type]++;
				n->AddProperty<RPR_TYPE_OF_MATERIAL_NODE>(RPR_MATERIAL_INPUT_ROUGHNESS, RPR_TYPE_OF_MATERIAL_NODE_INIT); nbMaterialInputCount[in_type]++;
				break;

			case RPR_MATERIAL_NODE_MICROFACET_BECKMANN:
				nbMaterialInputCount[in_type] = 0;
				n->AddProperty<RPR_TYPE_OF_MATERIAL_NODE>(RPR_MATERIAL_INPUT_COLOR, RPR_TYPE_OF_MATERIAL_NODE_INIT); nbMaterialInputCount[in_type]++;
				n->AddProperty<RPR_TYPE_OF_MATERIAL_NODE>(RPR_MATERIAL_INPUT_NORMAL, RPR_TYPE_OF_MATERIAL_NODE_INIT); nbMaterialInputCount[in_type]++;
				n->AddProperty<RPR_TYPE_OF_MATERIAL_NODE>(RPR_MATERIAL_INPUT_IOR, RPR_TYPE_OF_MATERIAL_NODE_INIT); nbMaterialInputCount[in_type]++;
				n->AddProperty<RPR_TYPE_OF_MATERIAL_NODE>(RPR_MATERIAL_INPUT_ROUGHNESS, RPR_TYPE_OF_MATERIAL_NODE_INIT); nbMaterialInputCount[in_type]++;
				break;

			case RPR_MATERIAL_NODE_PHONG:
				nbMaterialInputCount[in_type] = 0;
				n->AddProperty<RPR_TYPE_OF_MATERIAL_NODE>(RPR_MATERIAL_INPUT_COLOR, RPR_TYPE_OF_MATERIAL_NODE_INIT); nbMaterialInputCount[in_type]++;
				n->AddProperty<RPR_TYPE_OF_MATERIAL_NODE>(RPR_MATERIAL_INPUT_NORMAL, RPR_TYPE_OF_MATERIAL_NODE_INIT); nbMaterialInputCount[in_type]++;
				n->AddProperty<RPR_TYPE_OF_MATERIAL_NODE>(RPR_MATERIAL_INPUT_IOR, RPR_TYPE_OF_MATERIAL_NODE_INIT); nbMaterialInputCount[in_type]++;
				n->AddProperty<RPR_TYPE_OF_MATERIAL_NODE>(RPR_MATERIAL_INPUT_ROUGHNESS, RPR_TYPE_OF_MATERIAL_NODE_INIT); nbMaterialInputCount[in_type]++;
				break;
				
			case RPR_MATERIAL_NODE_REFLECTION:
				nbMaterialInputCount[in_type] = 0;
				n->AddProperty<RPR_TYPE_OF_MATERIAL_NODE>(RPR_MATERIAL_INPUT_COLOR, RPR_TYPE_OF_MATERIAL_NODE_INIT); nbMaterialInputCount[in_type]++;
				n->AddProperty<RPR_TYPE_OF_MATERIAL_NODE>(RPR_MATERIAL_INPUT_NORMAL, RPR_TYPE_OF_MATERIAL_NODE_INIT); nbMaterialInputCount[in_type]++;
				break;

			case RPR_MATERIAL_NODE_REFRACTION:
				nbMaterialInputCount[in_type] = 0;
				n->AddProperty<RPR_TYPE_OF_MATERIAL_NODE>(RPR_MATERIAL_INPUT_COLOR, RPR_TYPE_OF_MATERIAL_NODE_INIT); nbMaterialInputCount[in_type]++;
				n->AddProperty<RPR_TYPE_OF_MATERIAL_NODE>(RPR_MATERIAL_INPUT_NORMAL, RPR_TYPE_OF_MATERIAL_NODE_INIT); nbMaterialInputCount[in_type]++;
				n->AddProperty<RPR_TYPE_OF_MATERIAL_NODE>(RPR_MATERIAL_INPUT_IOR, RPR_TYPE_OF_MATERIAL_NODE_INIT); nbMaterialInputCount[in_type]++;
				break;

			case RPR_MATERIAL_NODE_MICROFACET_REFRACTION:
				nbMaterialInputCount[in_type] = 0;
				n->AddProperty<RPR_TYPE_OF_MATERIAL_NODE>(RPR_MATERIAL_INPUT_COLOR, RPR_TYPE_OF_MATERIAL_NODE_INIT); nbMaterialInputCount[in_type]++;
				n->AddProperty<RPR_TYPE_OF_MATERIAL_NODE>(RPR_MATERIAL_INPUT_NORMAL, RPR_TYPE_OF_MATERIAL_NODE_INIT); nbMaterialInputCount[in_type]++;
				n->AddProperty<RPR_TYPE_OF_MATERIAL_NODE>(RPR_MATERIAL_INPUT_IOR, RPR_TYPE_OF_MATERIAL_NODE_INIT); nbMaterialInputCount[in_type]++;
				n->AddProperty<RPR_TYPE_OF_MATERIAL_NODE>(RPR_MATERIAL_INPUT_ROUGHNESS, RPR_TYPE_OF_MATERIAL_NODE_INIT); nbMaterialInputCount[in_type]++;
				break;

			case RPR_MATERIAL_NODE_MICROFACET_ANISOTROPIC_REFLECTION:
				nbMaterialInputCount[in_type] = 0;
				n->AddProperty<RPR_TYPE_OF_MATERIAL_NODE>(RPR_MATERIAL_INPUT_COLOR, RPR_TYPE_OF_MATERIAL_NODE_INIT); nbMaterialInputCount[in_type]++;
				n->AddProperty<RPR_TYPE_OF_MATERIAL_NODE>(RPR_MATERIAL_INPUT_NORMAL, RPR_TYPE_OF_MATERIAL_NODE_INIT); nbMaterialInputCount[in_type]++;
				n->AddProperty<RPR_TYPE_OF_MATERIAL_NODE>(RPR_MATERIAL_INPUT_IOR, RPR_TYPE_OF_MATERIAL_NODE_INIT); nbMaterialInputCount[in_type]++;
				n->AddProperty<RPR_TYPE_OF_MATERIAL_NODE>(RPR_MATERIAL_INPUT_ROUGHNESS, RPR_TYPE_OF_MATERIAL_NODE_INIT); nbMaterialInputCount[in_type]++;
				n->AddProperty<RPR_TYPE_OF_MATERIAL_NODE>(RPR_MATERIAL_INPUT_ANISOTROPIC, RPR_TYPE_OF_MATERIAL_NODE_INIT); nbMaterialInputCount[in_type]++;
				n->AddProperty<RPR_TYPE_OF_MATERIAL_NODE>(RPR_MATERIAL_INPUT_ROTATION, RPR_TYPE_OF_MATERIAL_NODE_INIT); nbMaterialInputCount[in_type]++;
				break;

			case RPR_MATERIAL_NODE_MICROFACET_ANISOTROPIC_REFRACTION:
				nbMaterialInputCount[in_type] = 0;
				n->AddProperty<RPR_TYPE_OF_MATERIAL_NODE>(RPR_MATERIAL_INPUT_COLOR, RPR_TYPE_OF_MATERIAL_NODE_INIT); nbMaterialInputCount[in_type]++;
				n->AddProperty<RPR_TYPE_OF_MATERIAL_NODE>(RPR_MATERIAL_INPUT_NORMAL, RPR_TYPE_OF_MATERIAL_NODE_INIT); nbMaterialInputCount[in_type]++;
				n->AddProperty<RPR_TYPE_OF_MATERIAL_NODE>(RPR_MATERIAL_INPUT_IOR, RPR_TYPE_OF_MATERIAL_NODE_INIT); nbMaterialInputCount[in_type]++;
				n->AddProperty<RPR_TYPE_OF_MATERIAL_NODE>(RPR_MATERIAL_INPUT_ROUGHNESS, RPR_TYPE_OF_MATERIAL_NODE_INIT); nbMaterialInputCount[in_type]++;
				n->AddProperty<RPR_TYPE_OF_MATERIAL_NODE>(RPR_MATERIAL_INPUT_ANISOTROPIC, RPR_TYPE_OF_MATERIAL_NODE_INIT); nbMaterialInputCount[in_type]++;
				n->AddProperty<RPR_TYPE_OF_MATERIAL_NODE>(RPR_MATERIAL_INPUT_ROTATION, RPR_TYPE_OF_MATERIAL_NODE_INIT); nbMaterialInputCount[in_type]++;
				break;

			case RPR_MATERIAL_NODE_DIFFUSE_REFRACTION:
				nbMaterialInputCount[in_type] = 0;
				n->AddProperty<RPR_TYPE_OF_MATERIAL_NODE>(RPR_MATERIAL_INPUT_COLOR, RPR_TYPE_OF_MATERIAL_NODE_INIT); nbMaterialInputCount[in_type]++;
				n->AddProperty<RPR_TYPE_OF_MATERIAL_NODE>(RPR_MATERIAL_INPUT_NORMAL, RPR_TYPE_OF_MATERIAL_NODE_INIT); nbMaterialInputCount[in_type]++;
				n->AddProperty<RPR_TYPE_OF_MATERIAL_NODE>(RPR_MATERIAL_INPUT_ROUGHNESS, RPR_TYPE_OF_MATERIAL_NODE_INIT); nbMaterialInputCount[in_type]++;
				break;

			case RPR_MATERIAL_NODE_TRANSPARENT:
				nbMaterialInputCount[in_type] = 0;
				n->AddProperty<RPR_TYPE_OF_MATERIAL_NODE>(RPR_MATERIAL_INPUT_COLOR, RPR_TYPE_OF_MATERIAL_NODE_INIT); nbMaterialInputCount[in_type]++;
				break;

			case RPR_MATERIAL_NODE_EMISSIVE:
				nbMaterialInputCount[in_type] = 0;
				n->AddProperty<RPR_TYPE_OF_MATERIAL_NODE>(RPR_MATERIAL_INPUT_COLOR, RPR_TYPE_OF_MATERIAL_NODE_INIT); nbMaterialInputCount[in_type]++;
				break;

			case RPR_MATERIAL_NODE_PASSTHROUGH:
				nbMaterialInputCount[in_type] = 0;
				n->AddProperty<RPR_TYPE_OF_MATERIAL_NODE>(RPR_MATERIAL_INPUT_COLOR, RPR_TYPE_OF_MATERIAL_NODE_INIT); nbMaterialInputCount[in_type]++;
				break;

			case RPR_MATERIAL_NODE_WARD:
				nbMaterialInputCount[in_type] = 0;
				n->AddProperty<RPR_TYPE_OF_MATERIAL_NODE>(RPR_MATERIAL_INPUT_COLOR, RPR_TYPE_OF_MATERIAL_NODE_INIT); nbMaterialInputCount[in_type]++;
				n->AddProperty<RPR_TYPE_OF_MATERIAL_NODE>(RPR_MATERIAL_INPUT_NORMAL, RPR_TYPE_OF_MATERIAL_NODE_INIT); nbMaterialInputCount[in_type]++;
				n->AddProperty<RPR_TYPE_OF_MATERIAL_NODE>(RPR_MATERIAL_INPUT_ROUGHNESS_X, RPR_TYPE_OF_MATERIAL_NODE_INIT); nbMaterialInputCount[in_type]++;
				n->AddProperty<RPR_TYPE_OF_MATERIAL_NODE>(RPR_MATERIAL_INPUT_ROUGHNESS_Y, RPR_TYPE_OF_MATERIAL_NODE_INIT); nbMaterialInputCount[in_type]++;
				n->AddProperty<RPR_TYPE_OF_MATERIAL_NODE>(RPR_MATERIAL_INPUT_ROTATION, RPR_TYPE_OF_MATERIAL_NODE_INIT); nbMaterialInputCount[in_type]++;
				break;

			case RPR_MATERIAL_NODE_ADD:
				nbMaterialInputCount[in_type] = 0;
				n->AddProperty<RPR_TYPE_OF_MATERIAL_NODE>(RPR_MATERIAL_INPUT_COLOR0, RPR_TYPE_OF_MATERIAL_NODE_INIT); nbMaterialInputCount[in_type]++;
				n->AddProperty<RPR_TYPE_OF_MATERIAL_NODE>(RPR_MATERIAL_INPUT_COLOR1, RPR_TYPE_OF_MATERIAL_NODE_INIT); nbMaterialInputCount[in_type]++;
				break;

			case RPR_MATERIAL_NODE_BLEND:
				nbMaterialInputCount[in_type] = 0;
				n->AddProperty<RPR_TYPE_OF_MATERIAL_NODE>(RPR_MATERIAL_INPUT_COLOR0, RPR_TYPE_OF_MATERIAL_NODE_INIT); nbMaterialInputCount[in_type]++;
				n->AddProperty<RPR_TYPE_OF_MATERIAL_NODE>(RPR_MATERIAL_INPUT_COLOR1, RPR_TYPE_OF_MATERIAL_NODE_INIT); nbMaterialInputCount[in_type]++;
				n->AddProperty<RPR_TYPE_OF_MATERIAL_NODE>(RPR_MATERIAL_INPUT_WEIGHT, RPR_TYPE_OF_MATERIAL_NODE_INIT); nbMaterialInputCount[in_type]++;
				break;

			case RPR_MATERIAL_NODE_ARITHMETIC:
				nbMaterialInputCount[in_type] = 0;
				n->AddProperty<RPR_TYPE_OF_MATERIAL_NODE>(RPR_MATERIAL_INPUT_COLOR0, RPR_TYPE_OF_MATERIAL_NODE_INIT); nbMaterialInputCount[in_type]++;
				n->AddProperty<RPR_TYPE_OF_MATERIAL_NODE>(RPR_MATERIAL_INPUT_COLOR1, RPR_TYPE_OF_MATERIAL_NODE_INIT); nbMaterialInputCount[in_type]++;
				n->AddProperty<RPR_TYPE_OF_MATERIAL_NODE>(RPR_MATERIAL_INPUT_COLOR2, RPR_TYPE_OF_MATERIAL_NODE_INIT); nbMaterialInputCount[in_type]++;
				n->AddProperty<RPR_TYPE_OF_MATERIAL_NODE>(RPR_MATERIAL_INPUT_COLOR3, RPR_TYPE_OF_MATERIAL_NODE_INIT); nbMaterialInputCount[in_type]++;

				#ifdef RPR_FOR_BAIKAL
				n->AddProperty<RPR_TYPE_OF_MATERIAL_NODE>(RPR_MATERIAL_INPUT_OP, std::shared_ptr<MaterialNode>(new MaterialNode(RPR_MATERIAL_NODE_OP_ADD))); nbMaterialInputCount[in_type]++;
				#else
				n->AddProperty<rpr_uint>(RPR_MATERIAL_INPUT_OP, RPR_MATERIAL_NODE_OP_ADD); nbMaterialInputCount[in_type]++;
				#endif

				break;

			case RPR_MATERIAL_NODE_FRESNEL:
				nbMaterialInputCount[in_type] = 0;
				n->AddProperty<RPR_TYPE_OF_MATERIAL_NODE>(RPR_MATERIAL_INPUT_INVEC, RPR_TYPE_OF_MATERIAL_NODE_INIT); nbMaterialInputCount[in_type]++;
				n->AddProperty<RPR_TYPE_OF_MATERIAL_NODE>(RPR_MATERIAL_INPUT_NORMAL, RPR_TYPE_OF_MATERIAL_NODE_INIT); nbMaterialInputCount[in_type]++;
				n->AddProperty<RPR_TYPE_OF_MATERIAL_NODE>(RPR_MATERIAL_INPUT_IOR, RPR_TYPE_OF_MATERIAL_NODE_INIT); nbMaterialInputCount[in_type]++;
				break;

			case RPR_MATERIAL_NODE_FRESNEL_SCHLICK:
				nbMaterialInputCount[in_type] = 0;
				n->AddProperty<RPR_TYPE_OF_MATERIAL_NODE>(RPR_MATERIAL_INPUT_INVEC, RPR_TYPE_OF_MATERIAL_NODE_INIT); nbMaterialInputCount[in_type]++;
				n->AddProperty<RPR_TYPE_OF_MATERIAL_NODE>(RPR_MATERIAL_INPUT_NORMAL, RPR_TYPE_OF_MATERIAL_NODE_INIT); nbMaterialInputCount[in_type]++;
				n->AddProperty<RPR_TYPE_OF_MATERIAL_NODE>(RPR_MATERIAL_INPUT_REFLECTANCE, RPR_TYPE_OF_MATERIAL_NODE_INIT); nbMaterialInputCount[in_type]++;
				break;

			case RPR_MATERIAL_NODE_NORMAL_MAP:
				nbMaterialInputCount[in_type] = 0;
				n->AddProperty<RPR_TYPE_OF_MATERIAL_NODE>(RPR_MATERIAL_INPUT_COLOR, RPR_TYPE_OF_MATERIAL_NODE_INIT); nbMaterialInputCount[in_type]++;
				n->AddProperty<RPR_TYPE_OF_MATERIAL_NODE>(RPR_MATERIAL_INPUT_SCALE, RPR_TYPE_OF_MATERIAL_NODE_INIT); nbMaterialInputCount[in_type]++;
				break;

			case RPR_MATERIAL_NODE_UV_PROCEDURAL:
				nbMaterialInputCount[in_type] = 0;
				n->AddProperty<rpr_uint>(RPR_MATERIAL_INPUT_UV_TYPE, RPR_MATERIAL_NODE_UVTYPE_PLANAR); nbMaterialInputCount[in_type]++;
				n->AddProperty<RPR_TYPE_OF_MATERIAL_NODE>(RPR_MATERIAL_INPUT_ORIGIN, RPR_TYPE_OF_MATERIAL_NODE_INIT); nbMaterialInputCount[in_type]++;
				n->AddProperty<RPR_TYPE_OF_MATERIAL_NODE>(RPR_MATERIAL_INPUT_ZAXIS, RPR_TYPE_OF_MATERIAL_NODE_INIT); nbMaterialInputCount[in_type]++;
				n->AddProperty<RPR_TYPE_OF_MATERIAL_NODE>(RPR_MATERIAL_INPUT_XAXIS, RPR_TYPE_OF_MATERIAL_NODE_INIT); nbMaterialInputCount[in_type]++;
				n->AddProperty<RPR_TYPE_OF_MATERIAL_NODE>(RPR_MATERIAL_INPUT_UV_SCALE, RPR_TYPE_OF_MATERIAL_NODE_INIT); nbMaterialInputCount[in_type]++;
				n->AddProperty<RPR_TYPE_OF_MATERIAL_NODE>(RPR_MATERIAL_INPUT_THRESHOLD, RPR_TYPE_OF_MATERIAL_NODE_INIT); nbMaterialInputCount[in_type]++;
				break;

			case RPR_MATERIAL_NODE_UV_TRIPLANAR:
				nbMaterialInputCount[in_type] = 0;
				n->AddProperty<RPR_TYPE_OF_MATERIAL_NODE>(RPR_MATERIAL_INPUT_OFFSET, RPR_TYPE_OF_MATERIAL_NODE_INIT); nbMaterialInputCount[in_type]++;
				n->AddProperty<RPR_TYPE_OF_MATERIAL_NODE>(RPR_MATERIAL_INPUT_ZAXIS, RPR_TYPE_OF_MATERIAL_NODE_INIT); nbMaterialInputCount[in_type]++;
				n->AddProperty<RPR_TYPE_OF_MATERIAL_NODE>(RPR_MATERIAL_INPUT_XAXIS, RPR_TYPE_OF_MATERIAL_NODE_INIT); nbMaterialInputCount[in_type]++;
				n->AddProperty<RPR_TYPE_OF_MATERIAL_NODE>(RPR_MATERIAL_INPUT_UV_SCALE, RPR_TYPE_OF_MATERIAL_NODE_INIT); nbMaterialInputCount[in_type]++;
				n->AddProperty<RPR_TYPE_OF_MATERIAL_NODE>(RPR_MATERIAL_INPUT_WEIGHT, RPR_TYPE_OF_MATERIAL_NODE_INIT); nbMaterialInputCount[in_type]++;
				break;

			case RPR_MATERIAL_NODE_BUMP_MAP:
				nbMaterialInputCount[in_type] = 0;
				n->AddProperty<RPR_TYPE_OF_MATERIAL_NODE>(RPR_MATERIAL_INPUT_COLOR, RPR_TYPE_OF_MATERIAL_NODE_INIT); nbMaterialInputCount[in_type]++;
				n->AddProperty<RPR_TYPE_OF_MATERIAL_NODE>(RPR_MATERIAL_INPUT_SCALE, RPR_TYPE_OF_MATERIAL_NODE_INIT); nbMaterialInputCount[in_type]++;
				break;

			case RPR_MATERIAL_NODE_AO_MAP:
				nbMaterialInputCount[in_type] = 0;
				n->AddProperty<RPR_TYPE_OF_MATERIAL_NODE>(RPR_MATERIAL_INPUT_RADIUS, RPR_TYPE_OF_MATERIAL_NODE_INIT); nbMaterialInputCount[in_type]++;
				n->AddProperty<RPR_TYPE_OF_MATERIAL_NODE>(RPR_MATERIAL_INPUT_SIDE, RPR_TYPE_OF_MATERIAL_NODE_INIT); nbMaterialInputCount[in_type]++;
				break;

			case RPR_MATERIAL_NODE_IMAGE_TEXTURE:
				nbMaterialInputCount[in_type] = 0;
				n->AddProperty<RPR_TYPE_OF_MATERIAL_NODE>(RPR_MATERIAL_INPUT_DATA, RPR_TYPE_OF_MATERIAL_NODE_INIT, true); nbMaterialInputCount[in_type]++;
				n->AddProperty<RPR_TYPE_OF_MATERIAL_NODE>(RPR_MATERIAL_INPUT_UV, RPR_TYPE_OF_MATERIAL_NODE_INIT); nbMaterialInputCount[in_type]++;
				break;

			case RPR_MATERIAL_NODE_BUFFER_SAMPLER:
				nbMaterialInputCount[in_type] = 0;
				n->AddProperty<RPR_TYPE_OF_MATERIAL_NODE>(RPR_MATERIAL_INPUT_DATA, RPR_TYPE_OF_MATERIAL_NODE_INIT, true); nbMaterialInputCount[in_type]++;
				n->AddProperty<RPR_TYPE_OF_MATERIAL_NODE>(RPR_MATERIAL_INPUT_UV, RPR_TYPE_OF_MATERIAL_NODE_INIT); nbMaterialInputCount[in_type]++;
				break;

			case RPR_MATERIAL_NODE_NOISE2D_TEXTURE:
				nbMaterialInputCount[in_type] = 0;
				n->AddProperty<RPR_TYPE_OF_MATERIAL_NODE>(RPR_MATERIAL_INPUT_UV, RPR_TYPE_OF_MATERIAL_NODE_INIT); nbMaterialInputCount[in_type]++;
				break;

			case RPR_MATERIAL_NODE_DOT_TEXTURE:
				nbMaterialInputCount[in_type] = 0;
				n->AddProperty<RPR_TYPE_OF_MATERIAL_NODE>(RPR_MATERIAL_INPUT_UV, RPR_TYPE_OF_MATERIAL_NODE_INIT); nbMaterialInputCount[in_type]++;
				break;

			case RPR_MATERIAL_NODE_GRADIENT_TEXTURE:
				nbMaterialInputCount[in_type] = 0;
				n->AddProperty<RPR_TYPE_OF_MATERIAL_NODE>(RPR_MATERIAL_INPUT_UV, RPR_TYPE_OF_MATERIAL_NODE_INIT); nbMaterialInputCount[in_type]++;
				n->AddProperty<RPR_TYPE_OF_MATERIAL_NODE>(RPR_MATERIAL_INPUT_COLOR0, RPR_TYPE_OF_MATERIAL_NODE_INIT); nbMaterialInputCount[in_type]++;
				n->AddProperty<RPR_TYPE_OF_MATERIAL_NODE>(RPR_MATERIAL_INPUT_COLOR1, RPR_TYPE_OF_MATERIAL_NODE_INIT); nbMaterialInputCount[in_type]++;
				break;

			case RPR_MATERIAL_NODE_CHECKER_TEXTURE:
				nbMaterialInputCount[in_type] = 0;
				n->AddProperty<RPR_TYPE_OF_MATERIAL_NODE>(RPR_MATERIAL_INPUT_UV, RPR_TYPE_OF_MATERIAL_NODE_INIT); nbMaterialInputCount[in_type]++;
				break;

			case RPR_MATERIAL_NODE_CONSTANT_TEXTURE:
				nbMaterialInputCount[in_type] = 0;
				n->AddProperty <rpr_float4> (RPR_MATERIAL_INPUT_VALUE, rpr_float4(0,0,0,0)); nbMaterialInputCount[in_type]++;
				break;

			case RPR_MATERIAL_NODE_INPUT_LOOKUP:
				nbMaterialInputCount[in_type] = 0;
				n->AddProperty<rpr_uint>(RPR_MATERIAL_INPUT_VALUE, RPR_MATERIAL_NODE_LOOKUP_N); nbMaterialInputCount[in_type]++;
				break;

			case RPR_MATERIAL_NODE_STANDARD:
				break;

			case RPR_MATERIAL_NODE_BLEND_VALUE:
				nbMaterialInputCount[in_type] = 0;
				n->AddProperty<RPR_TYPE_OF_MATERIAL_NODE>(RPR_MATERIAL_INPUT_WEIGHT, RPR_TYPE_OF_MATERIAL_NODE_INIT); nbMaterialInputCount[in_type]++;
				n->AddProperty<RPR_TYPE_OF_MATERIAL_NODE>(RPR_MATERIAL_INPUT_COLOR0, RPR_TYPE_OF_MATERIAL_NODE_INIT); nbMaterialInputCount[in_type]++;
				n->AddProperty<RPR_TYPE_OF_MATERIAL_NODE>(RPR_MATERIAL_INPUT_COLOR1, RPR_TYPE_OF_MATERIAL_NODE_INIT); nbMaterialInputCount[in_type]++;
				break;

            case RPR_MATERIAL_NODE_VOLUME:
				nbMaterialInputCount[in_type] = 0;
                n->AddProperty<RPR_TYPE_OF_MATERIAL_NODE>(RPR_MATERIAL_INPUT_SCATTERING, RPR_TYPE_OF_MATERIAL_NODE_INIT); nbMaterialInputCount[in_type]++;
                n->AddProperty<RPR_TYPE_OF_MATERIAL_NODE>(RPR_MATERIAL_INPUT_ABSORBTION, RPR_TYPE_OF_MATERIAL_NODE_INIT); nbMaterialInputCount[in_type]++;
                n->AddProperty<RPR_TYPE_OF_MATERIAL_NODE>(RPR_MATERIAL_INPUT_EMISSION, RPR_TYPE_OF_MATERIAL_NODE_INIT); nbMaterialInputCount[in_type]++;
                n->AddProperty<RPR_TYPE_OF_MATERIAL_NODE>(RPR_MATERIAL_INPUT_G, RPR_TYPE_OF_MATERIAL_NODE_INIT); nbMaterialInputCount[in_type]++;
                n->AddProperty<RPR_TYPE_OF_MATERIAL_NODE>(RPR_MATERIAL_INPUT_MULTISCATTER, RPR_TYPE_OF_MATERIAL_NODE_INIT); nbMaterialInputCount[in_type]++;
                break;

			case RPR_MATERIAL_NODE_TWOSIDED:
				nbMaterialInputCount[in_type] = 0;
                n->AddProperty<RPR_TYPE_OF_MATERIAL_NODE>(RPR_MATERIAL_INPUT_FRONTFACE, RPR_TYPE_OF_MATERIAL_NODE_INIT); nbMaterialInputCount[in_type]++;
                n->AddProperty<RPR_TYPE_OF_MATERIAL_NODE>(RPR_MATERIAL_INPUT_BACKFACE, RPR_TYPE_OF_MATERIAL_NODE_INIT); nbMaterialInputCount[in_type]++;
                break;

				

			#ifdef RPR_FOR_BAIKAL

			case RPR_MATERIAL_NODE_UBERV2:
				nbMaterialInputCount[in_type] = 0;
				n->AddProperty<std::shared_ptr<MaterialNode>>(RPR_UBER_MATERIAL_LAYERS, std::shared_ptr<MaterialNode>(new MaterialNode()) ); nbMaterialInputCount[in_type]++;

				n->AddProperty<std::shared_ptr<MaterialNode>>(RPR_UBER_MATERIAL_DIFFUSE_COLOR , std::shared_ptr<MaterialNode>(new MaterialNode()) ); nbMaterialInputCount[in_type]++;
				n->AddProperty<std::shared_ptr<MaterialNode>>(RPR_UBER_MATERIAL_REFLECTION_COLOR  , std::shared_ptr<MaterialNode>(new MaterialNode()) ); nbMaterialInputCount[in_type]++;
				n->AddProperty<std::shared_ptr<MaterialNode>>(RPR_UBER_MATERIAL_REFLECTION_ROUGHNESS  , std::shared_ptr<MaterialNode>(new MaterialNode()) ); nbMaterialInputCount[in_type]++;
				n->AddProperty<std::shared_ptr<MaterialNode>>(RPR_UBER_MATERIAL_REFLECTION_ANISOTROPY  , std::shared_ptr<MaterialNode>(new MaterialNode()) ); nbMaterialInputCount[in_type]++;
				n->AddProperty<std::shared_ptr<MaterialNode>>(RPR_UBER_MATERIAL_REFLECTION_ANISOTROPY_ROTATION  , std::shared_ptr<MaterialNode>(new MaterialNode()) ); nbMaterialInputCount[in_type]++;
				n->AddProperty<std::shared_ptr<MaterialNode>>(RPR_UBER_MATERIAL_REFLECTION_IOR  , std::shared_ptr<MaterialNode>(new MaterialNode()) ); nbMaterialInputCount[in_type]++;
				n->AddProperty<std::shared_ptr<MaterialNode>>(RPR_UBER_MATERIAL_REFLECTION_METALNESS  , std::shared_ptr<MaterialNode>(new MaterialNode()) ); nbMaterialInputCount[in_type]++;
				n->AddProperty<std::shared_ptr<MaterialNode>>(RPR_UBER_MATERIAL_REFRACTION_COLOR  , std::shared_ptr<MaterialNode>(new MaterialNode()) ); nbMaterialInputCount[in_type]++;
				n->AddProperty<std::shared_ptr<MaterialNode>>(RPR_UBER_MATERIAL_REFRACTION_ROUGHNESS  , std::shared_ptr<MaterialNode>(new MaterialNode()) ); nbMaterialInputCount[in_type]++;
				n->AddProperty<std::shared_ptr<MaterialNode>>(RPR_UBER_MATERIAL_REFRACTION_IOR  , std::shared_ptr<MaterialNode>(new MaterialNode()) ); nbMaterialInputCount[in_type]++;
				n->AddProperty<std::shared_ptr<MaterialNode>>(RPR_UBER_MATERIAL_REFRACTION_IOR_MODE  , std::shared_ptr<MaterialNode>(new MaterialNode()) ); nbMaterialInputCount[in_type]++;
				n->AddProperty<std::shared_ptr<MaterialNode>>(RPR_UBER_MATERIAL_REFRACTION_THIN_SURFACE  , std::shared_ptr<MaterialNode>(new MaterialNode()) ); nbMaterialInputCount[in_type]++;
				n->AddProperty<std::shared_ptr<MaterialNode>>(RPR_UBER_MATERIAL_COATING_COLOR  , std::shared_ptr<MaterialNode>(new MaterialNode()) ); nbMaterialInputCount[in_type]++;
				n->AddProperty<std::shared_ptr<MaterialNode>>(RPR_UBER_MATERIAL_COATING_IOR  , std::shared_ptr<MaterialNode>(new MaterialNode()) ); nbMaterialInputCount[in_type]++;
				n->AddProperty<std::shared_ptr<MaterialNode>>(RPR_UBER_MATERIAL_EMISSION_COLOR  , std::shared_ptr<MaterialNode>(new MaterialNode()) ); nbMaterialInputCount[in_type]++;
				n->AddProperty<std::shared_ptr<MaterialNode>>(RPR_UBER_MATERIAL_EMISSION_WEIGHT  , std::shared_ptr<MaterialNode>(new MaterialNode()) ); nbMaterialInputCount[in_type]++;
				n->AddProperty<std::shared_ptr<MaterialNode>>(RPR_UBER_MATERIAL_EMISSION_MODE  , std::shared_ptr<MaterialNode>(new MaterialNode()) ); nbMaterialInputCount[in_type]++;
				n->AddProperty<std::shared_ptr<MaterialNode>>(RPR_UBER_MATERIAL_TRANSPARENCY  , std::shared_ptr<MaterialNode>(new MaterialNode()) ); nbMaterialInputCount[in_type]++;
				n->AddProperty<std::shared_ptr<MaterialNode>>(RPR_UBER_MATERIAL_NORMAL  , std::shared_ptr<MaterialNode>(new MaterialNode()) ); nbMaterialInputCount[in_type]++;
				n->AddProperty<std::shared_ptr<MaterialNode>>(RPR_UBER_MATERIAL_BUMP  , std::shared_ptr<MaterialNode>(new MaterialNode()) ); nbMaterialInputCount[in_type]++;
				n->AddProperty<std::shared_ptr<MaterialNode>>(RPR_UBER_MATERIAL_DISPLACEMENT  , std::shared_ptr<MaterialNode>(new MaterialNode()) ); nbMaterialInputCount[in_type]++;
				n->AddProperty<std::shared_ptr<MaterialNode>>(RPR_UBER_MATERIAL_SSS_ABSORPTION_COLOR  , std::shared_ptr<MaterialNode>(new MaterialNode()) ); nbMaterialInputCount[in_type]++;
				n->AddProperty<std::shared_ptr<MaterialNode>>(RPR_UBER_MATERIAL_SSS_SCATTER_COLOR  , std::shared_ptr<MaterialNode>(new MaterialNode()) ); nbMaterialInputCount[in_type]++;
				n->AddProperty<std::shared_ptr<MaterialNode>>(RPR_UBER_MATERIAL_SSS_ABSORPTION_DISTANCE  , std::shared_ptr<MaterialNode>(new MaterialNode()) ); nbMaterialInputCount[in_type]++;
				n->AddProperty<std::shared_ptr<MaterialNode>>(RPR_UBER_MATERIAL_SSS_SCATTER_DISTANCE  , std::shared_ptr<MaterialNode>(new MaterialNode()) ); nbMaterialInputCount[in_type]++;
				n->AddProperty<std::shared_ptr<MaterialNode>>(RPR_UBER_MATERIAL_SSS_SCATTER_DIRECTION  , std::shared_ptr<MaterialNode>(new MaterialNode()) ); nbMaterialInputCount[in_type]++;
				n->AddProperty<std::shared_ptr<MaterialNode>>(RPR_UBER_MATERIAL_SSS_SUBSURFACE_COLOR  , std::shared_ptr<MaterialNode>(new MaterialNode()) ); nbMaterialInputCount[in_type]++;
				n->AddProperty<std::shared_ptr<MaterialNode>>(RPR_UBER_MATERIAL_SSS_MULTISCATTER  , std::shared_ptr<MaterialNode>(new MaterialNode()) ); nbMaterialInputCount[in_type]++;

				break;
			#endif


			default :
				throw FrException(__FILE__,__LINE__,RPR_ERROR_INVALID_PARAMETER, "Invalid material type",in_matsys);
				break;
			}

			//RASTER IO
			n->AddProperty<RPR_TYPE_OF_MATERIAL_NODE>(RPR_MATERIAL_INPUT_RASTER_COLOR, nullptr);
			n->AddProperty<RPR_TYPE_OF_MATERIAL_NODE>(RPR_MATERIAL_INPUT_RASTER_NORMAL, nullptr);
			n->AddProperty<RPR_TYPE_OF_MATERIAL_NODE>(RPR_MATERIAL_INPUT_RASTER_METALLIC, nullptr);
			n->AddProperty<RPR_TYPE_OF_MATERIAL_NODE>(RPR_MATERIAL_INPUT_RASTER_ROUGHNESS, nullptr);
			n->AddProperty<RPR_TYPE_OF_MATERIAL_NODE>(RPR_MATERIAL_INPUT_RASTER_SUBSURFACE, nullptr);
			n->AddProperty<RPR_TYPE_OF_MATERIAL_NODE>(RPR_MATERIAL_INPUT_RASTER_ANISOTROPIC, nullptr);
			n->AddProperty<RPR_TYPE_OF_MATERIAL_NODE>(RPR_MATERIAL_INPUT_RASTER_SPECULAR, nullptr);
			n->AddProperty<RPR_TYPE_OF_MATERIAL_NODE>(RPR_MATERIAL_INPUT_RASTER_SPECULARTINT, nullptr);
			n->AddProperty<RPR_TYPE_OF_MATERIAL_NODE>(RPR_MATERIAL_INPUT_RASTER_SHEEN, nullptr);
			n->AddProperty<RPR_TYPE_OF_MATERIAL_NODE>(RPR_MATERIAL_INPUT_RASTER_SHEENTINT, nullptr);
			n->AddProperty<RPR_TYPE_OF_MATERIAL_NODE>(RPR_MATERIAL_INPUT_RASTER_CLEARCOAT, nullptr);
			n->AddProperty<RPR_TYPE_OF_MATERIAL_NODE>(RPR_MATERIAL_INPUT_RASTER_CLEARCOATGLOSS, nullptr);

			n->SetProperty(FR_NODE_CONTEXT, ctx);
			n->SetProperty(RPR_MATERIAL_NODE_TYPE, in_type);
			n->SetProperty(RPR_MATERIAL_NODE_SYSTEM, materialSystem);
            n->AddProperty(FR_SCENEGRAPH, sceneGraph);
			n->AddProperty(RPR_CONTEXT_ACTIVE_PLUGIN, renderer);
			});



			*out_node = node;

			s_logger.Trace__NewFrObjectCreated(Logger::TRACE_ADDRESS_TYPE__MATERIALNODE, *out_node);
			s_logger.Trace__DeclareFRobject("rpr_material_node", "materialnode_0x", *out_node);
			s_logger.printTrace("status = ");
			TRACE__FUNCTION_OPEN
			s_logger.TraceArg__rpr_material_system(in_matsys); s_logger.TraceArg__COMMA();
			s_logger.TraceArg__rpr_material_node_type(in_type); s_logger.TraceArg__COMMA();
			s_logger.printTrace("&materialnode_0x%p", *out_node);
			s_logger.Trace__FunctionClose();
			s_logger.printTrace("status = rprObjectSetName (materialnode_0x%p,\"materialnode_0x%p\"); RPRTRACE_CHECK// added by tracing for debug\r\n",  *out_node ,  *out_node);


			return RPR_SUCCESS;
		}
		else
		{
		// if node is RPR_MATERIAL_NODE_STANDARD :
			StandardMaterial newMat;

			// Create a diffuse part
			rpr_material_node diffuse = nullptr;

			s_logger.PauseTracing(); // pause tracing, because calling exposed traced API function internally
			rpr_int status = rprMaterialSystemCreateNode(in_matsys, RPR_MATERIAL_NODE_DIFFUSE, &diffuse);
			s_logger.ContinueTracing();

			if (status != RPR_SUCCESS)
			{
				throw FrException(__FILE__,__LINE__,status, "",ctx);
			}

			newMat.emplace(std::make_pair(StandardMaterialComponent::Diffuse , (rpr_material_node)diffuse));

			// Create refractive part
			rpr_material_node refraction = nullptr;

			s_logger.PauseTracing(); // pause tracing, because calling exposed traced API function internally
			status = rprMaterialSystemCreateNode(in_matsys, RPR_MATERIAL_NODE_MICROFACET_REFRACTION, &refraction);
			s_logger.ContinueTracing();

			if (status != RPR_SUCCESS)
			{
				throw FrException(__FILE__,__LINE__,status, "",ctx);
			}

			newMat.emplace(std::make_pair(StandardMaterialComponent::Refraction , (rpr_material_node)refraction));

			// Create glossy part
			rpr_material_node glossy = nullptr;

			s_logger.PauseTracing(); // pause tracing, because calling exposed traced API function internally
			status = rprMaterialSystemCreateNode(in_matsys, RPR_MATERIAL_NODE_WARD, &glossy);
			s_logger.ContinueTracing();

			if (status != RPR_SUCCESS)
			{
				throw FrException(__FILE__,__LINE__,status, "",ctx);
			}

			newMat.emplace(std::make_pair(StandardMaterialComponent::Glossy, (rpr_material_node)glossy));

			// Create clearcoat part
			rpr_material_node clearcoat = nullptr;

			s_logger.PauseTracing(); // pause tracing, because calling exposed traced API function internally
			status = rprMaterialSystemCreateNode(in_matsys, RPR_MATERIAL_NODE_REFLECTION, &clearcoat);
			s_logger.ContinueTracing();

			if (status != RPR_SUCCESS)
			{
				throw FrException(__FILE__,__LINE__,status, "",ctx);
			}

			newMat.emplace(std::make_pair(StandardMaterialComponent::Clearcoat, (rpr_material_node)clearcoat));

			// Create transparency part
			rpr_material_node transparency = nullptr;

			s_logger.PauseTracing(); // pause tracing, because calling exposed traced API function internally
			status = rprMaterialSystemCreateNode(in_matsys, RPR_MATERIAL_NODE_TRANSPARENT, &transparency);
			s_logger.ContinueTracing();

			if (status != RPR_SUCCESS)
			{
				throw FrException(__FILE__,__LINE__,status, "",ctx);
			}

			newMat.emplace(std::make_pair(StandardMaterialComponent::Transparency, (rpr_material_node)transparency));

			// Create blends
			rpr_material_node diffuse2refraction = nullptr;

			s_logger.PauseTracing(); // pause tracing, because calling exposed traced API function internally
			status = rprMaterialSystemCreateNode(in_matsys, RPR_MATERIAL_NODE_BLEND, &diffuse2refraction);
			s_logger.ContinueTracing();

			if (status != RPR_SUCCESS)
			{
				throw FrException(__FILE__,__LINE__,status, "",ctx);
			}

			newMat.emplace(std::make_pair(StandardMaterialComponent::Diffuse2Refraction, (rpr_material_node)diffuse2refraction));

			rpr_material_node glossy2diffuse = nullptr;

			s_logger.PauseTracing(); // pause tracing, because calling exposed traced API function internally
			status = rprMaterialSystemCreateNode(in_matsys, RPR_MATERIAL_NODE_BLEND, &glossy2diffuse);
			s_logger.ContinueTracing();

			if (status != RPR_SUCCESS)
			{
				throw FrException(__FILE__,__LINE__,status, "",ctx);
			}

			newMat.emplace(std::make_pair(StandardMaterialComponent::Glossy2Diffuse, (rpr_material_node)glossy2diffuse));

			rpr_material_node clearcoat2glossy = nullptr;

			s_logger.PauseTracing(); // pause tracing, because calling exposed traced API function internally
			status = rprMaterialSystemCreateNode(in_matsys, RPR_MATERIAL_NODE_BLEND, &clearcoat2glossy);
			s_logger.ContinueTracing();

			if (status != RPR_SUCCESS)
			{
				throw FrException(__FILE__,__LINE__,status, "",ctx);
			}

			newMat.emplace(std::make_pair(StandardMaterialComponent::Clearcoat2Glossy, (rpr_material_node)clearcoat2glossy));

			rpr_material_node transparency2clearcoat = nullptr;

			s_logger.PauseTracing(); // pause tracing, because calling exposed traced API function internally
			status = rprMaterialSystemCreateNode(in_matsys, RPR_MATERIAL_NODE_BLEND, &transparency2clearcoat);
			s_logger.ContinueTracing();

			if (status != RPR_SUCCESS)
			{
				throw FrException(__FILE__,__LINE__,status, "",ctx);
			}

			newMat.emplace(std::make_pair(StandardMaterialComponent::Transparent2Clearcoat, (rpr_material_node)transparency2clearcoat));

			s_logger.PauseTracing(); // pause tracing, because calling exposed traced API function internally
			rprMaterialNodeSetInputF(refraction, "color", 1.f, 1.f, 1.f, 1.f);
			rprMaterialNodeSetInputF(refraction, "ior", 1.3f, 1.3f, 1.3f, 1.3f);
			rprMaterialNodeSetInputF(refraction, "roughness", 0.5f, 0.5f, 0.5f, 1.f);

			rprMaterialNodeSetInputF(diffuse, "color", 0.7f, 0.7f, 0.7f, 1.f);
			rprMaterialNodeSetInputF(glossy, "color", 0.7f, 0.7f, 0.7f, 1.f);
			rprMaterialNodeSetInputF(glossy, "roughness_x", 0.1f, 0.1f, 0.1f, 1.f);
			rprMaterialNodeSetInputF(glossy, "roughness_y", 0.1f, 0.1f, 0.1f, 1.f);
			rprMaterialNodeSetInputF(clearcoat, "color", 0.7f, 0.7f, 0.7f, 1.f);

			rprMaterialNodeSetInputN(diffuse2refraction, "color0", refraction);
			rprMaterialNodeSetInputN(diffuse2refraction, "color1", diffuse);
			rprMaterialNodeSetInputF(diffuse2refraction, "weight", 1.f, 1.f, 1.f, 1.f);

			rprMaterialNodeSetInputN(glossy2diffuse, "color0", diffuse2refraction);
			rprMaterialNodeSetInputN(glossy2diffuse, "color1", glossy);
			rprMaterialNodeSetInputF(glossy2diffuse, "weight", 0.3f, 0.3f, 0.3f, 1.f);

			rprMaterialNodeSetInputN(clearcoat2glossy, "color0", glossy2diffuse);
			rprMaterialNodeSetInputN(clearcoat2glossy, "color1", clearcoat);
			rprMaterialNodeSetInputF(clearcoat2glossy, "weight", 0.1f, 0.1f, 0.1f, 1.f);

			rprMaterialNodeSetInputN(transparency2clearcoat, "color0", clearcoat2glossy);
			rprMaterialNodeSetInputN(transparency2clearcoat, "color1", transparency);
			rprMaterialNodeSetInputF(transparency2clearcoat, "weight", 0.0f, 0.0f, 0.0f, 1.0f);
			s_logger.ContinueTracing();

			(static_cast<FrNode*>(transparency2clearcoat))->SetProperty(FR_MATERIAL_NODE_IS_STD, true);
            (static_cast<FrNode*>(transparency2clearcoat))->AddProperty(FR_MATERIAL_STANDARD_MAP, newMat);

			*out_node = transparency2clearcoat;

			s_logger.Trace__NewFrObjectCreated(Logger::TRACE_ADDRESS_TYPE__MATERIALNODE, *out_node);
			s_logger.Trace__DeclareFRobject("rpr_material_node", "materialnode_0x", *out_node);
			s_logger.printTrace("status = ");
			TRACE__FUNCTION_OPEN
			s_logger.TraceArg__rpr_material_system(in_matsys); s_logger.TraceArg__COMMA();
			s_logger.TraceArg__rpr_material_node_type(in_type); s_logger.TraceArg__COMMA();
			s_logger.printTrace("&materialnode_0x%p", *out_node);
			s_logger.Trace__FunctionClose();
			s_logger.printTrace("status = rprObjectSetName (materialnode_0x%p,\"materialnode_0x%p\"); RPRTRACE_CHECK// added by tracing for debug\r\n",  *out_node ,  *out_node);


			return RPR_SUCCESS;
		}
	}
	catch (FrException& e)
	{
		TRACE__FUNCTION_FAILED(ctx); return e.GetErrorCode();
	}
}

rpr_int rprMaterialNodeGetInfo(rpr_material_node node, rpr_material_node_info info, size_t size, void* data, size_t* out_size)
{

	s_logger.printTrace("//"); //  ignore getInfo for the trace
	s_logger.printTrace("status = ");
	TRACE__FUNCTION_OPEN
	s_logger.Trace__FunctionClose();


	FrNode* matNode = static_cast<FrNode*>(node);

	try
	{
		CHECK_NOT_NULL(matNode);
		MACRO__CHECK_ARGUMENT_TYPE(node,Material);

		auto type = matNode->GetProperty<rpr_material_node_type>(RPR_MATERIAL_NODE_TYPE);
		bool isStd = matNode->GetProperty<bool>(FR_MATERIAL_NODE_IS_STD);

		if (type == RPR_MATERIAL_NODE_BLEND && isStd)
			type = RPR_MATERIAL_NODE_STANDARD;


		size_t requiredSize = 0;
		switch (info)
		{
		case RPR_MATERIAL_NODE_TYPE:
			requiredSize = sizeof(rpr_material_node_type);
			break;

		case RPR_MATERIAL_NODE_INPUT_COUNT:
			requiredSize = sizeof(size_t);
			break;

		case RPR_OBJECT_NAME:
		{
			std::string name = matNode->GetProperty<std::string>(RPR_OBJECT_NAME);
			requiredSize = name.size() + 1;
			break;
		}


		default:
			throw FrException(__FILE__,__LINE__,RPR_ERROR_INVALID_PARAMETER, "Invalid material node info requested",node);
		}

		if (data)
		{
			if (size >= requiredSize)
			{
				switch (info)
				{
				case RPR_MATERIAL_NODE_TYPE:
					*reinterpret_cast<rpr_material_node_type*>(data) = type;
					break;

				case RPR_OBJECT_NAME:
				{
					std::string name = matNode->GetProperty<std::string>(RPR_OBJECT_NAME);
					auto len = name.size();
					auto str = reinterpret_cast<rpr_char*>(data);
					std::copy(name.cbegin(), name.cend(), str);
					*(str + len) = 0;
					break;
				}

				// TODO: fix this ugly thing  <---  update:  this is less ugly now because using nbMaterialInputCount[]
				//                                           (before numbers of input was hard coded directly here)
				//                                           now it's not perfect, but it's better.
				case RPR_MATERIAL_NODE_INPUT_COUNT:
				{
					switch (type)
					{
					case RPR_MATERIAL_NODE_DIFFUSE:
					case RPR_MATERIAL_NODE_ORENNAYAR:
					case RPR_MATERIAL_NODE_MICROFACET:
					case RPR_MATERIAL_NODE_MICROFACET_BECKMANN:
					case RPR_MATERIAL_NODE_PHONG:
					case RPR_MATERIAL_NODE_REFLECTION:
					case RPR_MATERIAL_NODE_REFRACTION:
					case RPR_MATERIAL_NODE_MICROFACET_REFRACTION:
					case RPR_MATERIAL_NODE_TRANSPARENT:
					case RPR_MATERIAL_NODE_EMISSIVE:
					case RPR_MATERIAL_NODE_PASSTHROUGH:
					case RPR_MATERIAL_NODE_WARD:
					case RPR_MATERIAL_NODE_ADD:
					case RPR_MATERIAL_NODE_BLEND:
					case RPR_MATERIAL_NODE_ARITHMETIC:
					case RPR_MATERIAL_NODE_FRESNEL:
					case RPR_MATERIAL_NODE_NORMAL_MAP:
					case RPR_MATERIAL_NODE_UV_PROCEDURAL:
					case RPR_MATERIAL_NODE_UV_TRIPLANAR:
					case RPR_MATERIAL_NODE_BUMP_MAP:
					case RPR_MATERIAL_NODE_AO_MAP:
					case RPR_MATERIAL_NODE_IMAGE_TEXTURE:
					case RPR_MATERIAL_NODE_BUFFER_SAMPLER:
					case RPR_MATERIAL_NODE_NOISE2D_TEXTURE:
					case RPR_MATERIAL_NODE_DOT_TEXTURE:
					case RPR_MATERIAL_NODE_GRADIENT_TEXTURE:
					case RPR_MATERIAL_NODE_CHECKER_TEXTURE:
					case RPR_MATERIAL_NODE_CONSTANT_TEXTURE:
					case RPR_MATERIAL_NODE_INPUT_LOOKUP:
					case RPR_MATERIAL_NODE_BLEND_VALUE:
					case RPR_MATERIAL_NODE_FRESNEL_SCHLICK:
					case RPR_MATERIAL_NODE_DIFFUSE_REFRACTION:
					case RPR_MATERIAL_NODE_VOLUME:
					case RPR_MATERIAL_NODE_MICROFACET_ANISOTROPIC_REFLECTION:
					case RPR_MATERIAL_NODE_MICROFACET_ANISOTROPIC_REFRACTION:
					case RPR_MATERIAL_NODE_TWOSIDED:
						*reinterpret_cast<size_t*>(data) = nbMaterialInputCount[type];
						break;
					
					case RPR_MATERIAL_NODE_STANDARD:
						*reinterpret_cast<size_t*>(data) = StandardMaterialParameterMap.size();
						break;

					default:
						throw FrException(__FILE__,__LINE__,RPR_ERROR_INVALID_PARAMETER, "",matNode);

						break;
					}
					break;
				}
				default:
				{
					throw FrException(__FILE__,__LINE__,RPR_ERROR_INVALID_PARAMETER, "",matNode);
					break;
				}
				}
			}
			else
			{
				throw FrException(__FILE__,__LINE__,RPR_ERROR_INVALID_PARAMETER, "",matNode);
			}
		}

		if (out_size)
			*out_size = requiredSize;
	}
	catch (FrException& e)
	{
		TRACE__FUNCTION_FAILED(matNode); return e.GetErrorCode();
	}
	catch (FireSG::property_not_found_error&)
	{
		return RPR_ERROR_INVALID_PARAMETER;
	}
	catch (FireSG::property_type_error&)
	{
		return RPR_ERROR_INVALID_PARAMETER_TYPE;
	}

	return RPR_SUCCESS;
}

#ifndef RPR_FOR_BAIKAL
rpr_int rprMaterialNodeSetInputNByKey(rpr_material_node in_node, rpr_material_node_input in_input, rpr_material_node in_input_node)
{
	//note: don't generate trace here, function not exposed - but used internally.

	API_PRINT("rprMaterialSystemSetNodeInputN", 1, in_node);
	FrNode* matNode = static_cast<FrNode*>(in_node);

	rpr_material_node_type type = matNode->GetProperty<rpr_material_node_type>(RPR_MATERIAL_NODE_TYPE);
	bool isStd = matNode->GetProperty<bool>(FR_MATERIAL_NODE_IS_STD);

	try
	{
		CHECK_NOT_NULL(matNode);
		MACRO__CHECK_ARGUMENT_TYPE(in_node,Material);


		if (!isStd || (isStd && in_input == RPR_MATERIAL_INPUT_WEIGHT))
		{
			FrNode* context = matNode->GetProperty<FrNode*>(FR_NODE_CONTEXT);
			std::shared_ptr<FrSceneGraph> sg = context->GetProperty< std::shared_ptr<FrSceneGraph> >(FR_SCENEGRAPH);

			FrAutoreleasePool& autoreleasePool = matNode->GetProperty<FrAutoreleasePool>(FR_MATERIAL_AUTORELEASE_POOL);

			FrNode* oldProp = matNode->GetProperty<FrNode*>(in_input);

            if (oldProp)
            {
                oldProp->SetProperty<FrNode*>(FR_NODE_GRAPH_MATERIAL_PARENT, nullptr);
            }

			if (oldProp && autoreleasePool.find(oldProp) != autoreleasePool.cend())
			{
				sg->DeleteNode(oldProp);
				autoreleasePool.erase(oldProp);
			}

			matNode->SetProperty(in_input, static_cast<FrNode*>(in_input_node));

            if (in_input_node)
            {
                static_cast<FrNode*>(in_input_node)->SetProperty<FrNode*>(FR_NODE_GRAPH_MATERIAL_PARENT, static_cast<FrNode*>(in_node));
            }
		}
		else
		{
			auto& stdMatMap = matNode->GetProperty<StandardMaterial>(FR_MATERIAL_STANDARD_MAP);

			auto param = StandardMaterialParameterMap[in_input];

			rpr_int retCode = rprMaterialNodeSetInputNByKey(stdMatMap[param.comp], param.input, in_input_node);
			
			if ( retCode != RPR_SUCCESS )
				throw FrException(__FILE__,__LINE__,retCode, "",matNode);
			else
				return retCode;

		}
	}
	catch (FrException& e)
	{
		TRACE__FUNCTION_FAILED(matNode); return e.GetErrorCode();
	}
	catch (FireSG::property_not_found_error&)
	{
		return RPR_ERROR_INVALID_PARAMETER;
	}
	catch (FireSG::property_type_error&)
	{
		return RPR_ERROR_INVALID_PARAMETER_TYPE;
	}

	return RPR_SUCCESS;
}
#endif

rpr_int rprMaterialNodeSetInputN(rpr_material_node in_node, const rpr_char* in_input, rpr_material_node in_input_node)
{
	s_logger.printTrace("status = ");
	TRACE__FUNCTION_OPEN
	s_logger.TraceArg__rpr_material_node(in_node); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_char_P(in_input); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_material_node(in_input_node);
	s_logger.Trace__FunctionClose();

	API_PRINT("rprMaterialSystemSetNodeInputN", 1, in_node);

	FrNode* nodeInRPR = static_cast<FrNode*>(in_node);
	FrNode* nodeInRPRinput = static_cast<FrNode*>(in_input_node);
	try
	{
		CHECK_NOT_NULL(nodeInRPR);
		MACRO__CHECK_ARGUMENT_TYPE(in_node,Material);
		MACRO__CHECK_ARGUMENT_TYPE(in_input_node,Material);

		auto itr = MaterialNodeInputKeys.find(in_input);
		if (itr != MaterialNodeInputKeys.end())
		{
			#ifndef RPR_FOR_BAIKAL
			rpr_int retCode = rprMaterialNodeSetInputNByKey(in_node, itr->second, in_input_node);
			if ( retCode != RPR_SUCCESS )
				throw FrException(__FILE__,__LINE__,retCode, "",nodeInRPR);
			else
				return retCode;
			#else
			std::shared_ptr<MaterialNode> previousState = nodeInRPR->GetProperty<std::shared_ptr<MaterialNode>>(itr->second);
			previousState->SetFrNode(nodeInRPRinput);
			nodeInRPR->PropertyChanged(itr->second);
			return RPR_SUCCESS;
			#endif
		}
		else
		{
			throw FrException(__FILE__,__LINE__,RPR_ERROR_INVALID_PARAMETER, "",nodeInRPR);
		}
	}
	catch (FrException& e)
	{
		TRACE__FUNCTION_FAILED(in_node); return e.GetErrorCode();
	}
}

#ifndef RPR_FOR_BAIKAL
rpr_int rprMaterialNodeSetInputFByKey(rpr_material_node in_node, rpr_material_node_input in_input, rpr_float in_value_x, rpr_float in_value_y, rpr_float in_value_z, rpr_float in_value_w)
{
	//note: don't generate trace here, function not exposed - but used internally.

	API_PRINT("rprMaterialSystemSetNodeInputF", 1, in_node);
	FrNode* matNode = static_cast<FrNode*>(in_node);

	try
	{
		CHECK_NOT_NULL(matNode);
		MACRO__CHECK_ARGUMENT_TYPE(in_node,Material);

			// Don't create a new node if the current node is a CONSTANT_TEXTURE node
			rpr_material_node_type type = matNode->GetProperty<rpr_material_node_type>(RPR_MATERIAL_NODE_TYPE);
			if (type == RPR_MATERIAL_NODE_CONSTANT_TEXTURE)
			{
				matNode->SetProperty <rpr_float4>(in_input, rpr_float4(in_value_x, in_value_y, in_value_z, in_value_w));
				return RPR_SUCCESS;
			}

			FrNode* matSys = matNode->GetProperty<FrNode*>(RPR_MATERIAL_NODE_SYSTEM);
			FrNode* ctx = matNode->GetProperty<FrNode*>(FR_NODE_CONTEXT);
			std::shared_ptr<FrSceneGraph> sg = ctx->GetProperty< std::shared_ptr<FrSceneGraph> >(FR_SCENEGRAPH);
			FrAutoreleasePool& autoreleasePool = matNode->GetProperty<FrAutoreleasePool>(FR_MATERIAL_AUTORELEASE_POOL);
			std::shared_ptr<FrRendererEncalps> renderer = ctx->GetProperty< std::shared_ptr<FrRendererEncalps> >(RPR_CONTEXT_ACTIVE_PLUGIN);

			FrNode* tex = sg->CreateNode(NodeTypes::Material, [=](FrNode* n) {
				n->AddProperty <rpr_float4>(RPR_MATERIAL_INPUT_VALUE, rpr_float4(in_value_x, in_value_y, in_value_z, in_value_w));

				n->SetProperty(FR_NODE_CONTEXT, ctx);
				n->SetProperty(RPR_MATERIAL_NODE_TYPE, static_cast<rpr_material_node_type>(RPR_MATERIAL_NODE_CONSTANT_TEXTURE));
				n->SetProperty(RPR_MATERIAL_NODE_SYSTEM, matSys);
				n->AddProperty(FR_SCENEGRAPH, sg);
				n->AddProperty(RPR_CONTEXT_ACTIVE_PLUGIN, renderer);
				});

			autoreleasePool.insert(tex);

			rpr_int retCode = rprMaterialNodeSetInputNByKey(in_node, in_input, tex);
			if ( retCode != RPR_SUCCESS )
				throw FrException(__FILE__,__LINE__,retCode, "",matNode);
			else
				return retCode;

	}
	catch (FrException& e)
	{
		TRACE__FUNCTION_FAILED(matNode); return e.GetErrorCode();
	}
	catch (FireSG::property_not_found_error&)
	{
		return RPR_ERROR_INVALID_PARAMETER;
	}
	catch (FireSG::property_type_error&)
	{
		return RPR_ERROR_INVALID_PARAMETER_TYPE;
	}

	return RPR_SUCCESS;
}
#endif

rpr_int rprMaterialNodeSetInputF(rpr_material_node in_node, const rpr_char* in_input, rpr_float in_value_x, rpr_float in_value_y, rpr_float in_value_z, rpr_float in_value_w)
{
	s_logger.printTrace("status = ");
	TRACE__FUNCTION_OPEN
	s_logger.TraceArg__rpr_material_node(in_node); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_char_P(in_input); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_float(in_value_x); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_float(in_value_y); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_float(in_value_z); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_float(in_value_w);
	s_logger.Trace__FunctionClose();

	API_PRINT("rprMaterialNodeSetInputF", 1, in_node);

	FrNode* nodeInRPR = static_cast<FrNode*>(in_node);

	try
	{
		CHECK_NOT_NULL(nodeInRPR);
		MACRO__CHECK_ARGUMENT_TYPE(in_node,Material);


		auto itr = MaterialNodeInputKeys.find(in_input);
		if (itr != MaterialNodeInputKeys.end())
		{
			#ifndef RPR_FOR_BAIKAL
			rpr_int retCode = rprMaterialNodeSetInputFByKey(in_node, itr->second, in_value_x, in_value_y, in_value_z, in_value_w);
			if ( retCode != RPR_SUCCESS )
				throw FrException(__FILE__,__LINE__,retCode, "",nodeInRPR);
			else
				return retCode;
			#else
			std::shared_ptr<MaterialNode> previousState = nodeInRPR->GetProperty<std::shared_ptr<MaterialNode>>(itr->second);
			previousState->SetFloat4(in_value_x, in_value_y, in_value_z, in_value_w);
			nodeInRPR->PropertyChanged(itr->second);
			return RPR_SUCCESS;
			#endif
		}
		else
		{
			throw FrException(__FILE__,__LINE__,RPR_ERROR_INVALID_PARAMETER, "",nodeInRPR);
		}

	}
	catch (FrException& e)
	{
		TRACE__FUNCTION_FAILED(in_node); return e.GetErrorCode();
	}
}

rpr_int rprMaterialNodeSetInputUByKey(rpr_material_node in_node, rpr_material_node_input in_input, rpr_uint in_value)
{
	//note: don't generate trace here, function not exposed - but used internally.

	API_PRINT("rprMaterialSystemSetNodeInputU", 1, in_node);
	FrNode* matNode = static_cast<FrNode*>(in_node);

	try
	{
		CHECK_NOT_NULL(matNode);
		MACRO__CHECK_ARGUMENT_TYPE(in_node,Material);

		#ifndef RPR_FOR_BAIKAL
		matNode->SetProperty(in_input, in_value);
		#else
		std::shared_ptr<MaterialNode> previousState = matNode->GetProperty<std::shared_ptr<MaterialNode>>(in_input);
		previousState->SetUint1(in_value);
		matNode->PropertyChanged(in_input);
		#endif

	}
	catch (FrException& e)
	{
		TRACE__FUNCTION_FAILED(matNode); return e.GetErrorCode();
	}
	catch (FireSG::property_not_found_error&)
	{
		return RPR_ERROR_INVALID_PARAMETER;
	}
	catch (FireSG::property_type_error&)
	{
		return RPR_ERROR_INVALID_PARAMETER_TYPE;
	}

	return RPR_SUCCESS;
}

extern RPR_API_ENTRY rpr_int rprMaterialNodeSetInputU(rpr_material_node in_node, const rpr_char* in_input, rpr_uint in_value)
{
	s_logger.printTrace("status = ");
	TRACE__FUNCTION_OPEN
	s_logger.TraceArg__rpr_material_node(in_node); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_char_P(in_input); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_uint(in_value);
	s_logger.Trace__FunctionClose();

	API_PRINT("rprMaterialNodeSetInputU", 1, in_node);

	FrNode* nodeInRPR = static_cast<FrNode*>(in_node);

	try
	{
		CHECK_NOT_NULL(nodeInRPR);
		MACRO__CHECK_ARGUMENT_TYPE(in_node,Material);

		auto itr = MaterialNodeInputKeys.find(in_input);
		if (itr != MaterialNodeInputKeys.end())
		{
			rpr_int retCode = rprMaterialNodeSetInputUByKey(in_node, itr->second, in_value);
			if ( retCode != RPR_SUCCESS )
				throw FrException(__FILE__,__LINE__,retCode, "",nodeInRPR);
			else
				return retCode;
		}
		else
		{
			throw FrException(__FILE__,__LINE__,RPR_ERROR_INVALID_PARAMETER, "",nodeInRPR);
		}
	}
	catch (FrException& e)
	{
		TRACE__FUNCTION_FAILED(in_node); return e.GetErrorCode();
	}

}
rpr_int rprMaterialNodeSetInputImageDataByKey(rpr_material_node in_node, rpr_material_node_input in_input, rpr_image in_image)
{
	//note: don't generate trace here, function not exposed - but used internally.

	API_PRINT("rprMaterialSystemSetNodeInputImageData", 1, in_node);
	FrNode* matNode = static_cast<FrNode*>(in_node);

	try
	{
		CHECK_NOT_NULL(matNode);
		MACRO__CHECK_ARGUMENT_TYPE(in_node,Material);
		MACRO__CHECK_ARGUMENT_TYPE(in_image,Image);

		#ifndef RPR_FOR_BAIKAL
		matNode->SetProperty(in_input, static_cast<FrNode*>(in_image));
		#else
		std::shared_ptr<MaterialNode> previousState = matNode->GetProperty<std::shared_ptr<MaterialNode>>(in_input);
		previousState->SetFrNode(static_cast<FrNode*>(in_image));
		matNode->PropertyChanged(in_input);
		#endif
	}
	catch (FrException& e)
	{
		TRACE__FUNCTION_FAILED(matNode); return e.GetErrorCode();
	}
	catch (FireSG::property_not_found_error&)
	{
		return RPR_ERROR_INVALID_PARAMETER;
	}
	catch (FireSG::property_type_error&)
	{
		return RPR_ERROR_INVALID_PARAMETER_TYPE;
	}

	return RPR_SUCCESS;
}

rpr_int rprMaterialNodeSetInputBufferDataByKey(rpr_material_node in_node, rpr_material_node_input in_input, rpr_buffer in_buffer)
{
	//note: don't generate trace here, function not exposed - but used internally.

	API_PRINT("rprMaterialSystemSetNodeInputBufferData", 1, in_node);
	FrNode* matNode = static_cast<FrNode*>(in_node);

	try
	{
		CHECK_NOT_NULL(matNode);
		MACRO__CHECK_ARGUMENT_TYPE(in_node,Material);
		MACRO__CHECK_ARGUMENT_TYPE(in_buffer,Buffer);

		matNode->SetProperty(in_input, static_cast<FrNode*>(in_buffer));
	}
	catch (FrException& e)
	{
		TRACE__FUNCTION_FAILED(matNode); return e.GetErrorCode();
	}
	catch (FireSG::property_not_found_error&)
	{
		return RPR_ERROR_INVALID_PARAMETER;
	}
	catch (FireSG::property_type_error&)
	{
		return RPR_ERROR_INVALID_PARAMETER_TYPE;
	}

	return RPR_SUCCESS;
}

rpr_int rprMaterialNodeSetInputImageData(rpr_material_node in_node, const rpr_char* in_input, rpr_image image)
{
	s_logger.printTrace("status = ");
	TRACE__FUNCTION_OPEN
	s_logger.TraceArg__rpr_material_node(in_node); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_char_P(in_input); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_image(image);
	s_logger.Trace__FunctionClose();

	API_PRINT("rprMaterialNodeSetInputImageData", 1, in_node);

	FrNode* nodeInRPR = static_cast<FrNode*>(in_node);

	try
	{
		CHECK_NOT_NULL(nodeInRPR);
		MACRO__CHECK_ARGUMENT_TYPE(in_node,Material);
		MACRO__CHECK_ARGUMENT_TYPE(image,Image);

		auto itr = MaterialNodeInputKeys.find(in_input);
		if (itr != MaterialNodeInputKeys.end())
		{
			rpr_int retCode = rprMaterialNodeSetInputImageDataByKey(in_node, itr->second, image);
			if ( retCode != RPR_SUCCESS )
				throw FrException(__FILE__,__LINE__,retCode, "",nodeInRPR);
			else
				return retCode;
		}
		else
		{
			throw FrException(__FILE__,__LINE__,RPR_ERROR_INVALID_PARAMETER, "",nodeInRPR);
		}
	}
	catch (FrException& e)
	{
		TRACE__FUNCTION_FAILED(in_node); return e.GetErrorCode();
	}

}

rpr_int rprMaterialNodeSetInputBufferData(rpr_material_node in_node, const rpr_char* in_input, rpr_buffer buffer)
{
	s_logger.printTrace("status = ");
	TRACE__FUNCTION_OPEN
	s_logger.TraceArg__rpr_material_node(in_node); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_char_P(in_input); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_buffer(buffer);
	s_logger.Trace__FunctionClose();

	API_PRINT("rprMaterialNodeSetInputBufferData", 1, in_node);

	FrNode* nodeInRPR = static_cast<FrNode*>(in_node);

	try
	{
		CHECK_NOT_NULL(nodeInRPR);
		MACRO__CHECK_ARGUMENT_TYPE(in_node,Material);
		MACRO__CHECK_ARGUMENT_TYPE(buffer,Buffer);

		auto itr = MaterialNodeInputKeys.find(in_input);
		if (itr != MaterialNodeInputKeys.end())
		{
			rpr_int retCode = rprMaterialNodeSetInputBufferDataByKey(in_node, itr->second, buffer);
			if ( retCode != RPR_SUCCESS )
				throw FrException(__FILE__,__LINE__,retCode, "",nodeInRPR);
			else
				return retCode;
		}
		else
		{
			throw FrException(__FILE__,__LINE__,RPR_ERROR_INVALID_PARAMETER, "",nodeInRPR);
		}
	}
	catch (FrException& e)
	{
		TRACE__FUNCTION_FAILED(in_node); return e.GetErrorCode();
	}

}


rpr_int rprMaterialNodeGetInputInfo(rpr_material_node node, rpr_int input_idx, rpr_material_node_input_info info, size_t size, void* data, size_t* out_size)
{
	s_logger.printTrace("//"); //  ignore getInfo for the trace
	TRACE__FUNCTION_OPEN;
	s_logger.Trace__FunctionClose();

	FrNode* n = static_cast<FrNode*>(node);

	rpr_material_node_type type = n->GetProperty<rpr_material_node_type>(RPR_MATERIAL_NODE_TYPE);
	bool isStd = n->GetProperty<bool>(FR_MATERIAL_NODE_IS_STD);

	//note : need to to maintain the lowest and highest value of RPR_MATERIAL_INPUT_* here
	const uint32_t materialInput_lowest = RPR_MATERIAL_INPUT_COLOR;
	const uint32_t materialInput_highest = RPR_MATERIAL_INPUT_MAX-1;

	try
	{
		CHECK_NOT_NULL(n);

		MACRO__CHECK_ARGUMENT_TYPE(node,Material);


		if (type == RPR_MATERIAL_NODE_BLEND && isStd)
		{
			// Need to route to the correct component

			auto iter = StandardMaterialParameterMap.cbegin();

			std::advance(iter, input_idx);

			auto inputDesc = iter->second;

			//if the
			if ( inputDesc.comp == StandardMaterialComponent::Transparent2Clearcoat && info != RPR_MATERIAL_NODE_INPUT_NAME_STRING )
			{

				// Find the property at the specified input index.
				bool found = false;
				int absolutePropIndex = 0;
				int relativePropIndex = 0;
				for (absolutePropIndex = 0; absolutePropIndex < n->GetNumProperties(); ++absolutePropIndex)
				{
					uint32_t key = n->GetPropertyKey(absolutePropIndex);

					if (key >= materialInput_lowest && key <= materialInput_highest)
					{
						if ( key == inputDesc.input )
						{
							input_idx = relativePropIndex;
							found = true;
							break;
						}
						++relativePropIndex;
					}
				}
				if ( !found )
				{
					throw FrException(__FILE__,__LINE__,RPR_ERROR_INTERNAL_ERROR, "",n);
				}

			}


			else
			{

				if (info == RPR_MATERIAL_NODE_INPUT_NAME_STRING)
				{

					auto strName = MaterialNodeInputStrings[iter->first];
					auto sizeNeeded = strName.size() + 1;
					if (out_size) *out_size = sizeNeeded;

					if (size >= sizeNeeded)
					{
						std::copy(strName.cbegin(), strName.cend(), (char*)data);
						*((char*)data + sizeNeeded - 1) = 0;
					}

					return RPR_SUCCESS;
				}

				auto& stdMatMap = static_cast<FrNode*>(node)->GetProperty<StandardMaterial>(FR_MATERIAL_STANDARD_MAP);

				auto comp = inputDesc.comp;
				auto input = inputDesc.input;

				auto instance = stdMatMap[comp];

				size_t numInputs = 0;
				s_logger.PauseTracing(); // pause tracing, because calling exposed traced API function internally
				rpr_int status = rprMaterialNodeGetInfo(instance, RPR_MATERIAL_NODE_INPUT_COUNT, sizeof(size_t), &numInputs, nullptr);
				s_logger.ContinueTracing();

				if (status != RPR_SUCCESS)
				{
					throw FrException(__FILE__,__LINE__,RPR_ERROR_INTERNAL_ERROR, "",n);
				}

				for (auto i = 0; i < numInputs; ++i)
				{
					rpr_material_node_input inputKey = 0;
					s_logger.PauseTracing(); // pause tracing, because calling exposed traced API function internally
					status = rprMaterialNodeGetInputInfo(instance, i, RPR_MATERIAL_NODE_INPUT_NAME, sizeof(rpr_material_node_input), &inputKey, nullptr);
					s_logger.ContinueTracing();

					if (status != RPR_SUCCESS)
					{
						throw FrException(__FILE__,__LINE__,RPR_ERROR_INTERNAL_ERROR, "",n);
					}

					if (inputKey == input)
					{
						s_logger.PauseTracing(); // pause tracing, because calling exposed traced API function internally
						rpr_int returnnum = rprMaterialNodeGetInputInfo(instance, i, info, size, data, out_size);
						s_logger.ContinueTracing();

						if ( returnnum != RPR_SUCCESS )
							throw FrException(__FILE__,__LINE__,returnnum, "",n);
						else
							return returnnum;
						
					}
				}

				throw FrException(__FILE__,__LINE__,RPR_ERROR_INTERNAL_ERROR, "",n);

			}
		}

		// Find the property at the specified input index.
		int absolutePropIndex = 0;
		int relativePropIndex = 0;
		for (absolutePropIndex = 0; absolutePropIndex < n->GetNumProperties(); ++absolutePropIndex)
		{
			uint32_t key = n->GetPropertyKey(absolutePropIndex);
			if (key >= materialInput_lowest && key <= materialInput_highest)
			{
				if (relativePropIndex == input_idx)
					break;
				++relativePropIndex;
			}
		}

		// Check bounds.
		if (relativePropIndex != input_idx)
			throw FrException(__FILE__,__LINE__,RPR_ERROR_INVALID_PARAMETER, "Invalid material node info requested",node);

		size_t requiredSize = 0;
		switch (info)
		{
		case RPR_MATERIAL_NODE_INPUT_NAME:
			requiredSize = sizeof(rpr_material_node_input);
			break;
		case RPR_MATERIAL_NODE_INPUT_NAME_STRING:
			requiredSize = MaterialNodeInputStrings[n->GetPropertyKey(absolutePropIndex)].size() + 1;
			break;

		case RPR_MATERIAL_NODE_INPUT_VALUE:
		{
			uint32_t key = n->GetPropertyKey(absolutePropIndex);
			auto matNodeType = n->GetProperty<rpr_material_node_type>(RPR_MATERIAL_NODE_TYPE);

			//special case #1: type is an UINT
			if (   matNodeType == RPR_MATERIAL_NODE_ADD && key == RPR_MATERIAL_INPUT_OP // <- INPUT_OP doesn't exist anymore for NODE_ADD ?
				|| matNodeType == RPR_MATERIAL_NODE_ARITHMETIC && key == RPR_MATERIAL_INPUT_OP
				|| matNodeType == RPR_MATERIAL_NODE_INPUT_LOOKUP && key == RPR_MATERIAL_INPUT_VALUE)
			{
				auto value = n->GetProperty<uint>(key); // just for debug, we check that no exception are raised when trying to get an uint from key
				requiredSize = sizeof(rpr_uint);
			}

			//special case #2: type is an IMAGE or a BUFFER (has the same size)
			else if ((matNodeType == RPR_MATERIAL_NODE_IMAGE_TEXTURE 
				|| matNodeType == RPR_MATERIAL_NODE_BUFFER_SAMPLER 
				|| matNodeType == RPR_MATERIAL_NODE_NORMAL_MAP 
				|| matNodeType == RPR_MATERIAL_NODE_BUMP_MAP 
				//|| matNodeType == RPR_MATERIAL_NODE_AO_MAP  // no input data for AO map
				) && key == RPR_MATERIAL_INPUT_DATA)
			{
				auto value = n->GetProperty<FrNode*>(key); // just for debug, we check that no exception are raised when trying to get an FrNode from key
				requiredSize = sizeof(rpr_image);
			}
			//all other cases:
			else
			{
				auto value = n->GetProperty<FrNode*>(key);

				if (!value)
				{
					requiredSize = sizeof(std::nullptr_t);
				}
				else
				{
					auto matNodeTypeValue = value->GetProperty<rpr_material_node_type>(RPR_MATERIAL_NODE_TYPE);

					if (matNodeTypeValue == RPR_MATERIAL_NODE_CONSTANT_TEXTURE)
					{
						requiredSize = sizeof(rpr_float4);
					}
					else
					{
						requiredSize = sizeof(FrNode*);
					}
				}
			}
			break;
		}

		case RPR_MATERIAL_NODE_INPUT_TYPE:
			requiredSize = sizeof(rpr_uint);
			break;

		case RPR_MATERIAL_NODE_INPUT_DESCRIPTION:
		default:
			throw FrException(__FILE__,__LINE__,RPR_ERROR_INVALID_PARAMETER, "Invalid material node info requested",node);
		}

		if (data)
		{
			if (size >= requiredSize)
			{
				switch (info)
				{
				case RPR_MATERIAL_NODE_INPUT_NAME:
					*reinterpret_cast<rpr_material_node_input*>(data) = n->GetPropertyKey(absolutePropIndex);
					break;

				case RPR_MATERIAL_NODE_INPUT_NAME_STRING:
				{
					std::string str = MaterialNodeInputStrings[n->GetPropertyKey(absolutePropIndex)];
					memcpy(data, str.c_str(), str.length());
					reinterpret_cast<char*>(data)[str.length()] = '\0';
					break;
				}

				case RPR_MATERIAL_NODE_INPUT_TYPE:
				{
					rpr_uint* type = reinterpret_cast<rpr_uint*>(data);

					uint32_t key = n->GetPropertyKey(absolutePropIndex);
					auto matNodeType = n->GetProperty<rpr_material_node_type>(RPR_MATERIAL_NODE_TYPE);

					//special case #1: type is an UINT
					if (   matNodeType == RPR_MATERIAL_NODE_ADD && key == RPR_MATERIAL_INPUT_OP // <- INPUT_OP doesn't exist anymore for NODE_ADD ?
						|| matNodeType == RPR_MATERIAL_NODE_ARITHMETIC && key == RPR_MATERIAL_INPUT_OP
						|| matNodeType == RPR_MATERIAL_NODE_INPUT_LOOKUP && key == RPR_MATERIAL_INPUT_VALUE)
					{
						auto value = n->GetProperty<uint>(key); // just for debug, we check that no exception are raised when trying to get an uint from key
						*type = RPR_MATERIAL_NODE_INPUT_TYPE_UINT;
					}

					else if( (matNodeType == RPR_MATERIAL_NODE_CONSTANT_TEXTURE && key == RPR_MATERIAL_INPUT_VALUE ) )
					{
						*type = RPR_MATERIAL_NODE_INPUT_TYPE_FLOAT4;
					}

					//all other cases:
					else
					{
						auto value = n->GetProperty<FrNode*>(key);

						if (!value)
						{
							*type = RPR_MATERIAL_NODE_INPUT_TYPE_NODE;
						}
						else if ( value->GetType() == NodeTypes::Image )
						{
							*type = RPR_MATERIAL_NODE_INPUT_TYPE_IMAGE;
						}
						else if ( value->GetType() == NodeTypes::Buffer )
						{
							*type = RPR_MATERIAL_NODE_INPUT_TYPE_BUFFER;
						}
						else if ( value->GetType() == NodeTypes::Material )
						{
							auto matNodeTypeValue = value->GetProperty<rpr_material_node_type>(RPR_MATERIAL_NODE_TYPE);

							if (matNodeTypeValue == RPR_MATERIAL_NODE_CONSTANT_TEXTURE)
							{
								*type = RPR_MATERIAL_NODE_INPUT_TYPE_FLOAT4;
							}
							else
							{
								*type = RPR_MATERIAL_NODE_INPUT_TYPE_NODE;
							}
						}
						else
						{
							throw FrException(__FILE__,__LINE__,RPR_ERROR_INVALID_PARAMETER, "Invalid value type",n);
						}
					}
					break;
				}

				case RPR_MATERIAL_NODE_INPUT_VALUE:
				{
					uint32_t key = n->GetPropertyKey(absolutePropIndex);
					auto matNodeType = n->GetProperty<rpr_material_node_type>(RPR_MATERIAL_NODE_TYPE);

					FrNode* matsys = n->GetProperty<FrNode*>(RPR_MATERIAL_NODE_SYSTEM);
					FrNode* ctx = n->GetProperty<FrNode*>(FR_NODE_CONTEXT);
					std::shared_ptr<FrRendererEncalps> renderer = ctx->GetProperty<  std::shared_ptr<FrRendererEncalps>  >(RPR_CONTEXT_ACTIVE_PLUGIN);

					//special case #1: type is an UINT
					if (   matNodeType == RPR_MATERIAL_NODE_ADD && key == RPR_MATERIAL_INPUT_OP  // <- INPUT_OP doesn't exist anymore for NODE_ADD ?
						|| matNodeType == RPR_MATERIAL_NODE_ARITHMETIC && key == RPR_MATERIAL_INPUT_OP
						|| matNodeType == RPR_MATERIAL_NODE_INPUT_LOOKUP && key == RPR_MATERIAL_INPUT_VALUE)
					{
						auto value = n->GetProperty<uint>(key); // just for debug, we check that no exception are raised when trying to get an uint from key
						*(reinterpret_cast<rpr_uint*>(data)) = renderer->m_FrRenderer->GetUint(n, key);
					}

					//special case #2: type is an IMAGE
					else if ((matNodeType == RPR_MATERIAL_NODE_IMAGE_TEXTURE 
						|| matNodeType == RPR_MATERIAL_NODE_NORMAL_MAP 
						|| matNodeType == RPR_MATERIAL_NODE_BUMP_MAP 
						//|| matNodeType == RPR_MATERIAL_NODE_AO_MAP  // no input data for AO map
						) && key == RPR_MATERIAL_INPUT_DATA)
					{
						auto value = n->GetProperty<FrNode*>(key);
						*(reinterpret_cast<rpr_image*>(data)) = value;
					}
					//special case #3: type is a BUFFER
					else if ((matNodeType == RPR_MATERIAL_NODE_BUFFER_SAMPLER) && key == RPR_MATERIAL_INPUT_DATA)
					{
						auto value = n->GetProperty<FrNode*>(key);
						*(reinterpret_cast<rpr_buffer*>(data)) = value;
					}
					//all other cases:
					else
					{
						auto value = n->GetProperty<FrNode*>(key);

						if (!value)
						{
							memset(data,0,requiredSize); // if no value found, fill data with null
						}

						else
						{
							auto matNodeTypeValue = value->GetProperty<rpr_material_node_type>(RPR_MATERIAL_NODE_TYPE);

							if (matNodeTypeValue == RPR_MATERIAL_NODE_CONSTANT_TEXTURE)
							{
								FrNode* matsys = n->GetProperty<FrNode*>(RPR_MATERIAL_NODE_SYSTEM);
								FrNode* ctx = n->GetProperty<FrNode*>(FR_NODE_CONTEXT);
								std::shared_ptr<FrRendererEncalps> renderer = ctx->GetProperty<std::shared_ptr<FrRendererEncalps>>(RPR_CONTEXT_ACTIVE_PLUGIN);
								*(reinterpret_cast<rpr_float4*>(data)) = renderer->m_FrRenderer->GetFloat4(value, RPR_MATERIAL_INPUT_DATA);
							}
							else
							{
								*(reinterpret_cast<FrNode**>(data)) = value;
							}
						}
					}

					break;
				}
				default:
				{
					throw FrException(__FILE__,__LINE__,RPR_ERROR_INVALID_PARAMETER, "",n);
					break;
				}

				}
			}
			else
			{
				throw FrException(__FILE__,__LINE__,RPR_ERROR_INVALID_PARAMETER, "",n);
			}
		}

		if (out_size)
			*out_size = requiredSize;
	}
	catch (FrException& e)
	{
		TRACE__FUNCTION_FAILED(n); return e.GetErrorCode();
	}

	return RPR_SUCCESS;
}

rpr_int rprObjectDelete(void* obj)
{

	s_logger.printTrace("status = ");
	TRACE__FUNCTION_OPEN
	s_logger.TraceArg__rpr_undef(obj);
	s_logger.Trace__FunctionClose();
	if ( obj != NULL )
	{
		s_logger.TraceArg__rpr_undef(obj);
		s_logger.printTrace("=NULL;\r\n");
	}

	try
	{

		FrNode* node = static_cast<FrNode*>(obj);
		CHECK_NOT_NULL(node);

		auto sceneGraph = node->GetProperty< std::shared_ptr<FrSceneGraph> >(FR_SCENEGRAPH);

		if (node->GetType() == NodeTypes::Context)
		{
			//do not remove the callback, in case context is deleted first.
			//sceneGraph->ClearCallbacks();
		}
		else
		{
			if (node->GetType() == NodeTypes::Mesh)
			{
				// Mesh vertex, index and num faces arrays must be manually freed since they are copies
				// of the original arrays passed to FireRender by the client.
				DeleteMeshArrays(node);
			}

			else if (node->GetType() == NodeTypes::IESLight)
			{
				// IES Data must be manually freed since they are copies
				// of the original data passed to FireRender by the client.
				DeleteIesData(node);
			}
		}

		// Signal scene graph of node deletion.
		sceneGraph->DeleteNode(node);
	
	}
	catch (FrException& e)
	{
		TRACE__FUNCTION_FAILED(nullptr); return e.GetErrorCode();
	}

	return RPR_SUCCESS;
}

////////////////////////////////////////////////////////////////////////////////////////
rpr_int rprContextCreateIESLight(rpr_context context, rpr_light* out_light)
{
	//in case of issues during creation, it's safer to return a null pointer, rather than let the pointer undefined.
	if ( out_light )
		*out_light = nullptr;
	

	s_logger.printTrace("//IESLight creation\r\n");
	s_logger.Trace__FlushAllFiles(); // in case a crash, ensure trace informs the creation

	API_PRINT("rprContextCreateIESLight", 1, context);
	FrNode* ctx = static_cast<FrNode*>(context);

	try
	{
		CHECK_NOT_NULL(ctx);
		MACRO__CHECK_ARGUMENT_TYPE(context,Context);


		std::shared_ptr<FrSceneGraph> sceneGraph = ctx->GetProperty< std::shared_ptr<FrSceneGraph> >(FR_SCENEGRAPH);
		std::shared_ptr<FrRendererEncalps> renderer = ctx->GetProperty< std::shared_ptr<FrRendererEncalps> >(RPR_CONTEXT_ACTIVE_PLUGIN);
		FrNode* node = sceneGraph->CreateNode(NodeTypes::IESLight, [&](FrNode* n) {
			n->SetProperty(FR_NODE_CONTEXT, ctx);
            n->AddProperty(FR_SCENEGRAPH, sceneGraph);
			n->AddProperty(RPR_CONTEXT_ACTIVE_PLUGIN, renderer);
		});

		*out_light = node;

		s_logger.Trace__NewFrObjectCreated(Logger::TRACE_ADDRESS_TYPE__LIGHT, *out_light);
		s_logger.Trace__DeclareFRobject("rpr_light", "light_0x", *out_light);
		s_logger.printTrace("status = ");
		TRACE__FUNCTION_OPEN
		s_logger.TraceArg__rpr_context(context); s_logger.TraceArg__COMMA();
		s_logger.printTrace("&light_0x%p", *out_light);
		s_logger.Trace__FunctionClose();
		s_logger.printTrace("status = rprObjectSetName (light_0x%p,\"light_0x%p\"); RPRTRACE_CHECK// added by tracing for debug\r\n",  *out_light ,  *out_light);

		return RPR_SUCCESS;
	}
	catch (FrException& e)
	{
		TRACE__FUNCTION_FAILED(context); return e.GetErrorCode();
	}
}

////////////////////////////////////////////////////////////////////////////////////////
rpr_int      rprIESLightSetRadiantPower3f(rpr_light light, rpr_float r, rpr_float g, rpr_float b)
{
	s_logger.printTrace("status = ");
	TRACE__FUNCTION_OPEN
	s_logger.TraceArg__rpr_light(light); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_float(r); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_float(g); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_float(b);
	s_logger.Trace__FunctionClose();

	API_PRINT("rprIESLightSetRadiantPower3f", 1, light);
	FrNode* l = static_cast<FrNode*>(light);

	try
	{
		CHECK_NOT_NULL(l);
		MACRO__CHECK_ARGUMENT_TYPE(light,IESLight);

		MACRO__CHECK_ARGUMENT_FLOAT_NAN(r,light);
		MACRO__CHECK_ARGUMENT_FLOAT_NAN(g,light);
		MACRO__CHECK_ARGUMENT_FLOAT_NAN(b,light);

		l->SetProperty(RPR_IES_LIGHT_RADIANT_POWER, rpr_float3{ r,g,b });

		return RPR_SUCCESS;
	}
	catch (FrException& e)
	{
		TRACE__FUNCTION_FAILED(l); return e.GetErrorCode();
	}
}

////////////////////////////////////////////////////////////////////////////////////////
rpr_int rprIESLightSetImageFromFile(rpr_light env_light, const rpr_char* imagePath, rpr_int nx, rpr_int ny)
{
	s_logger.printTrace("status = ");
	TRACE__FUNCTION_OPEN
	s_logger.TraceArg__rpr_light(env_light); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_char_P(imagePath);
	s_logger.Trace__FunctionClose();

	API_PRINT("rprIESLightSetImageFromFile", 1, env_light);
	FrNode* l = static_cast<FrNode*>(env_light);

	try
	{
		CHECK_NOT_NULL(l);
		MACRO__CHECK_ARGUMENT_TYPE(env_light,IESLight);


		//alloc new data
		size_t strlen_imagePath = strlen(imagePath)+1;
		rpr_char* imagePath_saved = new rpr_char[strlen_imagePath];
		memcpy(imagePath_saved, imagePath, strlen_imagePath);

		//we store the content of the IES file because it's important to have
		// a strong getter able to give the full detail of IES, and not just a file name.
		std::ifstream t(imagePath);
		std::string iesFileData = std::string((std::istreambuf_iterator<char>(t)),std::istreambuf_iterator<char>());
		size_t strlen_imageData = iesFileData.length()+1;
		rpr_char* imageData = new rpr_char[strlen_imageData];
		memcpy(imageData, iesFileData.c_str(), strlen_imageData);

		//free previous property
		DeleteIesData(l);

		l->SetProperty(RPR_IES_LIGHT_IMAGE_DESC, rpr_ies_image_desc{ nx, ny, imageData, imagePath_saved});

		return RPR_SUCCESS;
	}
	catch (FrException& e)
	{
		TRACE__FUNCTION_FAILED(l); return e.GetErrorCode();
	}
}


////////////////////////////////////////////////////////////////////////////////////////
rpr_int      rprIESLightSetImageFromIESdata(rpr_light env_light, const rpr_char* iesData, rpr_int nx, rpr_int ny)
{
	s_logger.printTrace("status = ");
	TRACE__FUNCTION_OPEN
	s_logger.TraceArg__rpr_light(env_light); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_char_P(iesData); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_int(nx); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_int(ny);
	s_logger.Trace__FunctionClose();

	API_PRINT("rprIESLightSetImageFromIESdata", 1, env_light);
	FrNode* l = static_cast<FrNode*>(env_light);
	//FrImageWrapper* img = static_cast<FrImageWrapper*>(image);

	try
	{
		CHECK_NOT_NULL(l);
		MACRO__CHECK_ARGUMENT_TYPE(env_light,IESLight);


		//alloc new data
		size_t strlen_iesData = strlen(iesData)+1;
		rpr_char* iesData_saved = new rpr_char[strlen_iesData];
		memcpy(iesData_saved, iesData, strlen_iesData);

		//free previous property
		DeleteIesData(l);

		l->SetProperty(RPR_IES_LIGHT_IMAGE_DESC, rpr_ies_image_desc{ nx, ny, iesData_saved, nullptr});

		return RPR_SUCCESS;
	}
	catch (FrException& e)
	{
		TRACE__FUNCTION_FAILED(l); return e.GetErrorCode();
	}
}

rpr_int rprObjectSetName(void* obj, rpr_char const* name)
{
	s_logger.printTrace("status = ");
	TRACE__FUNCTION_OPEN
	s_logger.TraceArg__rpr_undef(obj); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_char_P(name);
	s_logger.Trace__FunctionClose();

	FrNode* node = static_cast<FrNode*>(obj);

	try
	{
		CHECK_NOT_NULL(node);
		node->SetProperty(RPR_OBJECT_NAME, std::string(name));
	}
	catch (FrException& e)
	{
		TRACE__FUNCTION_FAILED(node); return e.GetErrorCode();
	}
	return RPR_SUCCESS;
}

rpr_int rprContextCreatePostEffect(rpr_context context, rpr_post_effect_type in_type, rpr_post_effect* out_effect)
{
	//in case of issues during creation, it's safer to return a null pointer, rather than let the pointer undefined.
	if ( out_effect )
		*out_effect = nullptr;
	

	s_logger.printTrace("//PostEffect creation\r\n");
	s_logger.Trace__FlushAllFiles(); // in case a crash, ensure trace informs the creation


    API_PRINT("rprContextCreatePostEffect", 1, context);

    FrNode* ctx = static_cast<FrNode*>(context);

    try
    {
		CHECK_NOT_NULL(ctx);
		MACRO__CHECK_ARGUMENT_TYPE(context,Context);


            std::shared_ptr<FrSceneGraph> sceneGraph = ctx->GetProperty< std::shared_ptr<FrSceneGraph> >(FR_SCENEGRAPH);

            FrNode* node = sceneGraph->CreateNode(NodeTypes::PostEffect, [&](FrNode* n) {

                switch (in_type)
                {
                case RPR_POST_EFFECT_WHITE_BALANCE:
                    n->AddProperty<rpr_uint>(RPR_POST_EFFECT_WHITE_BALANCE_COLOR_SPACE, RPR_COLOR_SPACE_SRGB);
                    n->AddProperty<rpr_float>(RPR_POST_EFFECT_WHITE_BALANCE_COLOR_TEMPERATURE, TAHOE_POSTEFFECT_DEFAULT_WHITE_BALANCE_COLOR_TEMPERATURE );
                    break;

				case RPR_POST_EFFECT_SIMPLE_TONEMAP:
                    n->AddProperty<rpr_float>(RPR_POST_EFFECT_SIMPLE_TONEMAP_EXPOSURE, TAHOE_POSTEFFECT_DEFAULT_SIMPLE_TONEMAP_EXPOSURE);
                    n->AddProperty<rpr_float>(RPR_POST_EFFECT_SIMPLE_TONEMAP_CONTRAST, TAHOE_POSTEFFECT_DEFAULT_SIMPLE_TONEMAP_CONTRAST);
					n->AddProperty<rpr_uint>(RPR_POST_EFFECT_SIMPLE_TONEMAP_ENABLE_TONEMAP, 0);
					break;
				case RPR_POST_EFFECT_TONE_MAP:
				case RPR_POST_EFFECT_NORMALIZATION:
				case RPR_POST_EFFECT_GAMMA_CORRECTION:
					break;
                default:
                    throw FrException(__FILE__,__LINE__,RPR_ERROR_INVALID_PARAMETER, "Invalid material type",ctx);
                    break;
                }

                n->SetProperty(FR_NODE_CONTEXT, ctx);
                n->AddProperty(FR_SCENEGRAPH, sceneGraph);
                n->SetProperty(RPR_POST_EFFECT_TYPE, in_type);
            });

            *out_effect = node;

            s_logger.Trace__NewFrObjectCreated(Logger::TRACE_ADDRESS_TYPE__POSTEFFECT, *out_effect);
			s_logger.Trace__DeclareFRobject("rpr_post_effect", "posteffect_0x", *out_effect);
			s_logger.printTrace("status = ");
			TRACE__FUNCTION_OPEN
			s_logger.TraceArg__rpr_context(context); s_logger.TraceArg__COMMA();
			s_logger.TraceArg__rpr_post_effect_type(in_type); s_logger.TraceArg__COMMA();
			s_logger.printTrace("&posteffect_0x%p", *out_effect);
			s_logger.Trace__FunctionClose();
			s_logger.printTrace("status = rprObjectSetName (posteffect_0x%p,\"posteffect_0x%p\"); RPRTRACE_CHECK// added by tracing for debug\r\n",  *out_effect ,  *out_effect);

            return RPR_SUCCESS;

    }
    catch (FrException& e)
    {
        TRACE__FUNCTION_FAILED(context); return e.GetErrorCode();
    }

}

rpr_int rprContextAttachPostEffect(rpr_context context, rpr_post_effect effect)
{

	s_logger.printTrace("status = ");
	TRACE__FUNCTION_OPEN
	s_logger.TraceArg__rpr_context(context); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_post_effect(effect); 
	s_logger.Trace__FunctionClose();

    API_PRINT("rprContextAttachPostEffect", 1, context);

    FrNode* ctx = static_cast<FrNode*>(context);
    FrNode* eff = static_cast<FrNode*>(effect);

    try
    {
		CHECK_NOT_NULL(ctx);
		CHECK_NOT_NULL(eff);

		MACRO__CHECK_ARGUMENT_TYPE(context,Context);
		MACRO__CHECK_ARGUMENT_TYPE(effect,PostEffect);


        auto& postEffectList = ctx->GetProperty<FrPostEffectList>(FR_CONTEXT_POST_EFFECT_LIST);

        if (std::find(postEffectList.cbegin(), postEffectList.cend(), eff) == postEffectList.cend())
        {
            postEffectList.push_back(eff);

            FireSG::ListChangedArgs args(FireSG::ListChangedArgs::Op::ItemAdded, reinterpret_cast<void*>(eff));
            ctx->PropertyChanged(FR_CONTEXT_POST_EFFECT_LIST, reinterpret_cast<void*>(&args));

			//Because of the way Tahoe is done with Tonemap,
			//each time we attach a new post effect, we have to set again  "tonemapping.type" in order to ensure
			//all parameters are set correctly
			s_logger.PauseTracing(); // pause tracing, because calling exposed traced API function internally
			rpr_uint newType = ctx->GetProperty<rpr_uint>(RPR_CONTEXT_TONE_MAPPING_TYPE);
			rpr_int status = rprContextSetParameter1u(context, "tonemapping.type", newType);
			s_logger.ContinueTracing();

        }
        return RPR_SUCCESS;
    }
    catch (FrException& e)
    {
        TRACE__FUNCTION_FAILED(context); return e.GetErrorCode();
    }

    return RPR_SUCCESS;
}

rpr_int rprContextDetachPostEffect(rpr_context context, rpr_post_effect effect)
{
	s_logger.printTrace("status = ");
	TRACE__FUNCTION_OPEN
	s_logger.TraceArg__rpr_context(context); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_post_effect(effect); 
	s_logger.Trace__FunctionClose();

    API_PRINT("rprContextDetachPostEfffect", 1, context);

    FrNode* ctx = static_cast<FrNode*>(context);
    FrNode* eff = static_cast<FrNode*>(effect);

    try
    {
		CHECK_NOT_NULL(ctx);
		CHECK_NOT_NULL(eff);

		MACRO__CHECK_ARGUMENT_TYPE(context,Context);
		MACRO__CHECK_ARGUMENT_TYPE(effect,PostEffect);



        auto& postEffectList = ctx->GetProperty<FrPostEffectList>(FR_CONTEXT_POST_EFFECT_LIST);
        auto iter = std::find(postEffectList.cbegin(), postEffectList.cend(), eff);

        if ( iter != postEffectList.cend())
        {
            postEffectList.erase(iter);

            FireSG::ListChangedArgs args(FireSG::ListChangedArgs::Op::ItemRemoved, reinterpret_cast<void*>(eff));
            ctx->PropertyChanged(FR_CONTEXT_POST_EFFECT_LIST, reinterpret_cast<void*>(&args));
        }
        return RPR_SUCCESS;
    }
    catch (FrException& e)
    {
        TRACE__FUNCTION_FAILED(context); return e.GetErrorCode();
    }

    return RPR_SUCCESS;

}

rpr_int rprPostEffectSetParameterByKey1u(rpr_post_effect effect, rpr_post_effect_info in_info, rpr_uint value)
{
    //note: don't generate trace here, function not exposed - but used internally.

    API_PRINT("rprPostEffectSetParameterByKey1u", 1, effect);
    FrNode* eff = static_cast<FrNode*>(effect);

    try
    {
		CHECK_NOT_NULL(eff);

		MACRO__CHECK_ARGUMENT_TYPE(effect,PostEffect);


        eff->SetProperty(in_info, value);
        return RPR_SUCCESS;
    }
    catch (FrException& e)
    {
        TRACE__FUNCTION_FAILED(eff); return e.GetErrorCode();
    }
    catch (FireSG::property_not_found_error&)
    {
        return RPR_ERROR_INVALID_PARAMETER;
    }
    catch (FireSG::property_type_error&)
    {
        return RPR_ERROR_INVALID_PARAMETER_TYPE;
    }
}

rpr_int rprPostEffectSetParameterByKey1f(rpr_post_effect effect, rpr_post_effect_info in_info, rpr_float x)
{
    //note: don't generate trace here, function not exposed - but used internally.


    API_PRINT("rprPostEffectSetParameterByKey1f", 1, effect);
    FrNode* eff = static_cast<FrNode*>(effect);

    try
    {
		CHECK_NOT_NULL(eff);

		MACRO__CHECK_ARGUMENT_TYPE(effect,PostEffect);


        eff->SetProperty(in_info, x);
        return RPR_SUCCESS;
    }
    catch (FrException& e)
    {
        TRACE__FUNCTION_FAILED(eff); return e.GetErrorCode();
    }
    catch (FireSG::property_not_found_error&)
    {
        return RPR_ERROR_INVALID_PARAMETER;
    }
    catch (FireSG::property_type_error&)
    {
        return RPR_ERROR_INVALID_PARAMETER_TYPE;
    }
}

rpr_int rprPostEffectSetParameterByKey3f(rpr_post_effect effect, rpr_post_effect_info in_info, rpr_float x, rpr_float y, rpr_float z)
{
    //note: don't generate trace here, function not exposed - but used internally.

    API_PRINT("rprPostEffectSetParameterByKey1f", 1, effect);
    FrNode* eff = static_cast<FrNode*>(effect);

    try
    {
		CHECK_NOT_NULL(eff);

		MACRO__CHECK_ARGUMENT_TYPE(effect,PostEffect);


        eff->SetProperty(in_info, rpr_float3(x, y, z));
        return RPR_SUCCESS;
    }
    catch (FrException& e)
    {
        TRACE__FUNCTION_FAILED(eff); return e.GetErrorCode();
    }
    catch (FireSG::property_not_found_error&)
    {
        return RPR_ERROR_INVALID_PARAMETER;
    }
    catch (FireSG::property_type_error&)
    {
        return RPR_ERROR_INVALID_PARAMETER_TYPE;
    }
}

rpr_int rprPostEffectSetParameterByKey4f(rpr_post_effect effect, rpr_post_effect_info in_info, rpr_float x, rpr_float y, rpr_float z, rpr_float w)
{
    //note: don't generate trace here, function not exposed - but used internally.

    API_PRINT("rprPostEffectSetParameterByKey3f", 1, effect);
    FrNode* eff = static_cast<FrNode*>(effect);

    try
    {
		CHECK_NOT_NULL(eff);

		MACRO__CHECK_ARGUMENT_TYPE(effect,PostEffect);


        eff->SetProperty(in_info, rpr_float4(x, y, z, w));
        return RPR_SUCCESS;
    }
    catch (FrException& e)
    {
        TRACE__FUNCTION_FAILED(eff); return e.GetErrorCode();
    }
    catch (FireSG::property_not_found_error&)
    {
        return RPR_ERROR_INVALID_PARAMETER;
    }
    catch (FireSG::property_type_error&)
    {
        return RPR_ERROR_INVALID_PARAMETER_TYPE;
    }
}

rpr_int rprPostEffectSetParameter1u(rpr_post_effect effect, const rpr_char* name, rpr_uint value)
{
	s_logger.printTrace("status = ");
	TRACE__FUNCTION_OPEN
	s_logger.TraceArg__rpr_post_effect(effect); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_char_P(name); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_uint(value);
	s_logger.Trace__FunctionClose();

    API_PRINT("rprPostEffectSetParameter1u", 1, effect);

	FrNode* nodeInRPR = static_cast<FrNode*>(effect);

	try
    {
		CHECK_NOT_NULL(nodeInRPR);

		MACRO__CHECK_ARGUMENT_TYPE(effect,PostEffect);

		// Convert named string to key and call overloaded method which takes the key.
		std::string lcname = name;
		std::transform(lcname.begin(), lcname.end(), lcname.begin(), ::tolower);
		auto itr = PostEffectParameterNamesToKeys.find(lcname);
		if (itr != PostEffectParameterNamesToKeys.cend())
		{
			rpr_int retCode = rprPostEffectSetParameterByKey1u(effect, itr->second, value);
			if ( retCode != RPR_SUCCESS )
				throw FrException(__FILE__,__LINE__,retCode, "",nodeInRPR);
			else
				return retCode;
		}
		else
		{
			throw FrException(__FILE__,__LINE__,RPR_ERROR_INVALID_PARAMETER, "",nodeInRPR);
		}
	}
    catch (FrException& e)
    {
        TRACE__FUNCTION_FAILED(effect); return e.GetErrorCode();
    }
}

rpr_int rprPostEffectSetParameter1f(rpr_post_effect effect, const rpr_char* name, rpr_float x)
{
	s_logger.printTrace("status = ");
	TRACE__FUNCTION_OPEN
	s_logger.TraceArg__rpr_post_effect(effect); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_char_P(name); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_float(x);
	s_logger.Trace__FunctionClose();

    API_PRINT("rprContextSetParameter1f", 1, effect);

	FrNode* nodeInRPR = static_cast<FrNode*>(effect);

	try
    {
		CHECK_NOT_NULL(nodeInRPR);

		MACRO__CHECK_ARGUMENT_TYPE(effect,PostEffect);

		// Convert named string to key and call overloaded method which takes the key.
		std::string lcname = name;
		std::transform(lcname.begin(), lcname.end(), lcname.begin(), ::tolower);
		auto itr = PostEffectParameterNamesToKeys.find(lcname);
		if (itr != PostEffectParameterNamesToKeys.cend())
		{
			rpr_int retCode = rprPostEffectSetParameterByKey1f(effect, itr->second, x);
			if ( retCode != RPR_SUCCESS )
				throw FrException(__FILE__,__LINE__,retCode, "",nodeInRPR);
			else
				return retCode;
		}
		else
		{
			throw FrException(__FILE__,__LINE__,RPR_ERROR_INVALID_PARAMETER, "",nodeInRPR);
		}
	}
    catch (FrException& e)
    {
        TRACE__FUNCTION_FAILED(effect); return e.GetErrorCode();
    }
}

rpr_int rprPostEffectSetParameter3f(rpr_post_effect effect, const rpr_char* name, rpr_float x, rpr_float y, rpr_float z)
{
	s_logger.printTrace("status = ");
	TRACE__FUNCTION_OPEN
	s_logger.TraceArg__rpr_post_effect(effect); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_char_P(name); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_float(x); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_float(y); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_float(z);
	s_logger.Trace__FunctionClose();

    API_PRINT("rprContextSetParameter3f", 1, effect);

	FrNode* nodeInRPR = static_cast<FrNode*>(effect);

	try
    {
		CHECK_NOT_NULL(nodeInRPR);

		MACRO__CHECK_ARGUMENT_TYPE(effect,PostEffect);

		// Convert named string to key and call overloaded method which takes the key.
		std::string lcname = name;
		std::transform(lcname.begin(), lcname.end(), lcname.begin(), ::tolower);
		auto itr = PostEffectParameterNamesToKeys.find(lcname);
		if (itr != PostEffectParameterNamesToKeys.cend())
		{
			rpr_int retCode = rprPostEffectSetParameterByKey3f(effect, itr->second, x, y, z);
			if ( retCode != RPR_SUCCESS )
				throw FrException(__FILE__,__LINE__,retCode, "",nodeInRPR);
			else
				return retCode;
		}
		else
		{
			throw FrException(__FILE__,__LINE__,RPR_ERROR_INVALID_PARAMETER, "",nodeInRPR);
		}
	}
    catch (FrException& e)
    {
        TRACE__FUNCTION_FAILED(effect); return e.GetErrorCode();
    }
}

rpr_int rprPostEffectSetParameter4f(rpr_post_effect effect, const rpr_char* name, rpr_float x, rpr_float y, rpr_float z, rpr_float w)
{
	s_logger.printTrace("status = ");
	TRACE__FUNCTION_OPEN
	s_logger.TraceArg__rpr_post_effect(effect); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_char_P(name); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_float(x); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_float(y); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_float(z); s_logger.TraceArg__COMMA();
	s_logger.TraceArg__rpr_float(w);
	s_logger.Trace__FunctionClose();

    API_PRINT("rprContextSetParameter4f", 1, effect);

	FrNode* nodeInRPR = static_cast<FrNode*>(effect);

	try
    {
		CHECK_NOT_NULL(nodeInRPR);

		MACRO__CHECK_ARGUMENT_TYPE(effect,PostEffect);


		// Convert named string to key and call overloaded method which takes the key.
		std::string lcname = name;
		std::transform(lcname.begin(), lcname.end(), lcname.begin(), ::tolower);
		auto itr = PostEffectParameterNamesToKeys.find(lcname);
		if (itr != PostEffectParameterNamesToKeys.cend())
		{
			rpr_int retCode = rprPostEffectSetParameterByKey4f(effect, itr->second, x, y, z, w);
			if ( retCode != RPR_SUCCESS )
				throw FrException(__FILE__,__LINE__,retCode, "",nodeInRPR);
			else
				return retCode;
		}
		else
		{
			throw FrException(__FILE__,__LINE__,RPR_ERROR_INVALID_PARAMETER, "",nodeInRPR);
		}
	}
    catch (FrException& e)
    {
        TRACE__FUNCTION_FAILED(effect); return e.GetErrorCode();
    }
}


rpr_int rprContextGetAttachedPostEffectCount(rpr_context context, rpr_uint* nb)
{
	s_logger.printTrace("//"); //  ignore getInfo for the trace
	s_logger.printTrace("status = ");
	TRACE__FUNCTION_OPEN
	s_logger.Trace__FunctionClose();

	API_PRINT("rprContextGetAttachedPostEffectCount",0);

    FrNode* ctx = static_cast<FrNode*>(context);

    try
    {
		CHECK_NOT_NULL(ctx);

		MACRO__CHECK_ARGUMENT_TYPE(context,Context);


        auto& postEffectList = ctx->GetProperty<FrPostEffectList>(FR_CONTEXT_POST_EFFECT_LIST);
		*nb = (rpr_uint)postEffectList.size();
        return RPR_SUCCESS;
    }
    catch (FrException& e)
    {
        TRACE__FUNCTION_FAILED(context); return e.GetErrorCode();
    }

	return RPR_ERROR_INVALID_PARAMETER;
}

rpr_int rprContextGetAttachedPostEffect(rpr_context context, rpr_uint i, rpr_post_effect* out_effect)
{
	s_logger.printTrace("//"); //  ignore getInfo for the trace
	s_logger.printTrace("status = ");
	TRACE__FUNCTION_OPEN
	s_logger.Trace__FunctionClose();

	API_PRINT("rprContextGetAttachedPostEffect",0);

    FrNode* ctx = static_cast<FrNode*>(context);

	*out_effect = nullptr;

    try
    {
		CHECK_NOT_NULL(ctx);

		MACRO__CHECK_ARGUMENT_TYPE(context,Context);


        auto& postEffectList = ctx->GetProperty<FrPostEffectList>(FR_CONTEXT_POST_EFFECT_LIST);

		if (postEffectList.size() > i)
		{
			std::list<FrNode*>::iterator it = postEffectList.begin();
			std::advance(it, i);
			*out_effect = reinterpret_cast<rpr_post_effect*>(*it);
		}
		else
		{
			throw FrException(__FILE__,__LINE__,RPR_ERROR_INTERNAL_ERROR, "wrong index",ctx);
		}

        return RPR_SUCCESS;
    }
    catch (FrException& e)
    {
        TRACE__FUNCTION_FAILED(context); return e.GetErrorCode();
    }

	return RPR_ERROR_INVALID_PARAMETER;
}

rpr_int rprPostEffectGetInfo(rpr_post_effect effect, rpr_post_effect_info posteff_info, size_t size, void * data, size_t * size_ret)
{
	s_logger.printTrace("//"); //  ignore getInfo for the trace
	s_logger.printTrace("status = ");
	TRACE__FUNCTION_OPEN
	s_logger.Trace__FunctionClose();

    API_PRINT("rprPostEffectGetInfo",0);
	FrNode* node = static_cast<FrNode*>(effect);

	try
	{
		CHECK_NOT_NULL(node);

		MACRO__CHECK_ARGUMENT_TYPE(effect,PostEffect);


		size_t requiredSize = 0;

		switch (posteff_info)
		{
		case RPR_OBJECT_NAME:
		{
			std::string name = node->GetProperty<std::string>(RPR_OBJECT_NAME);
			requiredSize = name.size() + 1;
			break;
		}

		default:
			requiredSize = node->GetPropertyTypeSize(posteff_info);
		}


		if (data)
		{
			if (size >= requiredSize)
			{
				rpr_post_effect_type typePostEffect = node->GetProperty<rpr_post_effect_type>(RPR_POST_EFFECT_TYPE);

				switch (posteff_info)
				{

				case RPR_POST_EFFECT_TYPE:
				{
					*reinterpret_cast<rpr_post_effect_type*>(data) = typePostEffect;
					break;
				}

				case RPR_POST_EFFECT_WHITE_BALANCE_COLOR_SPACE:
				{
					if ( typePostEffect == RPR_POST_EFFECT_WHITE_BALANCE )
					{
						*reinterpret_cast<rpr_post_effect_type*>(data) = node->GetProperty<rpr_post_effect_type>(posteff_info);
					}
					break;
				}

				case RPR_OBJECT_NAME:
				{
					std::string name = node->GetProperty<std::string>(RPR_OBJECT_NAME);
					auto len = name.size();
					auto str = reinterpret_cast<rpr_char*>(data);
					std::copy(name.cbegin(), name.cend(), str);
					*(str + len) = 0;
					break;
				}

				default:
				{

					// rpr_uint parameters
					if (   typePostEffect == RPR_POST_EFFECT_WHITE_BALANCE && posteff_info == RPR_POST_EFFECT_WHITE_BALANCE_COLOR_SPACE 
						|| typePostEffect == RPR_POST_EFFECT_SIMPLE_TONEMAP && posteff_info == RPR_POST_EFFECT_SIMPLE_TONEMAP_ENABLE_TONEMAP  
						)
					{
						rpr_uint& sensorSize = node->GetProperty<rpr_uint>(posteff_info);
						rpr_uint* dest = reinterpret_cast<rpr_uint*>(data);
						*dest = sensorSize;
					}

					// rpr_float parameters
					else if (   typePostEffect == RPR_POST_EFFECT_WHITE_BALANCE && posteff_info == RPR_POST_EFFECT_WHITE_BALANCE_COLOR_TEMPERATURE 
						     || typePostEffect == RPR_POST_EFFECT_SIMPLE_TONEMAP && posteff_info == RPR_POST_EFFECT_SIMPLE_TONEMAP_EXPOSURE  
						     || typePostEffect == RPR_POST_EFFECT_SIMPLE_TONEMAP && posteff_info == RPR_POST_EFFECT_SIMPLE_TONEMAP_CONTRAST  
						)
					{
						rpr_float& sensorSize = node->GetProperty<rpr_float>(posteff_info);
						rpr_float* dest = reinterpret_cast<rpr_float*>(data);
						*dest = sensorSize;
					}


					else
					{
						throw FrException(__FILE__,__LINE__,RPR_ERROR_INTERNAL_ERROR, "wrong parameter",effect);
					}
				}

				}
			}
			else
			{
				throw FrException(__FILE__,__LINE__,RPR_ERROR_INVALID_PARAMETER, "",node);
			}
		}

		if (size_ret)
			*size_ret = requiredSize;
	}
	catch (FrException& e)
	{
		TRACE__FUNCTION_FAILED(node); return e.GetErrorCode();
	}

	return RPR_SUCCESS;
}




rpr_int rprCompositeGetInfo(rpr_composite composite, rpr_composite_info composite_info, size_t size, void * data, size_t * size_ret)
{
	s_logger.printTrace("//"); //  ignore getInfo for the trace
	s_logger.printTrace("status = ");
	TRACE__FUNCTION_OPEN
	s_logger.Trace__FunctionClose();

    API_PRINT("rprCompositeGetInfo",0);
	FrNode* node = static_cast<FrNode*>(composite);

	try
	{
		CHECK_NOT_NULL(node);

		MACRO__CHECK_ARGUMENT_TYPE(composite,Composite);


		size_t requiredSize = 0;

		switch (composite_info)
		{
		case RPR_OBJECT_NAME:
		{
			std::string name = node->GetProperty<std::string>(RPR_OBJECT_NAME);
			requiredSize = name.size() + 1;
			break;
		}

		default:
			requiredSize = node->GetPropertyTypeSize(composite_info);
		}


		if (data)
		{
			if (size >= requiredSize)
			{
				rpr_composite_type typeComposite = node->GetProperty<rpr_composite_type>(RPR_COMPOSITE_TYPE);

				switch (composite_info)
				{

				case RPR_COMPOSITE_TYPE:
				{
					*reinterpret_cast<rpr_composite_type*>(data) = typeComposite;
					break;
				}

				/*
				case RPR_COMPOSITE_WHITE_BALANCE_COLOR_SPACE:
				{
					if ( typeComposite == RPR_COMPOSITE_WHITE_BALANCE )
					{
						*reinterpret_cast<rpr_composite_type*>(data) = node->GetProperty<rpr_composite_type>(composite_info);
					}
					break;
				}
				*/

				case RPR_OBJECT_NAME:
				{
					std::string name = node->GetProperty<std::string>(RPR_OBJECT_NAME);
					auto len = name.size();
					auto str = reinterpret_cast<rpr_char*>(data);
					std::copy(name.cbegin(), name.cend(), str);
					*(str + len) = 0;
					break;
				}

				default:
				{

					// FrNode* parameters
					if (   typeComposite == RPR_COMPOSITE_FRAMEBUFFER && composite_info == RPR_COMPOSITE_FRAMEBUFFER_INPUT_FB 
						|| typeComposite == RPR_COMPOSITE_LUT && composite_info == RPR_COMPOSITE_LUT_INPUT_LUT  
						|| typeComposite == RPR_COMPOSITE_LUT && composite_info == RPR_COMPOSITE_LUT_INPUT_COLOR  
						|| typeComposite == RPR_COMPOSITE_NORMALIZE && composite_info == RPR_COMPOSITE_NORMALIZE_INPUT_COLOR  
						|| typeComposite == RPR_COMPOSITE_NORMALIZE && composite_info == RPR_COMPOSITE_NORMALIZE_INPUT_SHADOWCATCHER 
						|| typeComposite == RPR_COMPOSITE_LERP_VALUE && composite_info == RPR_COMPOSITE_LERP_VALUE_INPUT_COLOR0 
						|| typeComposite == RPR_COMPOSITE_LERP_VALUE && composite_info == RPR_COMPOSITE_LERP_VALUE_INPUT_COLOR1 
						|| typeComposite == RPR_COMPOSITE_LERP_VALUE && composite_info == RPR_COMPOSITE_LERP_VALUE_INPUT_WEIGHT 
						|| typeComposite == RPR_COMPOSITE_ARITHMETIC && composite_info == RPR_COMPOSITE_ARITHMETIC_INPUT_COLOR0 
						|| typeComposite == RPR_COMPOSITE_ARITHMETIC && composite_info == RPR_COMPOSITE_ARITHMETIC_INPUT_COLOR1 
						|| typeComposite == RPR_COMPOSITE_GAMMA_CORRECTION && composite_info == RPR_COMPOSITE_GAMMA_CORRECTION_INPUT_COLOR 
						)
					{
						FrNode* sensorSize = node->GetProperty<FrNode*>(composite_info);
						FrNode** dest = reinterpret_cast<FrNode**>(data);
						*dest = sensorSize;
					}

					// rpr_float4 parameters
					else if (  typeComposite == RPR_COMPOSITE_CONSTANT && composite_info == RPR_COMPOSITE_CONSTANT_INPUT_VALUE   
						)
					{
						rpr_float4& sensorSize = node->GetProperty<rpr_float4>(composite_info);
						rpr_float4* dest = reinterpret_cast<rpr_float4*>(data);
						*dest = sensorSize;
					}

					// rpr_material_node_arithmetic_operation parameters
					else if (  typeComposite == RPR_COMPOSITE_ARITHMETIC && composite_info == RPR_COMPOSITE_ARITHMETIC_INPUT_OP 
						)
					{
						rpr_material_node_arithmetic_operation& sensorSize = node->GetProperty<rpr_material_node_arithmetic_operation>(composite_info);
						rpr_material_node_arithmetic_operation* dest = reinterpret_cast<rpr_material_node_arithmetic_operation*>(data);
						*dest = sensorSize;
					}



					else
					{
						throw FrException(__FILE__,__LINE__,RPR_ERROR_INTERNAL_ERROR, "wrong parameter",node);
					}
				}

				}
			}
			else
			{
				throw FrException(__FILE__,__LINE__,RPR_ERROR_INVALID_PARAMETER, "",node);
			}
		}

		if (size_ret)
			*size_ret = requiredSize;
	}
	catch (FrException& e)
	{
		TRACE__FUNCTION_FAILED(node); return e.GetErrorCode();
	}

	return RPR_SUCCESS;
}






//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


#ifdef RPR_FOR_BAIKAL

static const std::map<rpr_material_node_input, std::string> kRPRInputStrings =
{
    { RPR_UBER_MATERIAL_DIFFUSE_COLOR, "uberv2.diffuse.color" },
    { RPR_UBER_MATERIAL_LAYERS, "uberv2.layers" },
    { RPR_UBER_MATERIAL_REFLECTION_COLOR, "uberv2.reflection.color" },
    { RPR_UBER_MATERIAL_REFLECTION_ROUGHNESS, "uberv2.reflection.roughness" },
    { RPR_UBER_MATERIAL_REFLECTION_ANISOTROPY, "uberv2.reflection.anisotropy" },
    { RPR_UBER_MATERIAL_REFLECTION_ANISOTROPY_ROTATION, "uberv2.reflection.anisotropy_rotation" },
    { RPR_UBER_MATERIAL_REFLECTION_IOR, "uberv2.reflection.ior" },
    { RPR_UBER_MATERIAL_REFLECTION_METALNESS, "uberv2.reflection.metalness" },
    { RPR_UBER_MATERIAL_REFRACTION_COLOR, "uberv2.refraction.color" },
    { RPR_UBER_MATERIAL_REFRACTION_ROUGHNESS, "uberv2.refraction.roughness" },
    { RPR_UBER_MATERIAL_REFRACTION_IOR, "uberv2.refraction.ior" },
    { RPR_UBER_MATERIAL_REFRACTION_IOR_MODE, "uberv2.refraction.ior_mode" },
    { RPR_UBER_MATERIAL_REFRACTION_THIN_SURFACE, "uberv2.refraction.thin_surface" },
    { RPR_UBER_MATERIAL_COATING_COLOR, "uberv2.coating.color" },
    { RPR_UBER_MATERIAL_COATING_IOR, "uberv2.coating.ior" },
    { RPR_UBER_MATERIAL_EMISSION_COLOR, "uberv2.emission.color" },
    { RPR_UBER_MATERIAL_EMISSION_WEIGHT, "uberv2.emission.weight" },
    { RPR_UBER_MATERIAL_EMISSION_MODE, "uberv2.emission.mode" },
    { RPR_UBER_MATERIAL_TRANSPARENCY, "uberv2.transparency" },
    { RPR_UBER_MATERIAL_NORMAL, "uberv2.normal" },
    { RPR_UBER_MATERIAL_BUMP, "uberv2.bump" },
    { RPR_UBER_MATERIAL_DISPLACEMENT, "uberv2.displacement" },
    { RPR_UBER_MATERIAL_SSS_ABSORPTION_COLOR, "uberv2.sss.absorption_color" },
    { RPR_UBER_MATERIAL_SSS_SCATTER_COLOR, "uberv2.sss.scatter_color" },
    { RPR_UBER_MATERIAL_SSS_ABSORPTION_DISTANCE, "uberv2.sss.absorption_distance" },
    { RPR_UBER_MATERIAL_SSS_SCATTER_DISTANCE, "uberv2.sss.scatter_distance" },
    { RPR_UBER_MATERIAL_SSS_SCATTER_DIRECTION, "uberv2.sss.scatter_direction" },
    { RPR_UBER_MATERIAL_SSS_SUBSURFACE_COLOR, "uberv2.sss.subsurface_color" },
    { RPR_UBER_MATERIAL_SSS_MULTISCATTER, "uberv2.sss.multiscatter" }
};

rpr_int rprMaterialNodeSetInputN_ext(rpr_material_node in_node, rpr_material_node_input in_input, rpr_material_node in_input_node)
{

		
	FrNode* matNode = static_cast<FrNode*>(in_node);
	FrNode* matNodeInput = static_cast<FrNode*>(in_input_node);
	try
	{
		
		std::shared_ptr<MaterialNode> previousState = matNode->GetProperty<std::shared_ptr<MaterialNode>>(in_input);
		previousState->SetFrNode(matNodeInput);
		matNode->PropertyChanged(in_input);

	}
	catch (FrException& e)
	{
		TRACE__FUNCTION_FAILED(matNode); 
		return e.GetErrorCode();
	}

	return RPR_SUCCESS;



}
rpr_int rprMaterialNodeSetInputF_ext(rpr_material_node in_node, rpr_material_node_input in_input, rpr_float in_value_x, rpr_float in_value_y, rpr_float in_value_z, rpr_float in_value_w)
{
	
	FrNode* matNode = static_cast<FrNode*>(in_node);
	try
	{
		
		std::shared_ptr<MaterialNode> previousState = matNode->GetProperty<std::shared_ptr<MaterialNode>>(in_input);
		previousState->SetFloat4(in_value_x, in_value_y, in_value_z, in_value_w);
		matNode->PropertyChanged(in_input);

	}
	catch (FrException& e)
	{
		TRACE__FUNCTION_FAILED(matNode); 
		return e.GetErrorCode();
	}

	return RPR_SUCCESS;



}
rpr_int rprMaterialNodeSetInputU_ext(rpr_material_node in_node, rpr_material_node_input in_input, rpr_uint in_value)
{
	FrNode* matNode = static_cast<FrNode*>(in_node);
	try
	{
		
		std::shared_ptr<MaterialNode> previousState = matNode->GetProperty<std::shared_ptr<MaterialNode>>(in_input);
		previousState->SetUint1(in_value);
		matNode->PropertyChanged(in_input);

	}
	catch (FrException& e)
	{
		TRACE__FUNCTION_FAILED(matNode); 
		return e.GetErrorCode();
	}

	return RPR_SUCCESS;

}
rpr_int rprMaterialNodeSetInputImageData_ext(rpr_material_node in_node, rpr_material_node_input in_input, rpr_image image)
{
    auto name_it = kRPRInputStrings.find(in_input);
    return name_it != kRPRInputStrings.end() ?
        rprMaterialNodeSetInputImageData(in_node, name_it->second.c_str(), image)
        : RPR_ERROR_UNSUPPORTED;
}
rpr_int rprMaterialNodeSetInputBufferData_ext(rpr_material_node in_node, rpr_material_node_input in_input, rpr_buffer buffer)
{
    auto name_it = kRPRInputStrings.find(in_input);
    return name_it != kRPRInputStrings.end() ?
        rprMaterialNodeSetInputBufferData(in_node, name_it->second.c_str(), buffer)
        : RPR_ERROR_UNSUPPORTED;
}

#endif




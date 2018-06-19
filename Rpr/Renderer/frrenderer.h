#pragma once

#include <Rpr/Base/Common.h>
#include <Rpr/Node/FrNode.h>
#include <cstdint>

class PluginManager;

class FrRenderer
{
public:
	virtual ~FrRenderer() {};

    virtual void Initialize(FrNode* context) = 0;

    virtual void Render() = 0;

    virtual void RenderTile(rpr_uint xmin, rpr_uint xmax, rpr_uint ymin, rpr_uint ymax) = 0;

    virtual void ClearMemory() = 0;

    virtual void FrameBufferClear(FrNode* frameBuffer) = 0;

    virtual void FrameBufferSaveToFile(FrNode* frameBuffer, rpr_char const* filePath) = 0;

	virtual void FrameBufferGetData(FrNode* frameBuffer, void* data) = 0;

    virtual void ResolveFrameBuffer(FrNode* srcFrameBuffer, FrNode* dstFrameBuffer, bool normalizeOnly) = 0;

	virtual void ResolveComposite(FrNode* compositeNode, FrNode* fbNode) = 0;

	virtual void GetRenderStatistics(rpr_render_statistics& stat) = 0;

	virtual std::size_t GetInfoSize(rpr_context_info info) const { return 0; }

	virtual void GetInfo(rpr_context_info info, void* data) const { return; }

	virtual std::size_t GetInfoSize(FrNode* node, rpr_uint info) const { return 0; }

	virtual void GetInfo(FrNode* node, rpr_uint info, void* data) const { return; }

	virtual rpr_uint GetUint(FrNode* node, std::uint32_t key) const { return 0; }

	virtual rpr_float4 GetFloat4(FrNode* node, std::uint32_t key) const { return rpr_float4(); }


	virtual void GetSceneAABB(fr_float* aabb) const = 0;

	virtual rpr_uint GetMaterialSystemSize() const { return 0; };

	virtual void AdaptSubdiv(FrNode* shape , FrNode* camera, FrNode* framebuffer, int coeff) = 0;

	virtual void SetAOVindexLookup(int index, rpr_float4 color) = 0;
};


class FrRendererEncalps
{
public:

	FrRendererEncalps(rpr_int pluginID, PluginManager* g_pluginManager);
	~FrRendererEncalps();

	rpr_int m_pluginID;
	FrRenderer* m_FrRenderer;
	PluginManager* m_pluginManager;
};


#ifdef WIN32
#define FIRERENDER_API __declspec(dllexport)
#else
#define FIRERENDER_API
#endif

typedef FrRenderer* (*PFNCREATERENDERERINSTANCE)(void);

typedef void (*PFNDELETERENDERERINSTANCE)(FrRenderer*);
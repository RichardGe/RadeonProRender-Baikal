#pragma once

#include <memory>
#include <Rpr/Renderer/FrRenderer.h>


#include "WrapObject/WrapObject.h"
#include "WrapObject/ContextObject.h"
#include "WrapObject/CameraObject.h"
#include "WrapObject/FramebufferObject.h"
#include "WrapObject/LightObject.h"
#include "WrapObject/Materials/MaterialObject.h"
#include "WrapObject/MatSysObject.h"
#include "WrapObject/SceneObject.h"
#include "WrapObject/ShapeObject.h"
#include "Rpr/Base/FrException.h"



namespace Shasta
{
	class Renderer : public FrRenderer
	{
	public:
		Renderer();

		~Renderer();

		void Initialize(FrNode* context);

		void Render();

		void RenderTile(rpr_uint xmin, rpr_uint xmax, rpr_uint ymin, rpr_uint ymax);

		void ClearMemory();

		void FrameBufferClear(FrNode* node);

		void FrameBufferSaveToFile(FrNode* node, rpr_char const* filePath);

		void FrameBufferGetData(FrNode* node, void* data);

		void ResolveFrameBuffer(FrNode* srcNode, FrNode* dstNode, bool normalizeOnly);

		void ResolveComposite(FrNode* compositeNode, FrNode* fbNode);

		void OnNodeCreated(FrNode* node);

		void OnNodeDeleted(FrNode* node);

		void OnPropertyChanged(FrNode* node, uint32_t const& propertyKey, void* args);

		void GetRenderStatistics(rpr_render_statistics& stat);

		std::size_t GetInfoSize(rpr_context_info info) const;

		void GetInfo(rpr_context_info info, void* data) const;

		std::size_t GetInfoSize(FrNode* node, rpr_uint info) const;

		void GetInfo(FrNode* node, rpr_uint info, void* data) const;

		rpr_uint GetUint(FrNode* node, std::uint32_t key) const;

		rpr_float4 GetFloat4(FrNode* node, std::uint32_t key) const;


		void GetSceneAABB(fr_float* aabb) const;

		rpr_uint GetMaterialSystemSize() const;

		void AdaptSubdiv(FrNode* shape , FrNode* camera, FrNode* framebuffer, int coeff);

		void SetAOVindexLookup(int index, rpr_float4 color);
/*
	private:
		void OnMaterialNodeCreated(FrNode* node);

		void OnStandardMaterialCreated(FrNode* node);

		void OnMaterialNodePropertyChanged(FrNode* node, uint32_t const& propertyKey, void* args);

		Tahoe::gm::GraphMaterialDesc* CreateGraphMaterialDesc(rpr_material_node_type type, Tahoe::GraphMaterialSystem* matSys);

		void ResetCameraLensParams(FrNode* node);

		Tahoe::Node* MaterialSetHeteroVolume(FrNode* node,Tahoe::Node*);

		Tahoe::AOVChannel FireRenderToTahoeAOV(rpr_aov aov);
		Tahoe::SubdBoundaryInterpolationType RPRtoTahoe_subdinterop(rpr_subdiv_boundary_interfop_type type);
		Tahoe::InputLookups RPRtoTahoe_lookup(rpr_material_node_lookup_value val) const;
		rpr_material_node_lookup_value TahoetoRPR_lookup(Tahoe::InputLookups val) const;
		Tahoe::UvProceduralTypes RPRtoTahoe_uvtype(rpr_material_node_uvtype_value val) const;
		rpr_material_node_uvtype_value TahoetoRPR_uvtype(Tahoe::UvProceduralTypes val) const;
		Tahoe::TextureOptions RPRtoTahoe_textureWrap(rpr_image_wrap_type type) const;
		Tahoe::TextureOptions RPRtoTahoe_textureFilter(rpr_image_filter_type type) const;
		Tahoe::ArithmeticOps RPRtoTahoe_arithmeticOp(rpr_material_node_arithmetic_operation type) const;
		rpr_material_node_arithmetic_operation TahoetoRPR_arithmeticOp(Tahoe::ArithmeticOps  type) const;
		int RPRtoTahoe_heteroVolumeFilter(rpr_hetero_volume_filter type) const;

		void GetDeviceName(rpr_context_info deviceInfo, std::string& nameOut) const;

		rpr_creation_flags m_creationFlags;
		TahoeApiPtr m_api;
		FrNode* m_context;
		Node* m_nodeRenderContext;
		Node* m_defaultShader;
        Node* m_tempFrameBuffer1;
        Node* m_tempFrameBuffer2;
        int2 m_res;
		Node* m_cctxt; // composite context.  created and deleted  with m_context

		std::map<FrNode*,FrNode*> m_scene_envlight; // first is a Scene  -  second is a light

		// list of all nodes.
		// the Node is added during OnNodeCreated
		// the Node is removed during OnNodeDeleted
		std::set<FrNode*> m_rprNodeList; 
*/

		


	private : 


		void ResetCameraPosLookUp(FrNode* node);

		//cast data
   FrNode* m_context ;
   rpr_creation_flags m_creationFlags;
  
   
   //std::shared_ptr<ContextObject> m_contextBaikal;
   ContextObject* m_contextBaikal;





    	};
}

#ifdef __cplusplus
extern "C"
{
#endif
	FIRERENDER_API FrRenderer* CreateRendererInstance();
	FIRERENDER_API void DeleteRendererInstance(FrRenderer* instance);
#ifdef __cplusplus
}
#endif

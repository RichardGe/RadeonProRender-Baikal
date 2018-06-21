#include "Renderer.h"

#include "../RprHybrid/Node/RprMaterialNode.h"


#define FR_NODE_BAIKAL_NODE          (FR_CUSTOM_PROPERTY_KEYS_START - 1)





using namespace Shasta;

Renderer::Renderer()
{
	m_context = nullptr;


}

Renderer::~Renderer()
{
}

void Renderer::Initialize(FrNode* context)
{
	// Register callbacks for SceneGraph events.
	std::shared_ptr<FrSceneGraph> sceneGraph = context->GetProperty<  std::shared_ptr<FrSceneGraph>  >(FR_SCENEGRAPH);
	sceneGraph->RegisterCallback(FireSG::SceneGraphEvents::OnNodeCreated, std::function<void(FrNode*)>(std::bind(&Renderer::OnNodeCreated, this, std::placeholders::_1)));
	sceneGraph->RegisterCallback(FireSG::SceneGraphEvents::OnNodeDeleted, std::function<void(FrNode*)>(std::bind(&Renderer::OnNodeDeleted, this, std::placeholders::_1)));
	sceneGraph->RegisterCallback(FireSG::SceneGraphEvents::OnPropertyChanged, std::function<void(FrNode*, uint32_t const&, void*)>(std::bind(&Renderer::OnPropertyChanged, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3)));

	m_creationFlags = context->GetProperty<rpr_creation_flags>(RPR_CONTEXT_CREATION_FLAGS);

	m_context = context;

	//m_contextBaikal = std::shared_ptr<ContextObject>(new ContextObject(m_creationFlags));
	m_contextBaikal = new ContextObject(m_creationFlags);

}

void Renderer::Render()
{
	

    m_contextBaikal->Render();

    return;


}

void Renderer::GetImageData(FrNode* img, void* data)
{
	return;
}

void Renderer::RenderTile(rpr_uint xmin, rpr_uint xmax, rpr_uint ymin, rpr_uint ymax)
{
	m_contextBaikal->RenderTile(xmin, xmax, ymin, ymax);
	return;
}

void Renderer::ClearMemory()
{
}

void Renderer::FrameBufferClear(FrNode* node)
{
	FramebufferObject* buff = node->GetProperty<FramebufferObject*>(FR_NODE_BAIKAL_NODE); 
	buff->Clear();

}

void Renderer::FrameBufferSaveToFile(FrNode* node, rpr_char const* filePath)
{
	FramebufferObject* buff = node->GetProperty<FramebufferObject*>(FR_NODE_BAIKAL_NODE); 
	buff->SaveToFile(filePath);
}

void Renderer::FrameBufferGetData(FrNode* node, void* data)
{
	rpr_framebuffer_desc desc = node->GetProperty<rpr_framebuffer_desc>(RPR_FRAMEBUFFER_DESC);
	rpr_framebuffer_format format = node->GetProperty<rpr_framebuffer_format>(RPR_FRAMEBUFFER_FORMAT);

//	Tahoe::Node* tahoeNode = node->GetProperty<Tahoe::Node*>(FR_NODE_TAHOE_NODE);
//	m_api->readFrameBuffer(tahoeNode, 0, desc.fb_width, 0, desc.fb_height, (float*)data);

	float* ptr = (float*)data;
	for(int j=0; j<(int)desc.fb_height; j++)
	for(int i=0; i<(int)desc.fb_width; i++)
	{
		ptr[4*(i+j*desc.fb_width)+0] = i/(float)desc.fb_width;
		ptr[4*(i+j*desc.fb_width)+1] = j/(float)desc.fb_height;
		ptr[4*(i+j*desc.fb_width)+2] = 0.f;
		ptr[4*(i+j*desc.fb_width)+3] = 0.f;
	}
}

void Renderer::ResolveFrameBuffer(FrNode* srcNode, FrNode* dstNode, bool normalizeOnly)
{
	int aa=0;
}

void Renderer::ResolveComposite(FrNode* compositeNode, FrNode* fbNode)
{
}

void Renderer::OnNodeCreated(FrNode* node)
{
	try
	{

		switch (node->GetType())
		{
			case NodeTypes::Scene:
			{
				SceneObject* baikalNode = m_contextBaikal->CreateScene();
				node->AddProperty(FR_NODE_BAIKAL_NODE, baikalNode);
				break;
			}
			case NodeTypes::Camera:
			{
				CameraObject* baikalNode = m_contextBaikal->CreateCamera();
				node->AddProperty(FR_NODE_BAIKAL_NODE, baikalNode);
				break;
			}
			case NodeTypes::Mesh:
			{
				float* in_vertices = node->GetProperty<rpr_float*>(RPR_MESH_VERTEX_ARRAY);
				float* in_normals = node->GetProperty<rpr_float*>(RPR_MESH_NORMAL_ARRAY);
				float* in_texcoords = node->GetProperty<rpr_float*>(RPR_MESH_UV_ARRAY);
				//float* in_texcoords2 = node->GetProperty<rpr_float*>(RPR_MESH_UV2_ARRAY);

				size_t in_vertex_stride = node->GetProperty<size_t>(RPR_MESH_VERTEX_STRIDE);
				size_t in_normal_stride = node->GetProperty<size_t>(RPR_MESH_NORMAL_STRIDE);
				size_t in_texcoord_stride = node->GetProperty<size_t>(RPR_MESH_UV_STRIDE);
				//size_t in_texcoord_stride2 = node->GetProperty<size_t>(RPR_MESH_UV2_STRIDE);

				size_t in_num_vertices = node->GetProperty<size_t>(RPR_MESH_VERTEX_COUNT);
				size_t in_num_normals = node->GetProperty<size_t>(RPR_MESH_NORMAL_COUNT);
				size_t in_num_uv = node->GetProperty<size_t>(RPR_MESH_UV_COUNT);
				//size_t in_num_uv2 = node->GetProperty<size_t>(RPR_MESH_UV2_COUNT);

				rpr_int* in_numFaceVertices = node->GetProperty<rpr_int*>(RPR_MESH_NUM_FACE_VERTICES_ARRAY);
				size_t in_numFaces = node->GetProperty<size_t>(RPR_MESH_POLYGON_COUNT);

				rpr_int* in_vertexIndexArray = node->GetProperty<rpr_int*>(RPR_MESH_VERTEX_INDEX_ARRAY);
				rpr_int* in_normalIndexArray = node->GetProperty<rpr_int*>(RPR_MESH_NORMAL_INDEX_ARRAY);
				rpr_int* in_uvIndexArray = node->GetProperty<rpr_int*>(RPR_MESH_UV_INDEX_ARRAY);
				//rpr_int* in_uvIndexArray2 = node->GetProperty<rpr_int*>(RPR_MESH_UV2_INDEX_ARRAY);

				size_t in_vertexIdxStride = node->GetProperty<size_t>(RPR_MESH_VERTEX_INDEX_STRIDE);
				size_t in_normalIdxStride = node->GetProperty<size_t>(RPR_MESH_NORMAL_INDEX_STRIDE);
				size_t in_uvIdxStride = node->GetProperty<size_t>(RPR_MESH_UV_INDEX_STRIDE);
				//size_t in_uvIdxStride2 = node->GetProperty<size_t>(RPR_MESH_UV2_INDEX_STRIDE);

				//rpr_uint in_uvDim = node->GetProperty<rpr_uint>(RPR_MESH_UV_DIM);

				ShapeObject* baikalNode = m_contextBaikal->CreateShape(
					in_vertices, in_num_vertices, (rpr_int)in_vertex_stride,
                    in_normals, in_num_normals, (rpr_int)in_normal_stride,
                    in_texcoords, in_num_uv, (rpr_int)in_texcoord_stride,
                    in_vertexIndexArray, (rpr_int)in_vertexIdxStride,
                    in_normalIndexArray, (rpr_int)in_normalIdxStride,
                    in_uvIndexArray, (rpr_int)in_uvIdxStride,
                    in_numFaceVertices, in_numFaces);
				node->AddProperty(FR_NODE_BAIKAL_NODE, baikalNode);


				




				break;
			}
			case NodeTypes::FrameBuffer:
			{
				rpr_framebuffer_format in_format = node->GetProperty<rpr_framebuffer_format>(RPR_FRAMEBUFFER_FORMAT);
				rpr_framebuffer_desc in_fb_desc = node->GetProperty<rpr_framebuffer_desc>(RPR_FRAMEBUFFER_DESC);
	
				FramebufferObject* baikalNode = m_contextBaikal->CreateFrameBuffer(in_format, &in_fb_desc);
				node->AddProperty(FR_NODE_BAIKAL_NODE, baikalNode);
				break;
			}

			case NodeTypes::PointLight:
			{
				LightObject* baikalNode = m_contextBaikal->CreateLight(LightObject::Type::kPointLight);
				node->AddProperty(FR_NODE_BAIKAL_NODE, baikalNode);
				break;
			}

			case NodeTypes::SpotLight:
			{
				LightObject* baikalNode = m_contextBaikal->CreateLight(LightObject::Type::kSpotLight);
				node->AddProperty(FR_NODE_BAIKAL_NODE, baikalNode);
				break;
			}

			case NodeTypes::DirectionalLight:
			{
				LightObject* baikalNode = m_contextBaikal->CreateLight(LightObject::Type::kDirectionalLight);
				node->AddProperty(FR_NODE_BAIKAL_NODE, baikalNode);
				break;
			}

			case NodeTypes::EnvironmentLight:
			{
				LightObject* baikalNode = m_contextBaikal->CreateLight(LightObject::Type::kEnvironmentLight);
				node->AddProperty(FR_NODE_BAIKAL_NODE, baikalNode);
				break;
			}

			case NodeTypes::MaterialSystem:
			{

				MatSysObject* baikalNode = m_contextBaikal->CreateMaterialSystem();
				node->AddProperty(FR_NODE_BAIKAL_NODE, baikalNode);

				break;
			}

			case NodeTypes::Material:
			{
				FrNode* matsys = node->GetProperty<FrNode*>(RPR_MATERIAL_NODE_SYSTEM);
				MatSysObject* matsysBaikal = matsys->GetProperty<MatSysObject*>(FR_NODE_BAIKAL_NODE);
				rpr_material_node_type nodeType = node->GetProperty<rpr_material_node_type>(RPR_MATERIAL_NODE_TYPE);


				MaterialObject* baikalNode = matsysBaikal->CreateMaterial(nodeType);
				//MaterialObject* baikalNode = m_matsysBaikal__ASUP->CreateMaterial(nodeType);


				node->AddProperty(FR_NODE_BAIKAL_NODE, baikalNode);
				break;
			}

			case NodeTypes::Image:
			{
				rpr_image_format in_format = node->GetProperty<rpr_image_format>(RPR_IMAGE_FORMAT);
				rpr_image_desc in_image_desc = node->GetProperty<rpr_image_desc>(RPR_IMAGE_DESC);
				std::shared_ptr<unsigned char> in_data = node->GetProperty<std::shared_ptr<unsigned char>>(RPR_IMAGE_DATA);

				MaterialObject* baikalNode1 = m_contextBaikal->CreateImage(in_format, &in_image_desc, in_data.get());
				//MaterialObject* baikalNode2 = m_contextBaikal->CreateImageFromFile("../Resources/Textures/test_albedo1.jpg");




				/*
				std::string imgName = node->GetProperty<std::string>(RPR_OBJECT_NAME);
				MaterialObject* baikalNode1 = nullptr;
				if ( imgName != "" )
				{
					baikalNode1 = m_contextBaikal->CreateImageFromFile(imgName.c_str());
				}
				else
				{
					baikalNode1 = m_contextBaikal->CreateImage(in_format, &in_image_desc, in_data.get());
				}
				*/





				node->AddProperty(FR_NODE_BAIKAL_NODE, baikalNode1);
				break;
			}

			case NodeTypes::Instance:
			{

				FrNode* parentShape = node->GetProperty<FrNode*>(RPR_INSTANCE_PARENT_SHAPE);
				ShapeObject* parentShapeTahoeNode = parentShape->GetProperty<ShapeObject*>(FR_NODE_BAIKAL_NODE);
				
				
				//ShapeObject* tahoeNode = m_api->createInstance(TAHOE_INSTANCE_TAG, parentShapeTahoeNode);
				ShapeObject* out_instance = m_contextBaikal->CreateShapeInstance(parentShapeTahoeNode);
				
				node->AddProperty(FR_NODE_BAIKAL_NODE, out_instance);



				
				break;
			}

			

			default:
			{
				int aa=0;
				break;
			}

		}


		//  1001 is Tahoe
		//  1002 is Baikal
		node->SetProperty<rpr_uint>(RPR_OBJECT_RENDERER_ID, 1002);


	}
	catch(FrException& e)
	{
		throw e;
	}


	return;

}

void Renderer::OnNodeDeleted(FrNode* node)
{
	switch (node->GetType())
	{

		case NodeTypes::FrameBuffer:
		{
			FramebufferObject* obj = node->GetProperty<FramebufferObject*>(FR_NODE_BAIKAL_NODE);
			WrapObject* wobj = static_cast<WrapObject*>(obj);
			delete wobj;
			break;
		}

		case NodeTypes::Image:
		{
			MaterialObject* obj = node->GetProperty<MaterialObject*>(FR_NODE_BAIKAL_NODE);
			WrapObject* wobj = static_cast<WrapObject*>(obj);
			delete wobj;
			break;
		}

		case NodeTypes::EnvironmentLight:
		case NodeTypes::SpotLight:
		case NodeTypes::PointLight:
		case NodeTypes::DirectionalLight:
		{
			LightObject* obj = node->GetProperty<LightObject*>(FR_NODE_BAIKAL_NODE);
			WrapObject* wobj = static_cast<WrapObject*>(obj);
			delete wobj;
			break;
		}

		case NodeTypes::Mesh:
		case NodeTypes::Instance:
		{
			ShapeObject* obj = node->GetProperty<ShapeObject*>(FR_NODE_BAIKAL_NODE);
			WrapObject* wobj = static_cast<WrapObject*>(obj);
			delete wobj;
			break;
		}

		case NodeTypes::Material:
		{
			MaterialObject* obj = node->GetProperty<MaterialObject*>(FR_NODE_BAIKAL_NODE);
			WrapObject* wobj = static_cast<WrapObject*>(obj);
			delete wobj;
			break;
		}

		case NodeTypes::Camera:
		{
			CameraObject* obj = node->GetProperty<CameraObject*>(FR_NODE_BAIKAL_NODE);
			WrapObject* wobj = static_cast<WrapObject*>(obj);
			delete wobj;
			break;
		}

		case NodeTypes::Scene:
		{
			SceneObject* obj = node->GetProperty<SceneObject*>(FR_NODE_BAIKAL_NODE);
			WrapObject* wobj = static_cast<WrapObject*>(obj);
			delete wobj;
			break;
		}

		case NodeTypes::Context:
		{
			delete m_contextBaikal;
			m_contextBaikal = nullptr;

			int a=0;
			break;
		}

		default :
		{
			int aa=0;
			break;
		}

	}

}

void Renderer::ResetCameraPosLookUp(FrNode* node)
{
	rpr_float3 pos = node->GetProperty<rpr_float3>(RPR_CAMERA_POSITION);
	rpr_float3 at = node->GetProperty<rpr_float3>(RPR_CAMERA_LOOKAT);
	rpr_float3 up = node->GetProperty<rpr_float3>(RPR_CAMERA_UP);
	CameraObject* tahoeNodeCamera_ = node->GetProperty<CameraObject*>(FR_NODE_BAIKAL_NODE);

	tahoeNodeCamera_->LookAt(
		*((RadeonRays::float3*)&pos),
		*((RadeonRays::float3*)&at),
		*((RadeonRays::float3*)&up)
		);
}


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



std::map<uint32_t, std::string> kMaterialNodeInputStrings = {
        { RPR_MATERIAL_INPUT_COLOR, "color" },
        { RPR_MATERIAL_INPUT_COLOR0, "color0" },
        { RPR_MATERIAL_INPUT_COLOR1, "color1" },
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
        { RPR_MATERIAL_INPUT_SCALE, "bumpscale" },
        { RPR_MATERIAL_INPUT_SCATTERING, "sigmas" },
        { RPR_MATERIAL_INPUT_ABSORBTION, "sigmaa" },
        { RPR_MATERIAL_INPUT_EMISSION, "emission" },
        { RPR_MATERIAL_INPUT_G, "g" },
        { RPR_MATERIAL_INPUT_MULTISCATTER, "multiscatter" },

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

    };



void Renderer::OnPropertyChanged(FrNode* node, uint32_t const& propertyKey, void* args)
{
	#ifdef _DEBUG
	//we are changing an unlisted node... it's certainly an undef pointer
	//if ( m_rprNodeList.find(node) == m_rprNodeList.end() ) 
	//{
	//	throw FrException(__FILE__,__LINE__,RPR_ERROR_INVALID_PARAMETER, "OnPropertyChanged on unlisted node", m_context);
	//}
	#endif


	switch (node->GetType())
	{
		case NodeTypes::Context:
		{
			switch (propertyKey)
			{


				case RPR_CONTEXT_SCENE:
				{

					int aa=0;


					FrNode* lcargs = reinterpret_cast<FrNode*>(args);
					if ( lcargs )
					{
						SceneObject* tahoeNodeScene = lcargs->GetProperty<SceneObject*>(FR_NODE_BAIKAL_NODE);
						m_contextBaikal->SetCurrenScene(tahoeNodeScene);
					}
					else
					{
						//m_api->set( nullptr );
					}




					//cast data
					//ContextObject* context = WrapObject::Cast<ContextObject>(in_context);
					//SceneObject* scene = WrapObject::Cast<SceneObject>(in_scene);

					//if (!context)
					//{
					//	throw FrException(__FILE__,__LINE__,RPR_ERROR_INVALID_PARAMETER_TYPE, "OnPropertyChanged: Invalid parameter", m_context);
					//}

					//m_contextBaikal->SetCurrenScene(scene);

					break;
				}

				case FR_CONTEXT_AOV_LIST:
				{
					rpr_aov* lcargs = reinterpret_cast<rpr_aov*>(args);
					FrAOVList& aovList = node->GetProperty<FrAOVList>(FR_CONTEXT_AOV_LIST);

					FramebufferObject* framebuffer_TahoeNode = nullptr;
					if ( aovList[*lcargs] ) 
					{ 
						framebuffer_TahoeNode = aovList[*lcargs]->GetProperty<FramebufferObject*>(FR_NODE_BAIKAL_NODE); 
					}

					rpr_aov aov = *lcargs;

					//m_api->setAov(FireRenderToTahoeAOV(*lcargs), framebuffer_TahoeNode);
					m_contextBaikal->SetAOV(aov, framebuffer_TahoeNode);
					

					//ThrowIfFailed(__FILE__,__LINE__,m_api->getError(),m_api->getErrorMsg(), "TahoeContext::SetParameter can't apply the setting",m_context);
					break;
				}


				case RPR_CONTEXT_RANDOM_SEED:
				{
					rpr_uint v = node->GetProperty<rpr_uint>(RPR_CONTEXT_RANDOM_SEED);
					m_contextBaikal->SetParameter("randseed", v);
					break;
				}


				//list of parameters doing nothing
				case RPR_CONTEXT_ACTIVE_PLUGIN:
				case RPR_CONTEXT_GPU0_NAME:
				case RPR_CONTEXT_GPU1_NAME:
				case RPR_CONTEXT_GPU2_NAME:
				case RPR_CONTEXT_GPU3_NAME:
				case RPR_CONTEXT_GPU4_NAME:
				case RPR_CONTEXT_GPU5_NAME:
				case RPR_CONTEXT_GPU6_NAME:
				case RPR_CONTEXT_GPU7_NAME:
				case RPR_CONTEXT_CPU_NAME:
				case RPR_CONTEXT_LAST_ERROR_MESSAGE:
				{
					break;
				}


				default:
				{
					//throw FrException(__FILE__,__LINE__,RPR_ERROR_INVALID_PARAMETER_TYPE, "OnPropertyChanged: Invalid parameter", m_context);
					int aa=0;
					break;
				}
			}


			break;
		}


		case NodeTypes::Scene:
		{
			SceneObject* tahoeNodeScene_ = node->GetProperty<SceneObject*>(FR_NODE_BAIKAL_NODE);

			switch (propertyKey)
			{

				case RPR_SCENE_CAMERA:
				{
					
					FrNode* cameraRpr = node->GetProperty<FrNode*>(RPR_SCENE_CAMERA);
					
					if ( cameraRpr )
					{
						CameraObject* cameraBaikal = cameraRpr->GetProperty<CameraObject*>(FR_NODE_BAIKAL_NODE);
						tahoeNodeScene_->SetCamera(cameraBaikal);
					}
					else
					{
						tahoeNodeScene_->SetCamera(nullptr);
					}

					break;
				}

				case RPR_SCENE_SHAPE_LIST:
				{


					//Tahoe::Scene* tahoeNodeSceneCast = dynamic_cast<Tahoe::Scene*>(tahoeNodeScene_);


					// Get additional argument information for event.
					FireSG::ListChangedArgs* lcargs = reinterpret_cast<FireSG::ListChangedArgs*>(args);

					FrNode* shape = reinterpret_cast<FrNode*>(lcargs->Item());

					// Get the Tahoe::Node pointer stored in the FireRender node object.
					ShapeObject* tahoeShapeNode = shape->GetProperty<ShapeObject*>(FR_NODE_BAIKAL_NODE);

					// Add or remove the Tahoe::Node from the scene.
					switch (lcargs->Operation())
					{
					case FireSG::ListChangedArgs::Op::ItemAdded:
						//m_api->addToScene( tahoeNodeSceneCast, tahoeShapeNode );
						tahoeNodeScene_->AttachShape(tahoeShapeNode);
						break;

					case FireSG::ListChangedArgs::Op::ItemRemoved:
						//m_api->removeFromScene(  tahoeNodeSceneCast, tahoeShapeNode );
						tahoeNodeScene_->DetachShape(tahoeShapeNode);
						break;
					}

					break;
				}

				case RPR_SCENE_LIGHT_LIST:
				{


					//Tahoe::Scene* tahoeNodeSceneCast = dynamic_cast<Tahoe::Scene*>(tahoeNodeScene_);


					// Get additional argument information for event.
					FireSG::ListChangedArgs* lcargs = reinterpret_cast<FireSG::ListChangedArgs*>(args);

					FrNode* light = reinterpret_cast<FrNode*>(lcargs->Item());

					// Get the Tahoe::Node pointer stored in the FireRender node object.
					LightObject* tahoeLightNode = light->GetProperty<LightObject*>(FR_NODE_BAIKAL_NODE);

					// Add or remove the Tahoe::Node from the scene.
					switch (lcargs->Operation())
					{
					case FireSG::ListChangedArgs::Op::ItemAdded:
						//m_api->addToScene( tahoeNodeSceneCast, tahoeShapeNode );
						tahoeNodeScene_->AttachLight(tahoeLightNode);
						break;

					case FireSG::ListChangedArgs::Op::ItemRemoved:
						//m_api->removeFromScene(  tahoeNodeSceneCast, tahoeShapeNode );
						tahoeNodeScene_->DetachLight(tahoeLightNode);
						break;
					}

					break;
				}


				default:
				{
					int aa=0;
					break;

				}

			};

			break;
		}


		case NodeTypes::Camera:
		{
			CameraObject* tahoeNodeCamera_ = node->GetProperty<CameraObject*>(FR_NODE_BAIKAL_NODE);

			switch (propertyKey)
			{

				case RPR_CAMERA_MODE:
				{

					rpr_camera_mode newMode = node->GetProperty<rpr_camera_mode>(RPR_CAMERA_MODE);

					//TODO: only perspective camera used for now
					switch (newMode)
					{
					case RPR_CAMERA_MODE_PERSPECTIVE:
					case RPR_CAMERA_MODE_ORTHOGRAPHIC:
						tahoeNodeCamera_->SetMode(newMode);
						break;
					default:
						//UNIMLEMENTED_FUNCTION
						int aa=0;
						break;
					}

					break;
				}

				case RPR_CAMERA_SENSOR_SIZE:
				{
					rpr_float2 sensorSize = node->GetProperty<rpr_float2>(RPR_CAMERA_SENSOR_SIZE);
					tahoeNodeCamera_->SetSensorSize(   *((RadeonRays::float2*)&sensorSize)   );
					break;
				}

				case RPR_CAMERA_POSITION:
				case RPR_CAMERA_LOOKAT:
				case RPR_CAMERA_UP:
				{
					ResetCameraPosLookUp(node);
					break;
				}



				case RPR_CAMERA_TRANSFORM:
				{
					int a=0;
					break;
				}


				case RPR_CAMERA_FOCAL_LENGTH :
				{
					rpr_float fl = node->GetProperty<rpr_float>(RPR_CAMERA_FOCAL_LENGTH);
					tahoeNodeCamera_->SetFocalLength(fl);
					break;
				}

				case RPR_CAMERA_FOCUS_DISTANCE :
				{
					rpr_float fl = node->GetProperty<rpr_float>(RPR_CAMERA_FOCUS_DISTANCE);
					tahoeNodeCamera_->SetFocusDistance(fl);
					break;
				}

				case RPR_CAMERA_FSTOP :
				{
					rpr_float fl = node->GetProperty<rpr_float>(RPR_CAMERA_FSTOP);
					tahoeNodeCamera_->SetAperture(fl);
					break;
				}

				case RPR_CAMERA_ORTHO_WIDTH :
				{
					rpr_float fl = node->GetProperty<rpr_float>(RPR_CAMERA_ORTHO_WIDTH);
					tahoeNodeCamera_->SetOrthoWidth(fl);
					break;
				}

				case RPR_CAMERA_ORTHO_HEIGHT:
				{
					rpr_float fl = node->GetProperty<rpr_float>(RPR_CAMERA_ORTHO_HEIGHT);
					tahoeNodeCamera_->SetOrthoHeight(fl);
					break;
				}


				default:
				{
					int aa=0;
					break;
				}
			}

			break;
		}


		case NodeTypes::Image:
		{
			MaterialObject* tahoeNodeImage_ = node->GetProperty<MaterialObject*>(FR_NODE_BAIKAL_NODE);

			switch (propertyKey)
			{

				case RPR_OBJECT_NAME:
				{
					//good for debugging to set the names inside Tahoe too
					//Tahoe::Node* tahoeNode = node->GetProperty<Tahoe::Node*>(FR_NODE_TAHOE_NODE);
					auto objectName = node->GetProperty<std::string>(RPR_OBJECT_NAME);
					tahoeNodeImage_->SetName(objectName.c_str());
					break;
				}

				default:
				{
					int aa=0;
					break;
				}
			}


			break;
		}
		
		case NodeTypes::DirectionalLight:
		{
			LightObject* tahoeNodeLight_ = node->GetProperty<LightObject*>(FR_NODE_BAIKAL_NODE);

			switch (propertyKey)
			{

				case RPR_OBJECT_NAME:
				{
					//good for debugging to set the names inside Tahoe too
					//Tahoe::Node* tahoeNode = node->GetProperty<Tahoe::Node*>(FR_NODE_TAHOE_NODE);
					auto objectName = node->GetProperty<std::string>(RPR_OBJECT_NAME);
					tahoeNodeLight_->SetName(objectName.c_str());
					break;
				}

				case RPR_LIGHT_TRANSFORM:
				{
					RadeonProRender::matrix transform = node->GetProperty<RadeonProRender::matrix>(RPR_LIGHT_TRANSFORM);

					tahoeNodeLight_->SetTransform(   *((RadeonRays::matrix*)&transform)    );
					break;
				}

				case RPR_DIRECTIONAL_LIGHT_RADIANT_POWER:
				{
					RadeonProRender::float3 radiantPower = node->GetProperty<RadeonProRender::float3>(RPR_DIRECTIONAL_LIGHT_RADIANT_POWER);
					tahoeNodeLight_->SetRadiantPower( *((RadeonRays::float3*)&radiantPower)   );
					break;
				}


				default:
				{
					int aa=0;
					break;
				}
			}

			break;
		}


		case NodeTypes::EnvironmentLight:
		{
			LightObject* tahoeNodeLight_ = node->GetProperty<LightObject*>(FR_NODE_BAIKAL_NODE);

			switch (propertyKey)
			{

				case RPR_OBJECT_NAME:
				{
					//good for debugging to set the names inside Tahoe too
					//Tahoe::Node* tahoeNode = node->GetProperty<Tahoe::Node*>(FR_NODE_TAHOE_NODE);
					auto objectName = node->GetProperty<std::string>(RPR_OBJECT_NAME);
					tahoeNodeLight_->SetName(objectName.c_str());
					break;
				}

				case RPR_ENVIRONMENT_LIGHT_IMAGE:
				{
					FrNode* image = node->GetProperty<FrNode*>(RPR_ENVIRONMENT_LIGHT_IMAGE);
					MaterialObject* img = image->GetProperty<MaterialObject*>(FR_NODE_BAIKAL_NODE);
					tahoeNodeLight_->SetEnvTexture(img);
					break;
				}

				default:
				{
					int aa=0;
					break;
				}
			}

			break;
		}


		case NodeTypes::PointLight:
		{
			LightObject* tahoeNodeLight_ = node->GetProperty<LightObject*>(FR_NODE_BAIKAL_NODE);

			switch (propertyKey)
			{

				case RPR_OBJECT_NAME:
				{
					//good for debugging to set the names inside Tahoe too
					//Tahoe::Node* tahoeNode = node->GetProperty<Tahoe::Node*>(FR_NODE_TAHOE_NODE);
					auto objectName = node->GetProperty<std::string>(RPR_OBJECT_NAME);
					tahoeNodeLight_->SetName(objectName.c_str());
					break;
				}

				case RPR_LIGHT_TRANSFORM:
				{
					RadeonProRender::matrix transform = node->GetProperty<RadeonProRender::matrix>(RPR_LIGHT_TRANSFORM);

					tahoeNodeLight_->SetTransform(   *((RadeonRays::matrix*)&transform)   );
					break;
				}

				case RPR_POINT_LIGHT_RADIANT_POWER:
				{
					RadeonProRender::float3 radiantPower = node->GetProperty<RadeonProRender::float3>(RPR_POINT_LIGHT_RADIANT_POWER);
					tahoeNodeLight_->SetRadiantPower(  *((RadeonRays::float3*)&radiantPower)   );
					break;
				}

				default:
				{
					int aa=0;
					break;
				}
			}

			break;
		}




		case NodeTypes::SpotLight:
		{
			LightObject* tahoeNodeLight_ = node->GetProperty<LightObject*>(FR_NODE_BAIKAL_NODE);

			switch (propertyKey)
			{

				case RPR_OBJECT_NAME:
				{
					//good for debugging to set the names inside Tahoe too
					//Tahoe::Node* tahoeNode = node->GetProperty<Tahoe::Node*>(FR_NODE_TAHOE_NODE);
					auto objectName = node->GetProperty<std::string>(RPR_OBJECT_NAME);
					tahoeNodeLight_->SetName(objectName.c_str());
					break;
				}

				case RPR_LIGHT_TRANSFORM:
				{
					RadeonProRender::matrix transform = node->GetProperty<RadeonProRender::matrix>(RPR_LIGHT_TRANSFORM);

					tahoeNodeLight_->SetTransform(   *((RadeonRays::matrix*)&transform)    );
					break;
				}

				case RPR_SPOT_LIGHT_RADIANT_POWER:
				{
					RadeonProRender::float3 radiantPower = node->GetProperty<RadeonProRender::float3>(RPR_SPOT_LIGHT_RADIANT_POWER);
					tahoeNodeLight_->SetRadiantPower(   *((RadeonRays::float3*)&radiantPower)    );
					break;
				}

				case RPR_SPOT_LIGHT_CONE_SHAPE:
				{
					RadeonProRender::float2 val = node->GetProperty<RadeonProRender::float2>(RPR_SPOT_LIGHT_CONE_SHAPE);
					tahoeNodeLight_->SetSpotConeShape(    *((RadeonRays::float2*)&val)     );
					break;
				}

				default:
				{
					int aa=0;
					break;
				}
			}

			break;
		}





		case NodeTypes::Mesh:
		case NodeTypes::Instance:
		{
			ShapeObject* tahoeNodeShape_ = node->GetProperty<ShapeObject*>(FR_NODE_BAIKAL_NODE);

			switch (propertyKey)
			{

				case RPR_OBJECT_NAME:
				{
					//good for debugging to set the names inside Tahoe too
					//Tahoe::Node* tahoeNode = node->GetProperty<Tahoe::Node*>(FR_NODE_TAHOE_NODE);
					auto objectName = node->GetProperty<std::string>(RPR_OBJECT_NAME);
					tahoeNodeShape_->SetName(objectName.c_str());
					break;
				}

				case RPR_SHAPE_MATERIAL:
				{
					FrNode* shapeMaterialNode = node->GetProperty<FrNode*>(RPR_SHAPE_MATERIAL);
					if ( shapeMaterialNode )
					{
						MaterialObject* mat = shapeMaterialNode->GetProperty<MaterialObject*>(FR_NODE_BAIKAL_NODE);
						tahoeNodeShape_->SetMaterial(mat);
					}
					else
					{
						tahoeNodeShape_->SetMaterial(nullptr);
					}
					break;
				}

				case RPR_SHAPE_TRANSFORM:
				{
					RadeonProRender::matrix m = node->GetProperty<RadeonProRender::matrix>(RPR_SHAPE_TRANSFORM);

					tahoeNodeShape_->SetTransform(   *((RadeonRays::matrix*)&m)    );

					break;
				}


				default:
				{
					int aa=0;
					break;
				}
			}

			break;
		}




		case NodeTypes::Material:
		{
			MaterialObject* tahoeNodeMaterial_ = node->GetProperty<MaterialObject*>(FR_NODE_BAIKAL_NODE);

			switch (propertyKey)
			{
				case RPR_OBJECT_NAME:
				{
					//good for debugging to set the names inside Tahoe too
					//Tahoe::Node* tahoeNode = node->GetProperty<Tahoe::Node*>(FR_NODE_TAHOE_NODE);
					auto objectName = node->GetProperty<std::string>(RPR_OBJECT_NAME);
					tahoeNodeMaterial_->SetName(objectName.c_str());
					break;
				}

				default:
				{
					//mat->SetInputValue(in_input, img);
					
					auto name_it = kRPRInputStrings.find(propertyKey);
					auto name_it2 = kMaterialNodeInputStrings.find(propertyKey);
					if ( name_it != kRPRInputStrings.end() )
					{
						/*
						size_t valueType = node->GetPropertyType(propertyKey);

						if (valueType == GET_TYPE_ID(rpr_uint))
						{
							rpr_uint value = node->GetProperty<rpr_uint>(propertyKey);
							tahoeNodeMaterial_->SetInputValue(name_it->second.c_str(), value);
						}
						else if (valueType == GET_TYPE_ID(rpr_float4))
						{
							int aa=0;
						}
						else if (valueType == GET_TYPE_ID(FrNode*))
						{
							int aa=0;
						}
						else
						{
							int a=0;
						}
						*/

						const std::shared_ptr<RprMaterialNode> valueType = node->GetProperty<std::shared_ptr<RprMaterialNode>>(propertyKey);

						if ( valueType->m_type == RprMaterialNode::MATNODETYPE::UINT1 )
						{
							tahoeNodeMaterial_->SetInputValue(name_it->second.c_str(), valueType->m_uint);
						}
						else if ( valueType->m_type == RprMaterialNode::MATNODETYPE::FLOAT4 )
						{
							tahoeNodeMaterial_->SetInputValue(name_it->second.c_str(),      *((RadeonRays::float4*)&valueType->m_float4)     );
						}
						else if ( valueType->m_type == RprMaterialNode::MATNODETYPE::FRNODE )
						{
							MaterialObject* input_ = valueType->m_node->GetProperty<MaterialObject*>(FR_NODE_BAIKAL_NODE);
							tahoeNodeMaterial_->SetInputValue(name_it->second.c_str(), input_);
						}
						else
						{
							int aa=0;
						}

						

						int aaa=0;
						
					}


					else if ( name_it2 != kMaterialNodeInputStrings.end() )
					{
						const std::shared_ptr<RprMaterialNode> valueType = node->GetProperty<std::shared_ptr<RprMaterialNode>>(propertyKey);

						if ( valueType->m_type == RprMaterialNode::MATNODETYPE::UINT1 )
						{
							tahoeNodeMaterial_->SetInputValue(name_it2->second.c_str(), valueType->m_uint);
						}
						else if ( valueType->m_type == RprMaterialNode::MATNODETYPE::FLOAT4 )
						{
							tahoeNodeMaterial_->SetInputValue(name_it2->second.c_str(),   *((RadeonRays::float4*)&valueType->m_float4)    );
						}
						else if ( valueType->m_type == RprMaterialNode::MATNODETYPE::FRNODE )
						{
							//valueType->m_node

				

							MaterialObject* input_ = valueType->m_node->GetProperty<MaterialObject*>(FR_NODE_BAIKAL_NODE);


							tahoeNodeMaterial_->SetInputValue(name_it2->second.c_str(), input_);
						}
						else
						{
							int aa=0;
						}

						

						int aaa=0;
					}


					else
					{
						int aa=0;
					}

					int aa=0;
					


					//OnMaterialNodePropertyChanged(node, propertyKey, args);



					break;
				}
			}


			break;
		}




		default:
		{
			//throw FrException(__FILE__,__LINE__,RPR_ERROR_INVALID_PARAMETER_TYPE, "OnPropertyChanged: Invalid parameter", m_context);
			int aa=0;
			break;

		}
	}

}

void Renderer::GetRenderStatistics(rpr_render_statistics& stat)
{
	if ( m_contextBaikal )
	{
		m_contextBaikal->GetRenderStatistics(&stat, nullptr);
	}
}

std::size_t Renderer::GetInfoSize(rpr_context_info info) const
{
	return 0;
}

void Renderer::GetInfo(rpr_context_info info, void* data) const
{
}

std::size_t Renderer::GetInfoSize(FrNode* node, rpr_uint info) const
{
	return 0;
}

void Renderer::GetInfo(FrNode* node, rpr_uint info, void* data) const
{
}

rpr_uint Renderer::GetUint(FrNode* node, std::uint32_t key) const
{
	return 0;
}

rpr_float4 Renderer::GetFloat4(FrNode* node, std::uint32_t key) const
{
	return rpr_float4(0.f,0.f,0.f,0.f);
}


void Renderer::GetSceneAABB(fr_float* aabb) const
{
}

rpr_uint Renderer::GetMaterialSystemSize() const
{
	return 0;
}

void Renderer::AdaptSubdiv(FrNode* shape , FrNode* camera, FrNode* framebuffer, int coeff)
{
}

void Renderer::SetAOVindexLookup(int index, rpr_float4 color)
{
}


FrRenderer* CreateRendererInstance()
{
	return new Shasta::Renderer();
}

void DeleteRendererInstance(FrRenderer* instance)
{
	delete static_cast<Shasta::Renderer*>(instance);
}
















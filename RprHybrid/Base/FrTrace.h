/*****************************************************************************\
*
*  Module Name    FrTrace.h
*  Project        FireRender Engine
*
*  Description    FireRender trace & log
*
*  Copyright 2011 - 2013 Advanced Micro Devices, Inc. (unpublished)
*
*  All rights reserved.  This notice is intended as a precaution against
*  inadvertent publication and does not imply publication or any waiver
*  of confidentiality.  The year included in the foregoing notice is the
*  year of creation of the work.
*
\*****************************************************************************/

#ifndef FR_TRACE_HPP
#define FR_TRACE_HPP

#include "Common.h"

#include <fstream>

class Logger
{
public:

	enum Type
	{
		TYPE_NONE,
		TYPE_FILE,
		TYPE_CONSOLE,
	};

	enum TRACE_ADDRESS_TYPE
	{
		TRACE_ADDRESS_TYPE__LIGHT,
		TRACE_ADDRESS_TYPE__IMAGE,
		TRACE_ADDRESS_TYPE__CAMERA,
		TRACE_ADDRESS_TYPE__FRAMEBUFFER,
		TRACE_ADDRESS_TYPE__SCENE,
		TRACE_ADDRESS_TYPE__SHAPE,
		TRACE_ADDRESS_TYPE__MATERIALSYSTEM,
		TRACE_ADDRESS_TYPE__MATERIALNODE,
		TRACE_ADDRESS_TYPE__CONTEXT,
		//TRACE_ADDRESS_TYPE__TAHOEPLUGINID, pluginID is not an address but an index
		TRACE_ADDRESS_TYPE__POSTEFFECT,
		TRACE_ADDRESS_TYPE__COMPOSITE,
		TRACE_ADDRESS_TYPE__BUFFER,
		TRACE_ADDRESS_TYPE__HETEROVOLUME,
		TRACE_ADDRESS_TYPE__LUT,
	};

	Logger();
	~Logger();

	void StartTrace();
	void StopTrace();

	void SetTracingFolder(const char* tracingFolder);

	//call  PauseTracing & ContinueTracing  when we don't want to trace part of source code
	//a case where we need those function is when internal functions call call exposed API function directly. in this case, we don't want to trace this internal call
	void PauseTracing();
	void ContinueTracing();

	void setType(Type type);
	void setLogPath(const char* path);

	void printLog(const char* funcName, int n, ...);

	void printTrace(const char* szfmt, ...);
	void printPlayerTraceCpp(const char* szfmt, ...);
	void printPlayerTraceH(const char* szfmt, ...);
	void printVariable(const char* szfmt, ...);
	void printPlaylist(const char* szfmt, ...);
	

	void TraceArg__COMMA();
	void TraceArg__STATUS();

	void TraceArg__rpr_int(rpr_int a);
	void TraceArg__rpr_int_hexa(rpr_int a);
	void TraceArg__int(int a);
	void TraceArg__uint(uint a);
	void TraceArg__rpr_uint(rpr_uint a);
	void TraceArg__rpr_float(rpr_float a);
	void TraceArg__rpr_bool(rpr_bool a);
	void TraceArg__size_t(size_t a);
	void TraceArg__rpr_hetero_volume_indices_topology(rpr_hetero_volume_indices_topology a);
	void TraceArg__rpr_char_P(const rpr_char* a);
	void TraceArg__rpr_material_node(rpr_material_node a);
	void TraceArg__rpr_composite(rpr_composite a);
	void TraceArg__rpr_lut(rpr_lut a);
	void TraceArg__rpr_context(rpr_context a);
	void TraceArg__rpr_post_effect(rpr_post_effect a);
	void TraceArg__rpr_material_system_type(rpr_material_system_type a);
	void TraceArg__rpr_aov(rpr_aov a);
	void TraceArg__rpr_material_node_input(rpr_material_node_input a);
	void TraceArg__rpr_camera(rpr_camera a);
	void TraceArg__rpr_shape(rpr_shape a);
	void TraceArg__rpr_tahoePluginID(rpr_int a);
	void TraceArg__rpr_light(rpr_light a);
	void TraceArg__rpr_scene(rpr_scene a);
	void TraceArg__rpr_environment_override(rpr_environment_override a);
	void TraceArg__rpr_subdiv_boundary_interfop_type(rpr_subdiv_boundary_interfop_type a);
	void TraceArg__rpr_image_wrap_type(rpr_image_wrap_type a);
	void TraceArg__rpr_image_filter_type(rpr_image_filter_type a);
	void TraceArg__rpr_image(rpr_image a);
	void TraceArg__rpr_buffer(rpr_buffer a);
	void TraceArg__rpr_framebuffer(rpr_framebuffer a);
	void TraceArg__rpr_hetero_volume(rpr_hetero_volume a);
	void TraceArg__rpr_hetero_volume_filter(rpr_hetero_volume_filter a);
	void TraceArg__rpr_undef(const void* a);
	void TraceArg__rpr_camera_mode(rpr_camera_mode a);
	void TraceArg__rpr_creation_flags(rpr_creation_flags a);
	void TraceArg__rpr_render_mode(rpr_render_mode a);
	void TraceArg__rpr_mesh_polygon_vertex_info(rpr_mesh_polygon_vertex_info a);
	void TraceArg__rpr_material_system(rpr_material_system a);
	void TraceArg__rpr_material_node_type(rpr_material_node_type a);
	void TraceArg__rpr_post_effect_type(rpr_post_effect_type a);
	void TraceArg__rpr_material_node_arithmetic_operation(rpr_material_node_arithmetic_operation a);
	void TraceArg__rpr_composite_type(rpr_composite_type a);

	void TraceArg_Prepare__rpr_framebuffer_format(rpr_framebuffer_format a);
	void TraceArg_Use__rpr_framebuffer_format(rpr_framebuffer_format a);

	void TraceArg_Prepare__rpr_image_format(rpr_image_format a);
	void TraceArg_Use__rpr_image_format(rpr_image_format a);

	void TraceArg_Prepare__rpr_framebuffer_desc_P(const rpr_framebuffer_desc* a);
	void TraceArg_Use__rpr_framebuffer_desc_P(const rpr_framebuffer_desc* a);

	void TraceArg_Prepare__rpr_image_desc_P(const rpr_image_desc* a);
	void TraceArg_Use__rpr_image_desc_P(const rpr_image_desc* a);

	void TraceArg_Prepare__rpr_buffer_desc_P(const rpr_buffer_desc* a);
	void TraceArg_Use__rpr_buffer_desc_P(const rpr_buffer_desc* a);

	void TraceArg_Prepare__rpr_float_P16(const rpr_float* a);
	void TraceArg_Use__rpr_float_P16(const rpr_float* a);

	void TraceArg_Prepare__rpr_tahoePluginIDlist(const rpr_int* a, size_t numberOfId);
	void TraceArg_Use__rpr_tahoePluginIDlist(const rpr_int* a);

	void TraceArg_Prepare__rpr_context_properties_P(const rpr_context_properties* a);
	void TraceArg_Use__rpr_context_properties_P(const rpr_context_properties* a);

	void TraceArg_Prepare__rpr_mesh_info_P(const rpr_mesh_info* a);
	void TraceArg_Use__rpr_mesh_info_P(const rpr_mesh_info* a);


	void TraceArg_Prepare__DATA(const void* data, unsigned long long dataSize, const char* varName);
	void TraceArg_Prepare__PDATA(const void** data, unsigned long long* dataSize, int numberOfData, const char* varName);
	void TraceArg_Use__DATA_const_rpr_float_P(const char* varName);
	void TraceArg_Use__DATA_const_rpr_float_PP(const char* varName);
	void TraceArg_Use__DATA_const_rpr_int_P(const char* varName);
	void TraceArg_Use__DATA_const_rpr_int_PP(const char* varName);
	void TraceArg_Use__DATA_const_size_t_P(const char* varName);
	void TraceArg_Use__DATA_const_void_P(const char* varName);

	void Trace__FunctionOpen(const char* functionName);
	void Trace__FunctionFailed(void* frNode, const char* functionName,rpr_int errorCode);
	void Trace__FunctionClose();

	bool IsTraceActivated() { return traceActivated; }

	unsigned long long GetCursorDataFile() { return cursorDataFile; };

	void Trace__NewFrObjectCreated(TRACE_ADDRESS_TYPE type, const void* address);

	void Trace__rprContextSetAOV(rpr_context context, rpr_aov aov, rpr_framebuffer frame_buffer);

	void Trace__CommentPossibilityExport_contex(rpr_context context);
	void Trace__CommentPossibilityExport_framebuf(rpr_framebuffer frame_buffer);
	
	//example : declar="rpr_light"  prefix="light_0x"    obj=address of fireRender object
	//Trace__DeclareFRobject will declare this object if the object isn't already declared
	void Trace__DeclareFRobject(const char* declar, const char* prefix, void* obj);

	//flush the trace files.
	void Trace__FlushAllFiles();

	void SetupNextRecordPage();

	//import an external file : example "C:\\path\\image.tga"
	//to a local trace folder : example : "externalFile_1.tga"
	// newFileNameLocal is an out parameter, it contains the name of the local file
	void ImportFileInTraceFolders(const rpr_char* filePath, std::string& newFileNameLocal);

private:
	Type m_type;

	FILE* flog;
	FILE* flog_variables;
	FILE* flog_playList;
	FILE* flog_playerClassCpp;
	FILE* flog_playerClassH;
	std::fstream flog_data;
	unsigned long long cursorDataFile;

	int trace_rpr_framebuffer_format_count_created;
	int trace_rpr_framebuffer_format_count_used;
	int trace_rpr_context_properties_count_created;
	int trace_rpr_context_properties_count_used;
	int trace_rpr_mesh_info_count_created;
	int trace_rpr_mesh_info_count_used;
	int trace_rpr_framebuffer_desc_P_count_created;
	int trace_rpr_framebuffer_desc_P_count_used;
	int trace_rpr_image_desc_P_count_created;
	int trace_rpr_image_desc_P_count_used;
	int trace_rpr_buffer_desc_P_count_created;
	int trace_rpr_buffer_desc_P_count_used;
	int trace_rpr_float_P16_count_created;
	int trace_rpr_float_P16_count_used;
	int trace_rpr_image_format_count_created;
	int trace_rpr_image_format_count_used;
	int trace_rpr_tahoePluginIDlist_count_created;
	int trace_rpr_tahoePluginIDlist_count_used;

	bool traceActivated; //  don't set this variable directly. switch it with  StartTrace/StopTrace
	int m_tracingPaused; // don't set this variable directly. set it with  PauseTracing/ContinueTracing.

	std::map<const void*, TRACE_ADDRESS_TYPE> m_trace_addressToType;
	
	// contains the list of all declared objects
	// example : "light_0x0000000046884D88" , "materialnode_0x000000006319F2B0" ...
	std::vector<std::string> m_trace_listDeclaredFrObject; 
	
	std::map<rpr_context, rpr_framebuffer> m_mapContextToFramebuffer; // updated each time we call  rprContextSetAOV(,RPR_AOV_COLOR,)

	int m_trace_countExportImage; // increment each time we add a "rprFrameBufferSaveToFile" comment

	std::string m_folderOfTraceFile;

	int m_recordFileIndex;

	int m_printfFileCount;

	static const int m_nbInstructionsPerFile;

	bool m_traceForcedByEnvVariable;


	static int m_fileImportedCounter;
};


#endif
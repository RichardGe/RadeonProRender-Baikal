set(SOURCES
    RadeonProRender.cpp
    RadeonProRender.h
    RadeonProRender_CL.h
    RadeonProRender_GL.h
    Wrap.cpp
	Base/FrException.cpp
	Base/FrTrace.cpp
	Base/Library.cpp
	Node/FrNode.cpp
	Cache/SimpleImageCache.cpp
	Cache/FrImageCache.cpp
	Renderer/FrRenderer.cpp
	../BaikalRendererDLL/ImageIo/libTiff/tif_aux.cpp
	../BaikalRendererDLL/ImageIo/libTiff/tif_close.cpp
	../BaikalRendererDLL/ImageIo/libTiff/tif_codec.cpp
	../BaikalRendererDLL/ImageIo/libTiff/tif_color.cpp
	../BaikalRendererDLL/ImageIo/libTiff/tif_compress.cpp
	../BaikalRendererDLL/ImageIo/libTiff/tif_dir.cpp
	../BaikalRendererDLL/ImageIo/libTiff/tif_dirinfo.cpp
	../BaikalRendererDLL/ImageIo/libTiff/tif_dirread.cpp
	../BaikalRendererDLL/ImageIo/libTiff/tif_dirwrite.cpp
	../BaikalRendererDLL/ImageIo/libTiff/tif_dumpmode.cpp
	../BaikalRendererDLL/ImageIo/libTiff/tif_error.cpp
	../BaikalRendererDLL/ImageIo/libTiff/tif_extension.cpp
	../BaikalRendererDLL/ImageIo/libTiff/tif_fax3.cpp
	../BaikalRendererDLL/ImageIo/libTiff/tif_fax3sm.cpp
	../BaikalRendererDLL/ImageIo/libTiff/tif_flush.cpp
	../BaikalRendererDLL/ImageIo/libTiff/tif_getimage.cpp
	../BaikalRendererDLL/ImageIo/libTiff/tif_jbig.cpp
	../BaikalRendererDLL/ImageIo/libTiff/tif_luv.cpp
	../BaikalRendererDLL/ImageIo/libTiff/tif_lzma.cpp
	../BaikalRendererDLL/ImageIo/libTiff/tif_lzw.cpp
	../BaikalRendererDLL/ImageIo/libTiff/tif_next.cpp
	../BaikalRendererDLL/ImageIo/libTiff/tif_ojpeg.cpp
	../BaikalRendererDLL/ImageIo/libTiff/tif_open.cpp
	../BaikalRendererDLL/ImageIo/libTiff/tif_packbits.cpp
	../BaikalRendererDLL/ImageIo/libTiff/tif_pixarlog.cpp
	../BaikalRendererDLL/ImageIo/libTiff/tif_predict.cpp
	../BaikalRendererDLL/ImageIo/libTiff/tif_print.cpp
	../BaikalRendererDLL/ImageIo/libTiff/tif_read.cpp
	../BaikalRendererDLL/ImageIo/libTiff/tif_strip.cpp
	../BaikalRendererDLL/ImageIo/libTiff/tif_swab.cpp
	../BaikalRendererDLL/ImageIo/libTiff/tif_thunder.cpp
	../BaikalRendererDLL/ImageIo/libTiff/tif_tile.cpp
	../BaikalRendererDLL/ImageIo/libTiff/tif_unix.cpp
	../BaikalRendererDLL/ImageIo/libTiff/tif_version.cpp
	../BaikalRendererDLL/ImageIo/libTiff/tif_vms.cpp
	../BaikalRendererDLL/ImageIo/libTiff/tif_warning.cpp
	../BaikalRendererDLL/ImageIo/libTiff/tif_win32.cpp
	../BaikalRendererDLL/ImageIo/libTiff/tif_wince.cpp
	../BaikalRendererDLL/ImageIo/libTiff/tif_write.cpp
	../BaikalRendererDLL/ImageIo/libTiff/tif_zip.cpp
	PluginManager.cpp

	)

set(WRAP_OBJECT_SOURCES
)

set(UTILS_SOURCES
)

source_group("WrapObject" FILES ${WRAP_OBJECT_SOURCES})
source_group("Utils" FILES ${UTILS_SOURCES})

list(APPEND SOURCES ${WRAP_OBJECT_SOURCES} ${UTILS_SOURCES})

add_library(RadeonProRender SHARED ${SOURCES})
target_compile_definitions(RadeonProRender PRIVATE RPR_EXPORT_API RPR_FOR_BAIKAL)
target_compile_features(RadeonProRender PRIVATE cxx_std_14)
target_include_directories(RadeonProRender PUBLIC ${Baikal_SOURCE_DIR}/Rpr
${Baikal_SOURCE_DIR}
)
target_link_libraries(RadeonProRender PUBLIC Baikal BaikalIO OpenGL::GL GLEW::GLEW)

    

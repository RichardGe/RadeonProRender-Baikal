set(SOURCES
    Renderer.cpp
    Renderer.h
#    Renderer/FrRenderer.cpp
#    Renderer/FrRenderer.h
#	PluginManager.cpp
#	Base/Library.cpp
	Utils/config_manager.cpp
	../Rpr/Base/FrException.cpp
	../Rpr/Base/FrException.h
	)

set(WRAP_OBJECT_SOURCES
    WrapObject/CameraObject.cpp
    WrapObject/CameraObject.h
    WrapObject/ContextObject.cpp
    WrapObject/ContextObject.h
    WrapObject/FramebufferObject.cpp
    WrapObject/FramebufferObject.h
    WrapObject/LightObject.cpp
    WrapObject/LightObject.h
    WrapObject/Materials/ArithmeticMaterialObject.cpp
    WrapObject/Materials/ArithmeticMaterialObject.h
    WrapObject/Materials/ImageMaterialObject.cpp
    WrapObject/Materials/ImageMaterialObject.h
    WrapObject/Materials/MaterialObject.cpp
    WrapObject/Materials/MaterialObject.h
    WrapObject/Materials/TextureMaterialObject.cpp
    WrapObject/Materials/TextureMaterialObject.h
    WrapObject/Materials/UberMaterialObject.cpp
    WrapObject/Materials/UberMaterialObject.h
    WrapObject/Materials/UnsupportedMaterialObject.cpp
    WrapObject/Materials/UnsupportedMaterialObject.h
    WrapObject/MatSysObject.cpp
    WrapObject/MatSysObject.h
    WrapObject/SceneObject.cpp
    WrapObject/SceneObject.h
    WrapObject/ShapeObject.cpp
    WrapObject/ShapeObject.h
    WrapObject/WrapObject.h)

set(UTILS_SOURCES

)

source_group("WrapObject" FILES ${WRAP_OBJECT_SOURCES})
source_group("Utils" FILES ${UTILS_SOURCES})

list(APPEND SOURCES ${WRAP_OBJECT_SOURCES} ${UTILS_SOURCES})

add_library(BaikalRendererDLL SHARED ${SOURCES})
target_compile_definitions(BaikalRendererDLL PRIVATE RPR_EXPORT_API)
target_compile_features(BaikalRendererDLL PRIVATE cxx_std_14)
target_include_directories(BaikalRendererDLL PUBLIC ${Baikal_SOURCE_DIR}/BaikalRendererDLL
${Baikal_SOURCE_DIR}
)
target_link_libraries(BaikalRendererDLL PUBLIC Baikal BaikalIO OpenGL::GL GLEW::GLEW)

    

set(SOURCES
    Renderer.cpp
    Renderer.h
	../Rpr/Utils/config_manager.cpp
	../RprHybrid/Base/FrException.cpp
	../RprHybrid/Base/FrException.h
	)

set(WRAP_OBJECT_SOURCES
    ../Rpr/WrapObject/CameraObject.cpp
    ../Rpr/WrapObject/CameraObject.h
    ../Rpr/WrapObject/ContextObject.cpp
    ../Rpr/WrapObject/ContextObject.h
    ../Rpr/WrapObject/FramebufferObject.cpp
    ../Rpr/WrapObject/FramebufferObject.h
    ../Rpr/WrapObject/LightObject.cpp
    ../Rpr/WrapObject/LightObject.h
    ../Rpr/WrapObject/Materials/ArithmeticMaterialObject.cpp
    ../Rpr/WrapObject/Materials/ArithmeticMaterialObject.h
    ../Rpr/WrapObject/Materials/ImageMaterialObject.cpp
    ../Rpr/WrapObject/Materials/ImageMaterialObject.h
    ../Rpr/WrapObject/Materials/MaterialObject.cpp
    ../Rpr/WrapObject/Materials/MaterialObject.h
    ../Rpr/WrapObject/Materials/TextureMaterialObject.cpp
    ../Rpr/WrapObject/Materials/TextureMaterialObject.h
    ../Rpr/WrapObject/Materials/UberMaterialObject.cpp
    ../Rpr/WrapObject/Materials/UberMaterialObject.h
    ../Rpr/WrapObject/Materials/UnsupportedMaterialObject.cpp
    ../Rpr/WrapObject/Materials/UnsupportedMaterialObject.h
    ../Rpr/WrapObject/MatSysObject.cpp
    ../Rpr/WrapObject/MatSysObject.h
    ../Rpr/WrapObject/SceneObject.cpp
    ../Rpr/WrapObject/SceneObject.h
    ../Rpr/WrapObject/ShapeObject.cpp
    ../Rpr/WrapObject/ShapeObject.h
    ../Rpr/WrapObject/WrapObject.h)

set(UTILS_SOURCES

)

source_group("WrapObject" FILES ${WRAP_OBJECT_SOURCES})
source_group("Utils" FILES ${UTILS_SOURCES})

list(APPEND SOURCES ${WRAP_OBJECT_SOURCES} ${UTILS_SOURCES})

add_library(BaikalRendererDLL SHARED ${SOURCES})
target_compile_definitions(BaikalRendererDLL PRIVATE RPR_EXPORT_API BAIKAL_RPR_IS_HYBRID_)
target_compile_features(BaikalRendererDLL PRIVATE cxx_std_14)
target_include_directories(BaikalRendererDLL PUBLIC ${Baikal_SOURCE_DIR}/BaikalRendererDLL
${Baikal_SOURCE_DIR}/Rpr
${Baikal_SOURCE_DIR}
)
target_link_libraries(BaikalRendererDLL PUBLIC Baikal BaikalIO OpenGL::GL GLEW::GLEW)

    

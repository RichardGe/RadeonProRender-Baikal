set(SOURCES
    main.cpp
    aov.h
    arithmetic.h
    basic.h
    camera.h
    light.h
    material.h)

add_executable(RprTest ${SOURCES})

if ( BAIKAL_RPR_IS_HYBRID )
target_compile_definitions(RprTest PRIVATE BAIKAL_RPR_IS_HYBRID_)
endif ( BAIKAL_RPR_IS_HYBRID )

target_compile_features(RprTest PRIVATE cxx_std_14)
target_include_directories(RprTest PRIVATE .)
target_link_libraries(RprTest PRIVATE GTest RprSupport RadeonProRender)
set_target_properties(RprTest
    PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY ${Baikal_SOURCE_DIR}/RprTest)

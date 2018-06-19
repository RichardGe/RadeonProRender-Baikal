set(SOURCES RprSupport.cpp)
    
add_library(RprSupport SHARED ${SOURCES})
target_compile_definitions(RprSupport PRIVATE RPR_EXPORT_API)
target_compile_features(RprSupport PRIVATE cxx_std_14)

if ( BAIKAL_RPR_IS_HYBRID )
target_include_directories(RprSupport PUBLIC ${Baikal_SOURCE_DIR}/RprHybrid ${Baikal_SOURCE_DIR}/Rpr ${RprSupport_HEADER})
else()
target_include_directories(RprSupport PUBLIC ${Baikal_SOURCE_DIR}/Rpr ${RprSupport_HEADER})
endif ( BAIKAL_RPR_IS_HYBRID )

target_link_libraries(RprSupport PUBLIC RadeonProRender)






    

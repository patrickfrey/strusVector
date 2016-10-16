cmake_minimum_required(VERSION 2.6 FATAL_ERROR)

# --------------------------------------
# SOURCES AND INCLUDES
# --------------------------------------
set(
	libstrus_vectorspace_source_files
	utils.cpp
	logger.cpp
	internationalization.cpp
	random.cpp
	simHash.cpp
	simGroup.cpp
	sampleSimGroupMap.cpp
	simRelationMap.cpp
	getSimRelationMap.cpp
	lshModel.cpp
	genModel.cpp 
	vectorSpaceModelConfig.cpp
	vectorSpaceModelFiles.cpp
	vectorSpaceModel.cpp
)

include_directories(
	"${Boost_INCLUDE_DIRS}"
	"${Intl_INCLUDE_DIRS}"
	"${PROJECT_BINARY_DIR}/3rdParty/armadillo/include/"
	"${PROJECT_SOURCE_DIR}/3rdParty/armadillo/include/"
	"${PROJECT_SOURCE_DIR}/include"
	"${strusbase_INCLUDE_DIRS}"
)
link_directories(
	"${Boost_LIBRARY_DIRS}"
	"${PROJECT_SOURCE_DIR}/src"
	"${PROJECT_BINARY_DIR}/3rdParty/armadillo"
	"${strusbase_LIBRARY_DIRS}"
)

# ------------------------------
# MODULE 
# ------------------------------
add_library( strus_vectorspace_static STATIC ${libstrus_vectorspace_source_files} )
target_link_libraries( strus_vectorspace_static strus_base strus_armadillo )

add_library( strus_vectorspace SHARED libstrus_vectorspace.cpp )
target_link_libraries( strus_vectorspace strus_vectorspace_static strus_base strus_armadillo ${Intl_LIBRARIES} ${Boost_LIBRARIES} )
set_target_properties(
    strus_vectorspace
    PROPERTIES
    DEBUG_POSTFIX "_d"
    SOVERSION "${STRUS_MAJOR_VERSION}.${STRUS_MINOR_VERSION}"
    VERSION ${STRUS_VERSION}
)

add_library( modstrus_storage_vectorspace  MODULE  modstrus_storage_vectorspace.cpp )
set_target_properties( modstrus_storage_vectorspace PROPERTIES PREFIX "")
target_link_libraries( modstrus_storage_vectorspace strus_module_storage strus_vectorspace ${Intl_LIBRARIES} ${Boost_LIBRARIES} )

add_executable( strusInspectVectorSpaceModel strusInspectVectorSpaceModel.cpp )
target_link_libraries( strusInspectVectorSpaceModel  strus_error strus_base strus_vectorspace_static ${Intl_LIBRARIES} ${Boost_LIBRARIES} )

# ------------------------------
# INSTALLATION
# ------------------------------
install( TARGETS strus_vectorspace
           LIBRARY DESTINATION ${LIB_INSTALL_DIR}/strus )

install( TARGETS modstrus_storage_vectorspace
           LIBRARY DESTINATION ${LIB_INSTALL_DIR}/strus/modules
	   RUNTIME DESTINATION bin )

install( TARGETS strusInspectVectorSpaceModel
	   RUNTIME DESTINATION bin )

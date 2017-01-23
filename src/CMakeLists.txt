cmake_minimum_required(VERSION 2.6 FATAL_ERROR)

# --------------------------------------
# SOURCES AND INCLUDES
# --------------------------------------
set(
	libstrus_vector_source_files
	utils.cpp
	logger.cpp
	internationalization.cpp
	random.cpp
	simHash.cpp
	simHashMap.cpp
	simGroup.cpp
	simGroupMap.cpp
	dependencyGraph.cpp
	sampleSimGroupMap.cpp
	sampleSimGroupAssignmentQueue.cpp
	simRelationMap.cpp
	simRelationMapBuilder.cpp
	getSimhashValues.cpp
	lshModel.cpp
	lshBench.cpp
	genGroupContext.cpp
	genGroupThreadContext.cpp
	genModel.cpp
	databaseAdapter.cpp
	vectorStorageConfig.cpp
	vectorStorageDump.cpp
	vectorStorageSearch.cpp
	vectorStorage.cpp
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
add_library( strus_vector_static STATIC ${libstrus_vector_source_files} )
target_link_libraries( strus_vector_static strus_base strus_armadillo )

add_library( strus_vector_std SHARED libstrus_vector_std.cpp )
target_link_libraries( strus_vector_std strus_vector_static strus_base strus_armadillo ${Intl_LIBRARIES} ${Boost_LIBRARIES} )
set_target_properties(
    strus_vector_std
    PROPERTIES
    DEBUG_POSTFIX "_d"
    SOVERSION "${STRUS_MAJOR_VERSION}.${STRUS_MINOR_VERSION}"
    VERSION ${STRUS_VERSION}
)

add_library( modstrus_storage_vector_std  MODULE  modstrus_storage_vector_std.cpp )
set_target_properties( modstrus_storage_vector_std PROPERTIES PREFIX "")
target_link_libraries( modstrus_storage_vector_std strus_module_storage strus_vector_std ${Intl_LIBRARIES} ${Boost_LIBRARIES} )

# ------------------------------
# INSTALLATION
# ------------------------------
install( TARGETS strus_vector_std
           LIBRARY DESTINATION ${LIB_INSTALL_DIR}/strus )

install( TARGETS modstrus_storage_vector_std
           LIBRARY DESTINATION ${LIB_INSTALL_DIR}/strus/modules
	   RUNTIME DESTINATION bin )


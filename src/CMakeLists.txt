cmake_minimum_required(VERSION 2.8 FATAL_ERROR)

# --------------------------------------
# SOURCES AND INCLUDES
# --------------------------------------
set( source_files
	logger.cpp
	armautils.cpp
	${CMAKE_CURRENT_BINARY_DIR}/internationalization.cpp
	simHash.cpp
	simHashMap.cpp
	getSimhashValues.cpp
	lshModel.cpp
	lshBench.cpp
	databaseAdapter.cpp
	vectorStorage.cpp
	vectorStorageClient.cpp
	vectorStorageDump.cpp
	vectorStorageSearch.cpp
	vectorStorageTransaction.cpp
)

include_directories(
	${Boost_INCLUDE_DIRS}
	"${Intl_INCLUDE_DIRS}"
	"${ARMADILLO_INCLUDE_DIRS}"
	"${VECTOR_INCLUDE_DIRS}"
	"${strusbase_INCLUDE_DIRS}"
	"${strus_INCLUDE_DIRS}"
	"${strusmodule_INCLUDE_DIRS}"
	"${MAIN_SOURCE_DIR}"
)
link_directories(
	${Boost_LIBRARY_DIRS}
	${ARMADILLO_LIBRARY_DIRS}
	"${strusbase_LIBRARY_DIRS}"
)

# ------------------------------
# MODULE 
# ------------------------------
add_cppcheck( strus_vector ${source_files} libstrus_vector_std.cpp modstrus_storage_vector_std.cpp )

add_library( strus_vector_static STATIC ${source_files} )
target_link_libraries( strus_vector_static strus_base ${ARMADILLO_LIBRARIES} ${Intl_LIBRARIES} )
set_property( TARGET strus_vector_static PROPERTY POSITION_INDEPENDENT_CODE TRUE )

add_library( strus_vector_std SHARED libstrus_vector_std.cpp )
target_link_libraries( strus_vector_std strus_vector_static strus_base ${ARMADILLO_LIBRARIES} ${Intl_LIBRARIES} ${Boost_LIBRARIES} )
set_target_properties(
    strus_vector_std
    PROPERTIES
    DEBUG_POSTFIX "${CMAKE_DEBUG_POSTFIX}"
    SOVERSION "${STRUS_MAJOR_VERSION}.${STRUS_MINOR_VERSION}"
    VERSION ${STRUS_VERSION}
)

add_library( modstrus_storage_vector_std  MODULE  modstrus_storage_vector_std.cpp )
set_target_properties( modstrus_storage_vector_std PROPERTIES PREFIX "")
target_link_libraries( modstrus_storage_vector_std strus_module_storage strus_vector_std ${Intl_LIBRARIES} ${Boost_LIBRARIES} )

# Copy pattern module into a created directory easy relocatable by tests:
add_custom_command( TARGET modstrus_storage_vector_std POST_BUILD
					  COMMAND ${CMAKE_COMMAND} -E make_directory  ${CMAKE_BINARY_DIR}/modules/strus )
add_custom_command( TARGET modstrus_storage_vector_std POST_BUILD
					  COMMAND ${CMAKE_COMMAND} -E copy_if_different  $<TARGET_FILE:modstrus_storage_vector_std>  ${CMAKE_BINARY_DIR}/modules/strus/
					  COMMENT "Copy strus standard vector module built to ${CMAKE_BINARY_DIR}/modules/strus/" )

# ------------------------------
# INSTALLATION
# ------------------------------
install( TARGETS strus_vector_std
           LIBRARY DESTINATION ${LIB_INSTALL_DIR}/strus )

install( TARGETS modstrus_storage_vector_std
           LIBRARY DESTINATION ${LIB_INSTALL_DIR}/strus/modules
	   RUNTIME DESTINATION bin )


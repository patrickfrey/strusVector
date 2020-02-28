cmake_minimum_required(VERSION 2.8 FATAL_ERROR)

# --------------------------------------
# SOURCES AND INCLUDES
# --------------------------------------
set( source_files
	vectorUtils.cpp
)

include_directories(
	${Boost_INCLUDE_DIRS}
	"${Intl_INCLUDE_DIRS}"
	"${ARMADILLO_INCLUDE_DIRS}"
	"${strusbase_INCLUDE_DIRS}"
	"${strus_INCLUDE_DIRS}"
)
link_directories(
	${Boost_LIBRARY_DIRS}
	${ARMADILLO_LIBRARY_DIRS}
	"${strusbase_LIBRARY_DIRS}"
)

# ------------------------------
# MODULE 
# ------------------------------
add_cppcheck( strus_vector_testutils  ${source_files} )

add_library( strus_vector_testutils STATIC ${source_files} )
target_link_libraries( strus_vector_testutils strus_base ${ARMADILLO_LIBRARIES} ${Intl_LIBRARIES} )
set_property( TARGET strus_vector_testutils PROPERTY POSITION_INDEPENDENT_CODE TRUE )


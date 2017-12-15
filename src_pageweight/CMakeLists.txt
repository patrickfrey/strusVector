cmake_minimum_required(VERSION 2.8 FATAL_ERROR)

# --------------------------------------
# INCLUDES
# --------------------------------------
include_directories(
	${Boost_INCLUDE_DIRS}
	"${Intl_INCLUDE_DIRS}"
	"${ARMADILLO_INCLUDE_DIRS}"
	"${VECTOR_INCLUDE_DIRS}"
	"${strusbase_INCLUDE_DIRS}"
)
link_directories(
	${Boost_LIBRARY_DIRS}
	"${VECTOR_LIBRARY_DIRS}"
	"${ARMADILLO_LIBRARY_DIRS}"
	"${strusbase_LIBRARY_DIRS}"
)

# ------------------------------
# PROGRAM
# ------------------------------
add_library( strus_pageweight_static STATIC pageweight.cpp )
target_link_libraries( strus_pageweight_static strus_base "${Boost_LIBRARIES}" "${Intl_LIBRARIES}" )
set_property( TARGET strus_pageweight_static PROPERTY POSITION_INDEPENDENT_CODE TRUE )

add_executable( strusPageWeight  strusPageWeight.cpp )
target_link_libraries( strusPageWeight strus_base strus_error strus_pageweight_static ${Boost_LIBRARIES} ${Intl_LIBRARIES} )


# ------------------------------
# INSTALLATION
# ------------------------------
install( TARGETS strusPageWeight
           RUNTIME DESTINATION bin )


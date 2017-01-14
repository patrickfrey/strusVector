cmake_minimum_required(VERSION 2.6 FATAL_ERROR)

# --------------------------------------
# INCLUDES
# --------------------------------------
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
	"${PROJECT_SOURCE_DIR}/src_pagerank"
	"${PROJECT_BINARY_DIR}/3rdParty/armadillo"
	"${strusbase_LIBRARY_DIRS}"
)

# ------------------------------
# PROGRAM
# ------------------------------
add_library( strus_pagerank_static STATIC pagerank.cpp )
target_link_libraries( strus_pagerank_static strus_base "${Boost_LIBRARIES}" "${Intl_LIBRARIES}" )

add_executable( strusPageRank  strusPageRank.cpp )
target_link_libraries( strusPageRank strus_base strus_error strus_pagerank_static ${Boost_LIBRARIES} ${Intl_LIBRARIES} )


# ------------------------------
# INSTALLATION
# ------------------------------
install( TARGETS strusPageRank
           RUNTIME DESTINATION bin )


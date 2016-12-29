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
	"${PROJECT_BINARY_DIR}/3rdParty/armadillo"
	"${strusbase_LIBRARY_DIRS}"
)

# ------------------------------
# PROGRAM
# ------------------------------
add_executable( strusPageRank  strusPageRank.cpp pagerank.cpp )
target_link_libraries( strusPageRank strus_base strus_error ${Boost_LIBRARIES} ${Intl_LIBRARIES} )


# ------------------------------
# INSTALLATION
# ------------------------------
install( TARGETS strusPageRank
           RUNTIME DESTINATION bin )


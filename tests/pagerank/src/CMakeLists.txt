cmake_minimum_required(VERSION 2.8 FATAL_ERROR)

include_directories(
	"${Boost_INCLUDE_DIRS}"
	"${Intl_INCLUDE_DIRS}"
	"${PROJECT_SOURCE_DIR}/src"
	"${PROJECT_SOURCE_DIR}/src_pageweight"
	"${PROJECT_SOURCE_DIR}/include"
	"${strusbase_INCLUDE_DIRS}"
)
link_directories(
	"${PROJECT_SOURCE_DIR}/src"
	"${PROJECT_SOURCE_DIR}/src_pageweight"
	"${PROJECT_BINARY_DIR}/3rdParty/armadillo"
	"${Boost_LIBRARY_DIRS}"
	"${strusbase_LIBRARY_DIRS}"
)

add_executable( testPageRank testPageRank.cpp )
target_link_libraries( testPageRank strus_pageweight_static strus_armadillo ${Boost_LIBRARIES} ${Intl_LIBRARIES} )




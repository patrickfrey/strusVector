cmake_minimum_required(VERSION 2.8 FATAL_ERROR)

include_directories(
	${Boost_INCLUDE_DIRS}
	"${Intl_INCLUDE_DIRS}"
	"${MAIN_SOURCE_DIR}"
	"${PAGEWEIGHT_INCLUDE_DIRS}/src_pageweight"
	"${VECTOR_INCLUDE_DIRS}"
	${ARMADILLO_INCLUDE_DIRS}
	"${strusbase_INCLUDE_DIRS}"
)
link_directories(
	"${VECTOR_LIBRARY_DIRS}"
	"${PAGEWEIGHT_LIBRARY_DIRS}"
	${ARMADILLO_LIBRARY_DIRS}
	${Boost_LIBRARY_DIRS}
	"${strusbase_LIBRARY_DIRS}"
)

add_executable( testPageRank testPageRank.cpp )
target_link_libraries( testPageRank strus_pageweight_static  ${ARMADILLO_LIBRARIES} ${Boost_LIBRARIES} ${Intl_LIBRARIES} )




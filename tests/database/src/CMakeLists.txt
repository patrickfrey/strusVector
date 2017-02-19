cmake_minimum_required(VERSION 2.8 FATAL_ERROR)

include_directories(
	"${Boost_INCLUDE_DIRS}"
	"${Intl_INCLUDE_DIRS}"
	"${PROJECT_BINARY_DIR}/3rdParty/armadillo/include/"
	"${PROJECT_SOURCE_DIR}/3rdParty/armadillo/include/"
	"${PROJECT_SOURCE_DIR}/src"
	"${PROJECT_SOURCE_DIR}/include"
	"${strusbase_INCLUDE_DIRS}"
	"${strus_INCLUDE_DIRS}"
)
link_directories(
	"${PROJECT_SOURCE_DIR}/src"
	"${PROJECT_BINARY_DIR}/3rdParty/armadillo"
	"${Boost_LIBRARY_DIRS}"
	"${strusbase_LIBRARY_DIRS}"
	"${strus_LIBRARY_DIRS}"
	"${strus_LIBRARY_DIRS}"
)

add_executable( testDatabaseAdapter testDatabaseAdapter.cpp)
target_link_libraries( testDatabaseAdapter strus_vector_static  strus_database_leveldb strus_error strus_armadillo ${Boost_LIBRARIES} ${Intl_LIBRARIES} )




cmake_minimum_required(VERSION 2.6 FATAL_ERROR)

include_directories(
	"${Intl_INCLUDE_DIRS}"
	"${PROJECT_SOURCE_DIR}/3rdParty/armadillo/include"
	"${PROJECT_BINARY_DIR}/3rdParty/armadillo/include"
	"${PROJECT_BINARY_DIR}/include"
)
link_directories(
	"${PROJECT_SOURCE_DIR}/3rdParty/armadillo"
)

add_executable( testArmadillo testArmadillo.cpp)
target_link_libraries( testArmadillo ${Intl_LIBRARIES} strus_armadillo )



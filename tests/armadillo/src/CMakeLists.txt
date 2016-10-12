cmake_minimum_required(VERSION 2.6 FATAL_ERROR)

include_directories(
	"${Intl_INCLUDE_DIRS}"
	"${PROJECT_BINARY_DIR}/3rdParty/armadillo/include"
)
link_directories(
	"${PROJECT_BINARY_DIR}/3rdParty/armadillo"
)

add_executable( testArmadillo testArmadillo.cpp)
target_link_libraries( testArmadillo ${Intl_LIBRARIES} strus_armadillo )



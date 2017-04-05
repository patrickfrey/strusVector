cmake_minimum_required(VERSION 2.8 FATAL_ERROR)

include_directories(
	"${Intl_INCLUDE_DIRS}"
	"${PROJECT_BINARY_DIR}/3rdParty/armadillo/include"
	"${PROJECT_SOURCE_DIR}/3rdParty/armadillo/include"
	"${PROJECT_BINARY_DIR}/include"
)
link_directories(
	"${PROJECT_BINARY_DIR}/3rdParty/armadillo"
)

# some hacks to make FreeBSD work
if(CMAKE_SYSTEM_NAME MATCHES "FreeBSD")
    set(FREEBSD true)
endif(CMAKE_SYSTEM_NAME MATCHES "FreeBSD")
if(FREEBSD)
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-rpath=/usr/local/lib/gcc5 -L/usr/local/lib/gcc5 -lgcc_s -lquadmath")
endif()

add_executable( testArmadillo testArmadillo.cpp)
target_link_libraries( testArmadillo ${Intl_LIBRARIES} strus_armadillo )



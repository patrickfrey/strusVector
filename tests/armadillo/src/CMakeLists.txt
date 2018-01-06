cmake_minimum_required(VERSION 2.8 FATAL_ERROR)

include_directories(
	"${Intl_INCLUDE_DIRS}"
	"${ARMADILLO_INCLUDE_DIRS}"
	"${VECTOR_INCLUDE_DIRS}"
)
link_directories(
	"${ARMADILLO_LIBRARY_DIRS}"
)

# some hacks to make FreeBSD work
if(CMAKE_SYSTEM_NAME MATCHES "FreeBSD")
    set(FREEBSD true)
endif(CMAKE_SYSTEM_NAME MATCHES "FreeBSD")
if(FREEBSD)
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-rpath=/usr/local/lib/gcc6 -L/usr/local/lib/gcc6 -lgcc_s -lquadmath")
endif()

add_executable( testArmadillo testArmadillo.cpp)
target_link_libraries( testArmadillo ${Intl_LIBRARIES} strus_armadillo )



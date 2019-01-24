cmake_minimum_required(VERSION 2.8 FATAL_ERROR)

include_directories(
	${Boost_INCLUDE_DIRS}
	"${Intl_INCLUDE_DIRS}"
	"${ARMADILLO_INCLUDE_DIRS}"
	"${VECTOR_INCLUDE_DIRS}"
	"${MAIN_SOURCE_DIR}"
	"${strusbase_INCLUDE_DIRS}"
	"${strus_INCLUDE_DIRS}"
)
link_directories(
	"${VECTOR_LIBRARY_DIRS}"
	${ARMADILLO_LIBRARY_DIRS}
	${Boost_LIBRARY_DIRS}
	"${strusbase_LIBRARY_DIRS}"
	"${strus_LIBRARY_DIRS}"
)

# some hacks to make FreeBSD work
if(CMAKE_SYSTEM_NAME MATCHES "FreeBSD")
    set(FREEBSD true)
endif(CMAKE_SYSTEM_NAME MATCHES "FreeBSD")
if(FREEBSD)
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-rpath=/usr/local/lib/gcc6 -L/usr/local/lib/gcc6 -lgcc_s -lquadmath")
endif()

add_executable( testDatabaseAdapter testDatabaseAdapter.cpp)
target_link_libraries( testDatabaseAdapter strus_vector_static  strus_database_leveldb strus_error  ${ARMADILLO_LIBRARIES} ${Boost_LIBRARIES} ${Intl_LIBRARIES} )




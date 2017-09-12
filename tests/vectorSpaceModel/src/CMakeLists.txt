cmake_minimum_required(VERSION 2.8 FATAL_ERROR)

include_directories(
	"${Boost_INCLUDE_DIRS}"
	"${Intl_INCLUDE_DIRS}"
	"${PROJECT_BINARY_DIR}/3rdParty/armadillo/include"
	"${PROJECT_SOURCE_DIR}/3rdParty/armadillo/include"
	"${PROJECT_SOURCE_DIR}/src"
	"${PROJECT_SOURCE_DIR}/include"
	"${strusbase_INCLUDE_DIRS}"
	"${strus_INCLUDE_DIRS}"
)
link_directories(
	"${CMAKE_CURRENT_BINARY_DIR}/../../../src"
	"${PROJECT_BINARY_DIR}/3rdParty/armadillo"
	"${Boost_LIBRARY_DIRS}"
	"${strusbase_LIBRARY_DIRS}"
)

# some hacks to make FreeBSD work
if(CMAKE_SYSTEM_NAME MATCHES "FreeBSD")
    set(FREEBSD true)
endif(CMAKE_SYSTEM_NAME MATCHES "FreeBSD")
if(FREEBSD)
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-rpath=/usr/local/lib/gcc5 -L/usr/local/lib/gcc5 -lgcc_s -lquadmath")
endif()

add_executable( testLshSimHash testLshSimHash.cpp)
target_link_libraries( testLshSimHash strus_vector_static  ${Boost_LIBRARIES} ${Intl_LIBRARIES} strus_armadillo )

add_executable( testLshVectorSpaceModel testLshVectorSpaceModel.cpp)
target_link_libraries( testLshVectorSpaceModel ${Boost_LIBRARIES} strus_base ${Boost_LIBRARIES} ${Intl_LIBRARIES} strus_armadillo )

add_executable( testVectorSpaceModel testVectorSpaceModel.cpp)
target_link_libraries( testVectorSpaceModel ${Boost_LIBRARIES} strus_base strus_error strus_vector_std strus_database_leveldb ${Boost_LIBRARIES} ${Intl_LIBRARIES} strus_armadillo )



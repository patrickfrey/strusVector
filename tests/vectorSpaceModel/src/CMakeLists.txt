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
	"${strusbase_LIBRARY_DIRS}"
	${Boost_LIBRARY_DIRS}
)

# some hacks to make FreeBSD work
if(CMAKE_SYSTEM_NAME MATCHES "FreeBSD")
    set(FREEBSD true)
endif(CMAKE_SYSTEM_NAME MATCHES "FreeBSD")
if(FREEBSD)
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-rpath=/usr/local/lib/gcc6 -L/usr/local/lib/gcc6 -lgcc_s -lquadmath")
endif()

add_executable( testLshSimHash testLshSimHash.cpp)
target_link_libraries( testLshSimHash strus_vector_static  ${ARMADILLO_LIBRARIES} ${Boost_LIBRARIES} ${Intl_LIBRARIES} )

add_executable( testLshVectorSpaceModel testLshVectorSpaceModel.cpp)
target_link_libraries( testLshVectorSpaceModel ${Boost_LIBRARIES} strus_base ${Boost_LIBRARIES} ${Intl_LIBRARIES}  ${ARMADILLO_LIBRARIES} )

add_executable( testVectorStorageInterface testVectorStorageInterface.cpp)
target_link_libraries( testVectorStorageInterface ${Boost_LIBRARIES} strus_base strus_error strus_vector_std strus_database_leveldb ${Boost_LIBRARIES} ${Intl_LIBRARIES}  ${ARMADILLO_LIBRARIES} )



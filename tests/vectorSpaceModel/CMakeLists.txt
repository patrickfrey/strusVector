cmake_minimum_required(VERSION 2.8 FATAL_ERROR)

add_subdirectory(src)

add_test( VectorSimHash ${CMAKE_CURRENT_BINARY_DIR}/src/testLshSimHash )
add_test( LshVectorSpaceModel ${CMAKE_CURRENT_BINARY_DIR}/src/testLshVectorSpaceModel )

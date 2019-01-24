cmake_minimum_required(VERSION 2.8 FATAL_ERROR)

add_subdirectory(src)

add_test( VectorStorageInterface ${CMAKE_CURRENT_BINARY_DIR}/src/testVectorStorageInterface ${CMAKE_CURRENT_BINARY_DIR} 600 3)
add_test( VectorSimHash ${CMAKE_CURRENT_BINARY_DIR}/src/testLshSimHash )
add_test( LshVectorSpaceModel ${CMAKE_CURRENT_BINARY_DIR}/src/testLshVectorSpaceModel )

cmake_minimum_required(VERSION 2.8 FATAL_ERROR)

add_subdirectory(src)

add_test( DatabaseAdapter ${CMAKE_CURRENT_BINARY_DIR}/src/testDatabaseAdapter ${CMAKE_CURRENT_BINARY_DIR}/  6000 3 )

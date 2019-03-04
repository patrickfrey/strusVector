cmake_minimum_required(VERSION 2.8 FATAL_ERROR)

add_subdirectory(src)

add_test( SentenceLexer ${CMAKE_CURRENT_BINARY_DIR}/src/testSentenceLexer ${CMAKE_CURRENT_BINARY_DIR} 3 3 15 10 10 )


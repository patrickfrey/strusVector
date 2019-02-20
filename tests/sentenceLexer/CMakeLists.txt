cmake_minimum_required(VERSION 2.8 FATAL_ERROR)

add_subdirectory(src)

add_test( SentenceLexer ${CMAKE_CURRENT_BINARY_DIR}/src/testSentenceLexer ${CMAKE_CURRENT_BINARY_DIR} 3 3 100 10 10 )
add_test( SentenceLexer ${CMAKE_CURRENT_BINARY_DIR}/src/testSentenceLexer ${CMAKE_CURRENT_BINARY_DIR} 5 3000 1000 10 10 )


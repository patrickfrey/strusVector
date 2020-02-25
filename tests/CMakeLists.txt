cmake_minimum_required(VERSION 2.8 FATAL_ERROR)

add_subdirectory( armadillo )
add_subdirectory( database )
add_subdirectory( vectorSpaceModel )
IF (WITH_PAGERANK STREQUAL "YES")
add_subdirectory( pagerank )
ENDIF (WITH_PAGERANK STREQUAL "YES")
# add_subdirectory( sentenceLexer )

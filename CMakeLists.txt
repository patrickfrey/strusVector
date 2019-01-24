cmake_minimum_required(VERSION 2.8 FATAL_ERROR)

# must be done BEFORE project in order to work!
include(cmake/install_prefix.cmake)

# Project globals:
project( StrusVector )
set( STRUS_MAJOR_VERSION 0 )
set( STRUS_MINOR_VERSION 17 )
set( STRUS_PATCH_VERSION 0 )
set( STRUS_VERSION ${STRUS_MAJOR_VERSION}.${STRUS_MINOR_VERSION}.${STRUS_PATCH_VERSION} )

if(NOT STRUS_ALL)
set( CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake" )
include( cmake/cxx11.cmake )
check_for_cxx11_compiler( HAVE_CXX11 )
include( cmake/dist.cmake )
include( cmake/LinuxDistribution.cmake )
include( cmake/build_rules.cmake )
include( cmake/boost.cmake )
include( cmake/find_strus_package.cmake )
include( cmake/find_leveldb.cmake )
# include( cmake/find_armadillo.cmake )
include( cmake/link_rules.cmake )
include( cmake/intl.cmake )
include( cmake/libdl.cmake )
include( cmake/cppcheck.cmake )
endif(NOT STRUS_ALL)

find_strus_package( base )
find_strus_package( core )
find_strus_package( module )
set( CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake" )
configure_file( "${PROJECT_SOURCE_DIR}/include/strus/versionVector.hpp.in"  "${CMAKE_CURRENT_BINARY_DIR}/include/strus/versionVector.hpp"  @ONLY )
configure_file( "${PROJECT_SOURCE_DIR}/dist/obs/deploy_to_obs.sh.in"  "${PROJECT_SOURCE_DIR}/dist/obs/deploy_to_obs.sh"  @ONLY )
configure_file( "${PROJECT_SOURCE_DIR}/dist/archlinux/PKGBUILD.in"  "${PROJECT_SOURCE_DIR}/dist/archlinux/PKGBUILD"  @ONLY )
configure_file( "${PROJECT_SOURCE_DIR}/dist/obs/strusvector.dsc.in"  "${PROJECT_SOURCE_DIR}/dist/obs/strusvector.dsc"  @ONLY )
configure_file( "${PROJECT_SOURCE_DIR}/dist/obs/strusvector-xUbuntu_14.04.dsc.in"  "${PROJECT_SOURCE_DIR}/dist/obs/strusvector-xUbuntu_14.04.dsc"  @ONLY )
configure_file( "${PROJECT_SOURCE_DIR}/dist/obs/strusvector-xUbuntu_16.04.dsc.in" "${PROJECT_SOURCE_DIR}/dist/obs/strusvector-xUbuntu_16.04.dsc"  @ONLY )
configure_file( "${PROJECT_SOURCE_DIR}/dist/redhat/strusvector.spec.in"  "${PROJECT_SOURCE_DIR}/dist/redhat/strusvector.spec"  @ONLY )

set( STRUS_GETTEXT_PACKAGE "strus-dom" )	#... 1st parameter of bindtextdomain(...)
set( STRUS_GETTEXT_LOCALEDIR "" )			#... 2nd parameter of bindtextdomain(...)
configure_file( "${PROJECT_SOURCE_DIR}/src/internationalization.cpp.in"  "${CMAKE_CURRENT_BINARY_DIR}/src/internationalization.cpp"  @ONLY )
IF (WITH_PAGERANK STREQUAL "YES")
set( STRUS_WITH_PAGERANK 1 )
ELSE (WITH_PAGERANK STREQUAL "YES")
set( STRUS_WITH_PAGERANK 0 )
ENDIF (WITH_PAGERANK STREQUAL "YES")
configure_file( "${PROJECT_SOURCE_DIR}/src_pageweight/pageweightdefs.hpp.in"  "${PROJECT_SOURCE_DIR}/src_pageweight/pageweightdefs.hpp"  @ONLY )

enable_testing()

# Path declarations:
set( ARMADILLO_INCLUDE_DIRS        "${CMAKE_CURRENT_BINARY_DIR}/3rdParty/armadillo/include" "${PROJECT_SOURCE_DIR}/3rdParty/armadillo/include" )
set( ARMADILLO_LIBRARY_DIRS        "${CMAKE_CURRENT_BINARY_DIR}/3rdParty/armadillo" )
set( ARMADILLO_LIBRARIES               strus_armadillo  ${BLAS_LIBRARIES}  ${OpenBLAS_LIBRARIES}  ${LAPACK_LIBRARIES}  ${ATLAS_LIBRARIES} )
set( VECTOR_INCLUDE_DIRS             "${CMAKE_CURRENT_BINARY_DIR}/include"  "${PROJECT_SOURCE_DIR}/include" )
set( VECTOR_LIBRARY_DIRS             "${CMAKE_CURRENT_BINARY_DIR}/src" )
set( PAGEWEIGHT_INCLUDE_DIRS    "${CMAKE_CURRENT_BINARY_DIR}/src_pageweight" )
set( PAGEWEIGHT_LIBRARY_DIRS    "${CMAKE_CURRENT_BINARY_DIR}/src_pageweight" )
set( MAIN_SOURCE_DIR                     "${PROJECT_SOURCE_DIR}/src" )
set( MAIN_TESTS_DIR                        "${PROJECT_SOURCE_DIR}/tests" )

# Subdirectories:
add_subdirectory( 3rdParty )
add_subdirectory( src )
add_subdirectory( tests )
add_subdirectory( src_pageweight )
 
if(NOT STRUS_ALL)
include( cmake/report_build_settings.cmake )
endif()

# Install header files:
install(DIRECTORY include/strus
          DESTINATION include
          FILES_MATCHING PATTERN "*.hpp")
install(DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/include/strus"
          DESTINATION include
          FILES_MATCHING PATTERN "versionVector.hpp")
install(DIRECTORY include/strus
          DESTINATION include
          FILES_MATCHING PATTERN "*.h")

if(NOT STRUS_ALL)
include( cmake/standard_targets.cmake )
endif()

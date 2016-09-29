cmake_minimum_required(VERSION 2.6 FATAL_ERROR)

# must be done BEFORE project in order to work!
include(cmake/install_prefix.cmake)

# Project globals:
project( StrusVector )
set( STRUS_MAJOR_VERSION 0 )
set( STRUS_MINOR_VERSION 11 )
set( STRUS_PATCH_VERSION 0 )
set( STRUS_VERSION ${STRUS_MAJOR_VERSION}.${STRUS_MINOR_VERSION}.${STRUS_PATCH_VERSION} )

include( cmake/dist.cmake )
include( cmake/LinuxDistribution.cmake )
include( cmake/build_rules.cmake )
include( cmake/boost.cmake )
include( cmake/intl.cmake )
include( cmake/find_strus_package.cmake )

find_strus_package( base )
set( CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake" )
configure_file( "${PROJECT_SOURCE_DIR}/include/strus/versionVector.hpp.in"  "${PROJECT_SOURCE_DIR}/include/strus/versionVector.hpp"  @ONLY )
configure_file( "${PROJECT_SOURCE_DIR}/dist/obs/deploy_to_obs.sh.in"  "${PROJECT_SOURCE_DIR}/dist/obs/deploy_to_obs.sh"  @ONLY )
configure_file( "${PROJECT_SOURCE_DIR}/dist/archlinux/PKGBUILD.in"  "${PROJECT_SOURCE_DIR}/dist/archlinux/PKGBUILD"  @ONLY )
configure_file( "${PROJECT_SOURCE_DIR}/dist/obs/strusvector.dsc.in"  "${PROJECT_SOURCE_DIR}/dist/obs/strusvector.dsc"  @ONLY )
configure_file( "${PROJECT_SOURCE_DIR}/dist/obs/strusvector-Debian_7.0.dsc.in"  "${PROJECT_SOURCE_DIR}/dist/obs/strusvector-Debian_7.0.dsc"  @ONLY )
configure_file( "${PROJECT_SOURCE_DIR}/dist/obs/strusvector-xUbuntu_12.04.dsc.in"  "${PROJECT_SOURCE_DIR}/dist/obs/strusvector-xUbuntu_12.04.dsc"  @ONLY )
configure_file( "${PROJECT_SOURCE_DIR}/dist/redhat/strusvector.spec.in"  "${PROJECT_SOURCE_DIR}/dist/redhat/strusvector.spec"  @ONLY )

set( STRUS_GETTEXT_PACKAGE "strus-dom" )	#... 1st parameter of bindtextdomain(...)
set( STRUS_GETTEXT_LOCALEDIR "" )			#... 2nd parameter of bindtextdomain(...)
configure_file( "${PROJECT_SOURCE_DIR}/src/internationalization.cpp.in"  "${PROJECT_SOURCE_DIR}/src/internationalization.cpp"  @ONLY )

enable_testing()

# Subdirectories:
add_subdirectory( 3rdParty )
add_subdirectory( src )
add_subdirectory( tests )
  
include( cmake/report_build_settings.cmake )

# Install header files:
install(DIRECTORY include/strus
          DESTINATION include
          FILES_MATCHING PATTERN "*.hpp")
install(DIRECTORY include/strus
          DESTINATION include
          FILES_MATCHING PATTERN "*.h")

include( cmake/standard_targets.cmake )

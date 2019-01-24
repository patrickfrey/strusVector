# - Find the "armadillo" library
#

if( ARMADILLO_ROOT )
MESSAGE( STATUS "Installation path of armadillo: '${ARMADILLO_ROOT}' " )

find_path( ai NAMES armadillo_bits
			HINTS "${ARMADILLO_ROOT}/include"  "${ARMADILLO_ROOT}"
			NO_CMAKE_ENVIRONMENT_PATH
			NO_CMAKE_PATH
			NO_SYSTEM_ENVIRONMENT_PATH
			NO_CMAKE_SYSTEM_PATH )
MESSAGE( STATUS "Find include path armadillo no system path returns: '${pt}' " )
if( ai AND NOT ai STREQUAL "ai-NOTFOUND" )
set( ARMADILLO_INCLUDE_DIRS ${ai} )
endif( ai AND NOT ai STREQUAL "ai-NOTFOUND" )

find_library( al NAMES armadillo
			HINTS "${ARMADILLO_ROOT}/${LIB_INSTALL_DIR}" "${ARMADILLO_ROOT}/lib" "${ARMADILLO_ROOT}"
			NO_DEFAULT_PATH
			NO_CMAKE_ENVIRONMENT_PATH
			NO_CMAKE_PATH
          		NO_SYSTEM_ENVIRONMENT_PATH
			NO_CMAKE_SYSTEM_PATH )
if( al AND NOT al STREQUAL "al-NOTFOUND" )
set( ARMADILLO_LIBRARIES ${al} )
endif( al AND NOT al STREQUAL "al-NOTFOUND" )

else( ARMADILLO_ROOT )

find_path ( ARMADILLO_INCLUDE_DIRS NAMES armadillo )
find_library ( ARMADILLO_LIBRARIES NAMES armadillo )

endif( ARMADILLO_ROOT )

# Handle the QUIETLY and REQUIRED arguments and set ARMADILLO_FOUND to TRUE if all listed variables are TRUE.
include ( FindPackageHandleStandardArgs )
find_package_handle_standard_args ( ARMADILLO DEFAULT_MSG ARMADILLO_LIBRARIES ARMADILLO_INCLUDE_DIRS )

if ( ARMADILLO_FOUND )
  MESSAGE( STATUS "Armadillo includes: ${ARMADILLO_INCLUDE_DIRS}" )
  MESSAGE( STATUS "Armadillo libraries: ${ARMADILLO_LIBRARIES}" )
else ( ARMADILLO_FOUND )
  message( STATUS "Armadillo library not found" )
endif ( ARMADILLO_FOUND )




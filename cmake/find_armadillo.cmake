# - Find the "armadillo" library
#

find_path ( ARMADILLO_INCLUDE_DIRS NAMES armadillo )
find_library ( ARMADILLO_LIBRARIES NAMES armadillo )

# Handle the QUIETLY and REQUIRED arguments and set TRE_FOUND to TRUE if all listed variables are TRUE.
include ( FindPackageHandleStandardArgs )
find_package_handle_standard_args ( ARMADILLO DEFAULT_MSG ARMADILLO_LIBRARIES ARMADILLO_INCLUDE_DIRS )

if ( ARMADILLO_FOUND )
  MESSAGE( STATUS "Armadillo includes: ${ARMADILLO_INCLUDE_DIRS}" )
  MESSAGE( STATUS "Armadillo libraries: ${ARMADILLO_LIBRARIES}" )
else ( ARMADILLO_FOUND )
  message( STATUS "Armadillo library not found" )
endif ( ARMADILLO_FOUND )




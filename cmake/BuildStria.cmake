##############################################################################
#
# CTest Automated Build Variables for Stria
#
##############################################################################

if ( NOT DEFINED ${CTEST_BUILD_NAME} )
  set( CTEST_BUILD_NAME "dakota_stria" )
endif()

set( DAKOTA_CMAKE_PLATFORM "Stria.cmake")
set( DAKOTA_CMAKE_BUILD_TYPE "DakotaDistro.cmake")

set( DAKOTA_CTEST_PROJECT_TAG "Continuous" )
set( CTEST_BUILD_CONFIGURATION Release )

set( DAKOTA_CTEST_REGEXP "dakota_*" )

##############################################################################

##########################################################################
# Set up Internal CMake paths first. Then call automated build file.
# DO NOT CHANGE!!
##########################################################################
include( ${CTEST_SCRIPT_DIRECTORY}/utilities/DakotaSetupBuildEnv.cmake )
include( common_build )
##########################################################################


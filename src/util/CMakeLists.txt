#  _______________________________________________________________________
#
#  DAKOTA: Design Analysis Kit for Optimization and Terascale Applications
#  Copyright 2014-2020
#  National Technology & Engineering Solutions of Sandia, LLC (NTESS).
#  This software is distributed under the GNU Lesser General Public License.
#  For more information, see the README file in the top Dakota directory.
#  _______________________________________________________________________


# BMA TODO: Do we want to have each module be its own project?
#project(DakotaUtil LANGUAGES CXX)

set(util_sources
  util_common.cpp
  UtilDataScaler.cpp
  UtilLinearSolvers.cpp
  util_metrics.cpp
  util_math_tools.cpp
  )

set(util_headers
  util_common.hpp
  UtilDataScaler.hpp
  UtilLinearSolvers.hpp
  util_metrics.hpp
  util_data_types.hpp
  util_eigen_plugins.hpp
  util_math_tools.hpp
  )

add_library(dakota_util ${util_sources} ${util_headers})

# Rationale: append includes from our source outward to system
target_include_directories(dakota_util PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
  $<INSTALL_INTERFACE:include>
  )

# Rationale: Teuchos is included in API headers
target_include_directories(dakota_util PUBLIC
  "$<BUILD_INTERFACE:${Teuchos_INCLUDE_DIRS}>"
  $<INSTALL_INTERFACE:include>
  )
# Rationale: Teuchos libraries aren't used directly, but something is
# indirectly using CommandLine, RCP, and TimeMonitor, so this link is
# more liberal than should be needed
target_link_libraries(dakota_util PUBLIC ${Teuchos_LIBRARIES})

# BMA TODO: CMake config at top-level per RWH example
#set(EIGEN3_INCLUDE_DIRS
#  "${Dakota_SOURCE_DIR}/packages/external/eigen3/include/eigen3")
SET( CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/../../packages/external/eigen3/share/eigen3/cmake)
find_package(Eigen3 3.3 REQUIRED CONFIG)
if(TARGET Eigen3::Eigen)
  message(STATUS "Eigen3 v${EIGEN3_VERSION_STRING} found in
  ${EIGEN3_INCLUDE_DIR}")
endif()

# Rationale: restrict Eigen to MPL2-licensed components
# Rationale: Eigen is referenced in API headers
target_compile_definitions(dakota_util PUBLIC "EIGEN_MPL2_ONLY")
target_link_libraries(dakota_util PUBLIC Eigen3::Eigen)

# Rationale: Boost headers are implementation details only, while
# serialization is referenced in API headers
target_link_libraries(dakota_util PRIVATE Boost::boost
  PUBLIC Boost::serialization)

dakota_strict_warnings(dakota_util)
dakota_gcov_target(dakota_util)

install(FILES ${util_headers} DESTINATION "include")
install(TARGETS dakota_util EXPORT ${ExportTarget} DESTINATION lib)

add_subdirectory(unit)

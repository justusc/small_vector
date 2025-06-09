cmake_minimum_required(VERSION 3.15)

if(PROJECT_NAME)
  set(SMALL_VECTOR_IS_SUBPROJECT TRUE)
endif ()

set(SMALL_VECTOR_VERSION_MAJOR 1)
set(SMALL_VECTOR_VERSION_MINOR 0)
set(SMALL_VECTOR_VERSION_PATCH 0)
set(SMALL_VECTOR_VERSION "${SMALL_VECTOR_VERSION_MAJOR}.${SMALL_VECTOR_VERSION_MINOR}.${SMALL_VECTOR_VERSION_PATCH}")

project(SmallVector VERSION ${SMALL_VECTOR_VERSION} LANGUAGES CXX)

# Add external dependencies
add_subdirectory(external)

# Create inferface library
add_library(small_vector INTERFACE)
target_include_directories(small_vector INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(small_vector INTERFACE deferral)

# Testing setup
include(FetchContent)
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG        v1.14.0
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

if(SMALL_VECTOR_IS_SUBPROJECT)
  return()
endif()

add_subdirectory(tests EXCLUDE_FROM_ALL)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

set(SMALL_VECTOR_CMAKE_CONFIG_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}")

write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake"
  VERSION "${PROJECT_VERSION}"
  COMPATIBILITY "SameMajorVersion"
)

configure_package_config_file(
  "${CMAKE_CURRENT_LIST_DIR}/cmake/${PROJECT_NAME}Config.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
  INSTALL_DESTINATION "${SMALL_VECTOR_CMAKE_CONFIG_DESTINATION}"
)

# Targets
install(
  TARGETS "small_vector"
  EXPORT "${PROJECT_NAME}Targets"
  DESTINATION "${CMAKE_INSTALL_LIBDIR}"
)
install(
  EXPORT "${PROJECT_NAME}Targets"
  NAMESPACE "${PROJECT_NAME}::"
  DESTINATION "${SMALL_VECTOR_CMAKE_CONFIG_DESTINATION}"
)
install(
  FILES "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
  DESTINATION "${SMALL_VECTOR_CMAKE_CONFIG_DESTINATION}"
)

# Install the header file
install(FILES include/jacl/small_vector.hh DESTINATION include)

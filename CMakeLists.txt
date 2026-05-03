cmake_minimum_required(VERSION 3.24)

include(cmake/GitHelper.cmake)

project(nestdaq VERSION "${PROJECT_VERSION}" LANGUAGES CXX C)

message(STATUS "PROJECT_NAME: ${PROJECT_NAME}")

include(cmake/common.cmake)
include(CMakePackageConfigHelpers)

option(NESTDAQ_ENABLE_CLANG_TIDY "Run clang-tidy during NestDAQ builds" OFF)
if(NESTDAQ_ENABLE_CLANG_TIDY)
  find_program(CLANG_TIDY_EXE NAMES clang-tidy REQUIRED)
  set(CMAKE_CXX_CLANG_TIDY "${CLANG_TIDY_EXE}")
  message(STATUS "clang-tidy enabled: ${CLANG_TIDY_EXE}")
endif()

set(NESTDAQ_CMAKE_INSTALL_DIR "${CMAKE_INSTALL_LIBDIR}/cmake/NestDAQ")

write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/cmake/NestDAQConfigVersion.cmake"
  VERSION "${PROJECT_VERSION}"
  COMPATIBILITY SameMajorVersion
)

configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake/NestDAQVersion.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/cmake/NestDAQVersion.cmake"
  @ONLY
)

install(FILES
  "${CMAKE_CURRENT_BINARY_DIR}/cmake/NestDAQConfig.cmake"
  "${CMAKE_CURRENT_BINARY_DIR}/cmake/NestDAQConfigVersion.cmake"
  "${CMAKE_CURRENT_BINARY_DIR}/cmake/NestDAQVersion.cmake"
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake/NestDAQTargets.cmake"
  DESTINATION "${NESTDAQ_CMAKE_INSTALL_DIR}"
)

#------------------------------------------------------------------------------
# RPATH handling
set(CMAKE_SKIP_BUILD_RPATH FALSE)
set(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE)
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
if(APPLE)
    set(CMAKE_INSTALL_RPATH "@loader_path/../${CMAKE_INSTALL_LIBDIR}")
elseif(UNIX)
    set(CMAKE_INSTALL_RPATH "$ORIGIN/../${CMAKE_INSTALL_LIBDIR}")
endif()

#------------------------------------------------------------------------------
find_package(FairMQ)
if(FairMQ_FOUND)
  message(STATUS "FairMQ found")
  message(STATUS "FairMQ_VERSION:              ${FairMQ_VERSION}")
  message(STATUS "FairMQ_GIT_VERSION:          ${FairMQ_GIT_VERSION}")
  message(STATUS "FairMQ_PACKAGE_DEPENDENCIES: ${FairMQ_PACKAGE_DEPENDENCIES}")
  message(STATUS "FairMQ_PACKAGE_COMPONENTS:   ${FairMQ_PACKAGE_COMPONENTS}")
  message(STATUS "FairMQ_PREFIX:               ${FairMQ_PREFIX}")
  message(STATUS "FairMQ_BINDIR:               ${FairMQ_BINDIR}")
  message(STATUS "FairMQ_INCDIR:               ${FairMQ_INCDIR}")
  message(STATUS "FairMQ_LIBDIR:               ${FairMQ_LIBDIR}")
  message(STATUS "FairMQ_DATADIR:              ${FairMQ_DATADIR}")
  message(STATUS "FairMQ_CMAKEMODDIR:          ${FairMQ_CMAKEMODDIR}")
  message(STATUS "FairMQ_BUILD_TYPE:           ${FairMQ_BUILD_TYPE}")
  message(STATUS "FairMQ_CXX_FLAGS:            ${FairMQ_CXX_FLAGS}")

  foreach(dep IN LISTS FairMQ_PACKAGE_DEPENDENCIES)
    if(FairMQ_${dep}_COMPONENTS)
      message(" dep: ${dep}")
      message("  FairMQ_${dep}_version:    ${FairMQ_${dep}_VERSION}")
      message("  FairMQ_${dep}_COMPONENTS: ${FairMQ_${dep}_COMPONENTS}")
      if(${dep} MATCHES "Boost")
        find_package(${dep} ${FairMQ_${dep}_VERSION} COMPONENTS ${FairMQ_${dep}_COMPONENTS}; iostreams; thread;)
      else()
        find_package(${dep} ${FairMQ_${dep}_VERSION} COMPONENTS ${FairMQ_${dep}_COMPONENTS};)
      endif()
    else()
      message(" dep: ${dep}")
      message("  FairMQ_${dep}_VERSION: ${FairMQ_${dep}_VERSION}")
      find_package(${dep} ${FairMQ_${dep}_VERSION})
    endif()
  endforeach()
else()
  message(WARNING "FairMQ not found")
endif()


message(STATUS "Boost_INCLUDE_DIRS: ${Boost_INCLUDE_DIRS}")
message(STATUS "Boost_LIBRARY_DIRS: ${Boost_LIBRARY_DIRS}")
message(STATUS "Boost_LIBRARIES:    ${Boost_LIBRARIES}")

if(NOT FairLogger_INCDIR)
  set(FairLogger_INCDIR "${FairMQ_INCDIR}/fairlogger/bundled")
endif()
message(STATUS "FairLogger_INCDIR: ${FairLogger_INCDIR}")
message(STATUS "FairLogger_LIBDIR: ${FairLogger_LIBDIR}")

configure_package_config_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake/NestDAQConfig.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/cmake/NestDAQConfig.cmake"
  INSTALL_DESTINATION "${NESTDAQ_CMAKE_INSTALL_DIR}"
)

find_package(opentelemetry-cpp CONFIG QUIET
  COMPONENTS
    sdk
    exporters_ostream
    exporters_otlp_http
    exporters_otlp_grpc
)
if(opentelemetry-cpp_FOUND)
  message(STATUS "opentelemetry-cpp found: enabling nestdaq telemetry library")
else()
  message(STATUS "opentelemetry-cpp not found: skipping nestdaq telemetry library")
endif()


#------------------------------------------------------------------------------
add_subdirectory(nestdaq)
add_subdirectory(plugins)
add_subdirectory(controller)
add_subdirectory(share)
add_subdirectory(scripts)

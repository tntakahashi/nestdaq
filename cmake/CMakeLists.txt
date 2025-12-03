cmake_minimum_required(VERSION 3.12)

project(nestdaq_external_dependencies LANGUAGES CXX C)

include(common.cmake)

#------------------------------------------------------------------------------
if(${PROJECT_NAME} STREQUAL "nestdaq")
  set(MY_CMAKE_FILES "cmake/")
endif()

set(BOOST_BUILD_PARALLEL_LEVEL "")
if(BUILD_PARALLEL_LEVEL)
  set(BOOST_BUILD_PARALLEL_LEVEL "-j${BUILD_PARALLEL_LEVEL}")
  # replace the value
  set(BUILD_PARALLEL_LEVEL "--parallel ${BUILD_PARALLEL_LEVEL}")
endif()


if(NOT ZeroMQ_VERSION)
  set(ZeroMQ_VERSION "4.3.5")
endif()
include(${MY_CMAKE_FILES}ZeroMQ.cmake)

if(NOT Boost_VERSION)
  set(Boost_VERSION "1.85.0")
endif()
include(${MY_CMAKE_FILES}Boost.cmake)

if(NOT FairLogger_VERSION)
  set(FairLogger_VERSION "2.3.0")
endif()
include(${MY_CMAKE_FILES}FairLogger.cmake)

if(NOT FairMQ_VERSION)
  set(FairMQ_VERSION "1.10.0")
endif()
include(${MY_CMAKE_FILES}FairMQ.cmake)

if(NOT hiredis_VERSION)
  set(hiredis_VERSION "1.3.0")
endif()
include(${MY_CMAKE_FILES}hiredis.cmake)

if(NOT redis_plus_plus_VERSION)
  set(redis_plus_plus_VERSION "1.3.15")
endif()
include(${MY_CMAKE_FILES}redis_plus_plus.cmake)

if(WITH_OTEL_CPP)
  if(NOT opentelemetry-cpp_VERSION)
    set(opentelemetry-cpp_VERSION "1.24.0")
  endif()
  include(${MY_CMAKE_FILES}opentelemetry-cpp.cmake)
endif()

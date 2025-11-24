# 1. Find the insalled opentelemetry-cpp using find_package()
# 2. If the package is not found, fetch and build opentelemetry-cpp from GitHub


message(STATUS "========== include opentelemetry-cpp.cmake ==========")

set(opentelemetry-cpp_GIT_TAG "v${opentelemetry-cpp_VERSION}")

find_package(opentelemetry-cpp ${opentelemetry-cpp_VERSION} QUIET)
if(opentelemetry-cpp_FOUND)
  message(STATUS "Found opentelemetry-cpp")
else()
  message(STATUS "opentelemetry-cpp not found. --- fetch from GitHub")
  if(DEFINED ENV{CMAKE_BUILD_PARALLEL_LEVEL})
    set(PARALLEL_JOBS $ENV{CMAKE_BUILD_PARALLEL_LEVEL})
  else()
    # fallback
    include(ProcessorCount)
    ProcessorCount(PARALLEL_JOBS)
  endif()

  include(ExternalProject)
  ExternalProject_Add(
    opentelemetry-cpp
    GIT_REPOSITORY https://github.com/open-telemetry/opentelemetry-cpp
    GIT_TAG        ${opentelemetry-cpp_GIT_TAG}

    CMAKE_ARGS
      -DCMAKE_BUILD_TYPE=Release
      -DCMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX}
      -DCMAKE_CXX_STANDARD=${CMAKE_CXX_STANDARD}
      -DCMAKE_EXE_LINKER_FLAGS=${CMAKE_EXE_LINKER_FLAGS}
      -DCMAKE_SHARED_LINKER_FLAGS=${CMAKE_SHARED_LINKER_FLAGS}
      -DBUILD_TESTING=OFF
      -DWITH_OTLP_GRPC=ON
      -DWITH_OTLP_HTTP=ON
      -DWITH_PROMETHEUS=OFF
  )
  set(opentelemetry-cpp_DIR ${CMAKE_INSTALL_PREFIX})
endif()


message(STATUS "========== include opentelemetry-cpp.cmake done ==========")
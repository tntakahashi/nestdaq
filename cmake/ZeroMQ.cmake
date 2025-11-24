# 1. Find the insalled ZeroMQ using find_package()
# 2. If the package is not found, fetch and build ZeroMQ from GitHub

message(STATUS "========== include ZeroMQ.cmake ==========")

set(ZeroMQ_GIT_TAG "v${ZeroMQ_VERSION}")

find_package(ZeroMQ ${ZeroMQ_VERSION} QUIET)
if(ZeroMQ_FOUND)
  message(STATUS "Found ZeroMQ")
else()
  message(STATUS "ZeroMQ not found. --- fetch from GitHub")
  if(DEFINED ENV{CMAKE_BUILD_PARALLEL_LEVEL})
    set(PARALLEL_JOBS $ENV{CMAKE_BUILD_PARALLEL_LEVEL})
  else()
    # fallback
    include(ProcessorCount)
    ProcessorCount(PARALLEL_JOBS)
  endif()

  include(ExternalProject)
  ExternalProject_Add(
    ZeroMQ
    GIT_REPOSITORY https://github.com/zeromq/libzmq
    GIT_TAG        ${ZeroMQ_GIT_TAG}

    CMAKE_ARGS
      -Wno-dev
      -DCMAKE_BUILD_TYPE=Release
      -DCMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX}
      -DCMAKE_CXX_STANDARD=${CMAKE_CXX_STANDARD}
      -DCMAKE_EXE_LINKER_FLAGS=${CMAKE_EXE_LINKER_FLAGS}
      -DCMAKE_SHARED_LINKER_FLAGS=${CMAKE_SHARED_LINKER_FLAGS}
  )
  set(ZeroMQ_DIR ${CMAKE_INSTALL_PREFIX})
endif()


message(STATUS "========== include ZeroMQ.cmake done ==========")
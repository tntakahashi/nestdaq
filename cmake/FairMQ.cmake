# 1. Find the insalled FairMQ using find_package()
# 2. If the package is not found,  fetch and build FairMQ from GitHub

message(STATUS "========== include FairMQ.cmake ==========")
set(FairMQ_GIT_TAG "v${FairMQ_VERSION}")

# FairMQ uses CMAKE_SOURCE_DIR internally. 
# FetchContent causes this variable to point to the parent project's root rather than FairMQ's own source directory.

find_package(FairMQ ${FairMQ_VERSION} QUIET)

if(FairMQ_FOUND)
  message(STATUS "Found FairMQ")

else()
  message(STATUS "FairMQ not found. --- fetch from GitHub")

  if(DEFINED ENV{CMAKE_BUILD_PARALLEL_LEVEL})
    set(PARALLEL_JOBS $ENV{CMAKE_BUILD_PARALLEL_LEVEL})
  else()
    # fallback
    include(ProcessorCount)
    ProcessorCount(PARALLEL_JOBS)
  endif()

  include(ExternalProject)
  ExternalProject_Add(
    FairMQ
    GIT_REPOSITORY https://github.com/FairRootGroup/FairMQ.git
    GIT_TAG        ${FairMQ_GIT_TAG}

    DEPENDS ZeroMQ boost FairLogger

    # linker flags must be empty
    CMAKE_ARGS
      -DCMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX}
      -DCMAKE_PREFIX_PATH=${CMAKE_INSTALL_PREFIX}
      -DCMAKE_CXX_STANDARD=${CMAKE_CXX_STANDARD}
      -DCMAKE_EXE_LINKER_FLAGS=
      -DCMAKE_SHARED_LINKER_FLAGS=
  )
  set(FairMQ_DIR ${CMAKE_INSTALL_PREFIX})
endif()

message(STATUS "========== include FairMQ.cmake done ==========")
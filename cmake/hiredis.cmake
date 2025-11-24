# 1. Find the insalled hiredis package using find_package()
# 2. If the package is not found, fetch and build hiredis from GitHub by ExternalProject_Add()

message(STATUS "========== include hiredis.cmake ==========")

set(hiredis_GIT_TAG "v${hiredis_VERSION}")

find_package(hiredis ${hiredis_VERSION} QUIET)
if(hiredis_FOUND)
  message(STATUS "Found hiredis")
else()
  message(STATUS "hiredis not found. --- fetch from GitHub")
  if(DEFINED ENV{CMAKE_BUILD_PARALLEL_LEVEL})
    set(PARALLEL_JOBS $ENV{CMAKE_BUILD_PARALLEL_LEVEL})
  else()
    # fallback
    include(ProcessorCount)
    ProcessorCount(PARALLEL_JOBS)
  endif()

  include(ExternalProject)
  ExternalProject_Add(
    hiredis
    GIT_REPOSITORY https://github.com/redis/hiredis.git


    CMAKE_ARGS
      -DCMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX}
      -DCMAKE_EXE_LINKER_FLAGS=${CMAKE_EXE_LINKER_FLAGS}
      -DCMAKE_SHARED_LINKER_FLAGS=${CMAKE_SHARED_LINKER_FLAGS}
  )
  set(hiredis_DIR ${CMAKE_INSTALL_PREFIX})
endif()


message(STATUS "========== include hiredis.cmake done ==========")


# 1. Find the insalled redis_plus_plus package using find_package()
# 2. If the package is not found, fetch and build redis_plus_plus from GitHub by ExternalProject_Add()

message(STATUS "========== include redis_plus_plus.cmake ==========")

set(redis_plus_plus_GIT_TAG "${redis_plus_plus_VERSION}")

find_package(redis_plus_plus ${redis_plus_plus_VERSION} QUIET)
if(redis_plus_plus_FOUND)
  message(STATUS "Found redis_plus_plus")
else()
  message(STATUS "redis_plus_plus not found. --- fetch from GitHub")
  if(DEFINED ENV{CMAKE_BUILD_PARALLEL_LEVEL})
    set(PARALLEL_JOBS $ENV{CMAKE_BUILD_PARALLEL_LEVEL})
  else()
    # fallback
    include(ProcessorCount)
    ProcessorCount(PARALLEL_JOBS)
  endif()

  include(ExternalProject)
  ExternalProject_Add(
    redis_plus_plus
    GIT_REPOSITORY https://github.com/sewenew/redis-plus-plus.git
    GIT_TAG        ${redis_plus_plus_GIT_TAG}

    DEPENDS hiredis

    CMAKE_ARGS
      -DCMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX}
      -DCMAKE_PREFIX_PATH=${CMAKE_INSTALL_PREFIX}
      -DREDIS_PLUS_PLUS_CXX_STANDARD=${CMAKE_CXX_STANDARD}
      -DREDIS_PLUS_PLUS_BUILD_TEST=OFF
      -DCMAKE_EXE_LINKER_FLAGS=${CMAKE_EXE_LINKER_FLAGS}
      -DCMAKE_SHARED_LINKER_FLAGS=${CMAKE_SHARED_LINKER_FLAGS}
  )
  set(redis_plus_plus_DIR ${CMAKE_INSTALL_PREFIX})
endif()


message(STATUS "========== include redis_plus_plus.cmake done ==========")


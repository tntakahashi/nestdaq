# 1. Find the insalled Boost package using find_package()
# 2. If the package is not found, fetch and build Boost from GitHub

message(STATUS "========== include FairLogger.cmake ==========")
set(FairLogger_GIT_TAG "v${FairLogger_VERSION}")

# FairLogger uses CMAKE_SOURCE_DIR internally. 
# FetchContent causes this variable to point to the parent project's root rather than FairLogger's own source directory.

find_package(FairLogger ${FairLogger_VERSION} QUIET)
if(FairLogger_FOUND)
  message(STATUS "Found FairLogger")

else()
  message(STATUS "FairLogger not found. --- fetching from GitHub")

  if(DEFINED ENV{CMAKE_BUILD_PARALLEL_LEVEL})
      set(PARALLEL_JOBS $ENV{CMAKE_BUILD_PARALLEL_LEVEL})
  else()
      # fallback
      include(ProcessorCount)
      ProcessorCount(PARALLEL_JOBS)
  endif()
  
  include(ExternalProject)
  ExternalProject_Add(
    FairLogger
    GIT_REPOSITORY https://github.com/FairRootGroup/FairLogger.git
    GIT_TAG        ${FairLogger_GIT_TAG}

    DEPENDS boost

    CMAKE_ARGS
      -DEXTERNAL_FMT=OFF 
      -DCMAKE_BUILD_TYPE=Release
      -DCMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX}
      -DCMAKE_PREFIX_PATH=${CMAKE_INSTALL_PREFIX}
      -DCMAKE_CXX_STANDARD=${CMAKE_CXX_STANDARD}
      -DCMAKE_EXE_LINKER_FLAGS=${CMAKE_EXE_LINKER_FLAGS}
      -DCMAKE_SHARED_LINKER_FLAGS=${CMAKE_SHARED_LINKER_FLAGS}
  )
  set(FairLogger_DIR ${CMAKE_INSTALL_PREFIX})
endif()

message(STATUS "========== include FairLogger.cmake done ==========")
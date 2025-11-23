# 1. Find the insalled Boost package using find_package()
# 2. If the package is not found, fetch and build Boost from GitHub

message(STATUS "========== include FairLogger.cmake ==========")

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
  ExternalProject_Add(FairLogger
    GIT_REPOSITORY https://github.com/FairRootGroup/FairLogger.git
    GIT_TAG        ${FAIRLOGGER_GIT_TAG}
    GIT_PROGRESS   TRUE

    UPDATE_COMMAND ""

    CONFIGURE_COMMAND
      ${CMAKE_COMMAND} 
        -DEXTERNAL_FMT=OFF 
        -DCMAKE_BUILD_TYPE=Release
        -DCMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX}
        -DCMAKE_CXX_STANDARD=${CMAKE_CXX_STANDARD}
        -DCMAKE_EXE_LINKER_FLAGS=${CMAKE_EXE_LINKER_FLAGS}
        -DCMAKE_SHARED_LINKER_FLAGS=${CMAKE_SHARED_LINKER_FLAGS}
        -S <SOURCE_DIR> 
        -B <BINARY_DIR>

    BUILD_COMMAND
      ${CMAKE_COMMAND} --build <BINARY_DIR> -j${PARALLEL_JOBS}
    
    INSTALL_COMMAND
      ${CMAKE_COMMAND} --install <BINARY_DIR>
  )
  set(FairLogger_ROOT ${CMAKE_INSTALL_PREFIX})
endif()

message(STATUS "========== include FairLogger.cmake done ==========")

#  CMAKE_ARGS
#    -DEXTERNAL_FMT=OFF 
#    -DCMAKE_BUILD_TYPE=Release
#    -DCMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX}
#    -DCMAKE_CXX_STANDARD=${CMAKE_CXX_STANDARD}
#    -DCMAKE_EXE_LINKER_FLAGS=${CMAKE_EXE_LINKER_FLAGS}
#    -DCMAKE_SHARED_LINKER_FLAGS=${CMAKE_SHARED_LINKER_FLAGS}
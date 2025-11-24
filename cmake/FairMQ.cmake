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
      -DCMAKE_BUILD_TYPE=Release
      -DCMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX}
      -DCMAKE_PREFIX_PATH=${CMAKE_INSTALL_PREFIX}
      -DCMAKE_CXX_STANDARD=${CMAKE_CXX_STANDARD}
      -DCMAKE_EXE_LINKER_FLAGS=
      -DCMAKE_SHARED_LINKER_FLAGS=
  )
  set(FairMQ_DIR ${CMAKE_INSTALL_PREFIX})
endif()

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

message(STATUS "========== include FairMQ.cmake done ==========")
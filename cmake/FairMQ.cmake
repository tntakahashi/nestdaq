# 1. Find the insalled FairMQ using find_package()
# 2. Use FairMQ from a bundled git submodle (if present) 
# 3. Use FetchContent to fetch and build FairMQ from GitHub

include(CMakeFindDepndencyMacro)

# =========================================================
# 1. Try to find system-installed FairMQ
# =========================================================
find_package(FairMQ QUIET)


find_package(Boost)


if(FairMQ_FOUND)
  message(STATUS "Found system FairMQ")
else()
  # =========================================================
  # 2. Try to use submodule version in external/FairMQ
  # =========================================================

  message(STATUS "FairMQ not found in system --- checking submodule...")
  set(FairMQ_SUBMODULE_PATH ${CMAKE_SOURCE_DIR/external/FairMQ})
  
  if(EXISTS "${FairMQ_SUBMODULE_PATH}/CMakeLists.txt")
    message(STATUS "Found FairMQ submodule at: ${FairMQ_SUBMODULE_PATH}")
    add_subdirectiry(${FairMQ_SUBMODULE_PATH} EXCLUDE_FROM_ALL)
  else()
    message(STATUS "FairMQ submodule not found --- fetching from GitHub)

    # =========================================================
    # 3. FetchContent to clone and build FairMQ from GitHub
    # =========================================================

   include(FetchContent)
   FetchContent_Declare(
     "FairMQ"
     GIT_REPOSITORY https://github.com/FairRootGroup/FairMQ.github
     GIT_TAG $(FairMQ_GIT_TAG)
   )
   FetchContent_MakeAvailable(FairMQ)
   endif()
endif()

find_package(FairMQ REQUIRED)

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
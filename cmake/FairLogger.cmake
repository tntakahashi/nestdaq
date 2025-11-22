# 1. Find the insalled Boost package using find_package()
# 2. Use Boost from a bundled git submodle (if present) 
# 3. Use FetchContent to fetch and build Boost from GitHub

# =========================================================
# 1. Try to find an installed FairLogger
# =========================================================
find_package(FairLogger)

if(FairLogger_FOUND)
  message(STATUS "Found system FairLogger")

else()
  message(STATUS "FairLogger not found in system --- checking submodule...")
  set(FairLogger_SUBMODULE_PATH "${CMAKE_SOURCE_DIR}/external/FairLogger")
  
  if(EXISTS ${FairLogger_SUBMODULE_PATH})
    message(STATUS "Using FairLogger submodule at ${FairLogger_SUBMODULE_PATH}")
    add_subdirectory(${FairLogger_SUBMODULE_PATH} EXCLUDE_FROM_ALL)
  else()
    message(STATUS "No FairLogger submodule --- fetching from GitHub")
    include(FetchContent)
    FetchContent_Declare(
      FairLogger
      GIT_REPOSITORY https://github.com/FairRootGroup/FairLogger.git
      GIT_TAG ${FairLogger_GIT_TAG}
    )
    FetchContent_MakeAvailable(FairLogger)

  endif()
endif()

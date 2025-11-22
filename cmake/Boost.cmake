# 1. Find the insalled Boost package using find_package()
# 2. Use Boost from a bundled git submodle (if present) 
# 3. Use FetchContent to fetch and build Boost from GitHub


# =========================================================
# 1. Try to find an installed Boost 
# =========================================================
find_package(Boost ${Boost_VERSION} QUIET filesystem iostreams thread program_options)

if(Boost_FOUND)
  message(STATUS "Found system Boost: ${Boost_VERSION}")

else()
  message(STATUS "Boost not found in system --- checking submodule...")

  # =========================================================
  # 2. Use Boost from a git submodule (if available)
  # =========================================================
  set(BOOST_SUBMODULE_PATH "${CMAKE_SOURCE_DIR}/external/boost")

  if(EXISTS "${BOOST_SUBMODULE_PATH}/CMakeLists.txt" OR EXISTS ${BOOST_SUBMODULE_PATH/boost}")
    message(STATUS "Using Boost submodule at ${BOOST_SUBMODULE_PATH}")

    message(STATUS "BOOST_IOSTREAMS_ENABLE_ZLIB:  ${BOOST_IOSTREAMS_ENABLE_ZLIB}")
    message(STATUS "BOOST_IOSTREAMS_ENABLE_BZIP2: ${BOOST_IOSTREAMS_ENABLE_BZIP2}")
    message(STATUS "BOOST_IOSTREAMS_ENABLE_ZSTD:  ${BOOST_IOSTREAMS_ENABLE_ZSTD}")
    add_subdirectory(${BOOST_SUBMODULE_PATH} EXCLUDE_FROM_ALL)

  else()
    message(STATUS "No Boost submodule --- fetching from GitHub")

    # =========================================================
    # 3. Use FetchContent to fetch and build Boost from GitHub
    # =========================================================
    include(FetchContent)
    FetchContent_Declare(
      boost
      GIT_REPOSITORY https://github.com/boostorg/boost.git
      GIT_TAG ${BOOST_GIT_TAG}
    )
    # boost uses nestd submodules --- ensure they are feched
    set(FETCHCONTENT_QUIET FALSE)
    set(BOOST_ENABLE_CMAKE ON CACHE BOOL "" FORCE)
    message(STATUS "BOOST_IOSTREAMS_ENABLE_ZLIB:  ${BOOST_IOSTREAMS_ENABLE_ZLIB}")
    message(STATUS "BOOST_IOSTREAMS_ENABLE_BZIP2: ${BOOST_IOSTREAMS_ENABLE_BZIP2}")
    message(STATUS "BOOST_IOSTREAMS_ENABLE_ZSTD:  ${BOOST_IOSTREAMS_ENABLE_ZSTD}")

    FetchContent_MakeAvailable(boost)
  endif()
endif()

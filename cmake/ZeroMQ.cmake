# 1. Find the insalled ZeroMQ using find_package()
# 2. Use ZeroMQ from a bundled git submodle (if present) 
# 3. Use FetchContent to fetch and build ZeroMQ from GitHub

# =========================================================
# 1. Try to find an installed Boost 
# =========================================================
find_package(ZeroMQ CONFIG QUIET)

if(ZeroMQ_FOUND)
  message(STATUS "Found system ZeroMQ")

else()
  message(STATUS "ZeroMQ not found in system --- checking submodule...")

  # =========================================================
  # 2. Use ZeroMQ from a git submodule (if available)
  # =========================================================
  set(ZEROMQ_SUBMODULE_PATH "${CMAKE_SOURCE_DIR}/external/libzmq")
  if(EXISTS "${ZEROMQ_SUBMODULE_PATH}")
    message(STATUS "Using ZeroMQ submodule at ${ZEROMQ_SUBMODULE_PATH}")

    add_subdirectory(${ZEROMQ_SUBMODULE_PATH} EXCLUDEFOM_ALL)

  else()
    message(STATUS "No ZeroMQ submodule --- fetching from GitHub")
    # =========================================================
    # 3. Use FetchContent to fetch and build ZeroMQ from GitHub
    # =========================================================
    include(FetchContent)
    FetchContent_Declare(
      zeromq 
      GIT_REPOSITORY https://github.com/zeromq/libzmq.git 
      GIT_TAG ${ZEROMQ_GIT_TAG}
      CMAKE_ARGS
        -DCMAKE_BUILD_TYPE=Release
        -DCMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX}
        -DCMAKE_CXX_STANDARD=${CMAKE_CXX_STANDARD}
        -DCMAKE_EXE_LINKER_FLAGS=${CMAKE_EXE_LINKER_FLAGS}
        -DCMAKE_SHARED_LINKER_FLAGS=${CMAKE_SHARED_LINKER_FLAGS}
        -DCMAKE_POSITION_INDEPENDENT_CODE=${CMAKE_POSITIN_INDEPENDENT_CODE}
        -DCMAKE_SKIP_BUILD_RPATH=${CMAKE_SKIP_BUILD_RPATH}
        -DCMAKE_INSTALL_RPATH_USE_LINK_PATH=${CMAKE_INSTALL_RPATH_USE_LINK_PATH}
        -DCMAKE_BUILD_WITH_INSTALL_RPATH=${CMAKE_BUILD_WITH_INSTALL_RPATH}
    )
    FetchContent_MakeAvailable(zeromq)
  endif()
endif()

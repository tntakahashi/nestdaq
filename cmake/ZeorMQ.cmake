# 1. Find the insalled ZeroMQ using find_package()
# 2. Use ZeroMQ from a bundled git submodle (if present) 
# 3. Use FetchContent to fetch and build ZeroMQ from GitHub

# =========================================================
# 1. Try to find an installed Boost 
# =========================================================
find_package(ZeroMQ Config QUIET)

if(ZeroMQ_FOUND)
  message(STATUS "Found system ZeroMQ")

else()
  message(STATUS "ZeroMQ not found in system --- checking submodule...")

  # =========================================================
  # 2. Use ZeroMQ from a git submodule (if available)
  # =========================================================
  set(ZEROMQ_SUBMODULE_PATH "${CMAKE_SOURCE_DIR}/external/libzmq")
  if(EXITS "${ZEROMQ_SUBMODULE_PATH}")
    message(STATUS "Using ZeroMQ submodule at ${ZEROMQ_SUBMODULE_PATH}")

    add_subdirectory(${ZEROMQ_SUBMODULE_PATH} EXCLUDEFOM_ALL)

  else()
    message(STATU "No ZeroMQ submodule --- fetching from GitHub")
    # =========================================================
    # 3. Use FetchContent to fetch and build ZeroMQ from GitHub
    # =========================================================
    include(FetchContent)
    FetchContent_Declare(
      zeromq https://github.com/zeromq/libzmq.git 
      GIT_TAG ${ZEROMQ_GIT_TAG}
    )
    FetchContent_MakeAvailable(zeromq)
  endif()
endif()

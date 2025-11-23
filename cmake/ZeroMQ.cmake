# 1. Find the insalled ZeroMQ using find_package() in FetchContent_MakeAvailable()
# 2. If the package is not found, fetch and build ZeroMQ from GitHub

message(STATUS "========== include ZeroMQ.cmake ==========")

include(FetchContent)
FetchContent_Declare(
  ZeroMQ
  GIT_REPOSITORY         https://github.com/zeromq/libzmq.git 
  GIT_TAG                ${ZEROMQ_GIT_TAG}
  CMAKE_ARGS
    -DCMAKE_BUILD_TYPE=Release
    -DCMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX}
    -DCMAKE_CXX_STANDARD=${CMAKE_CXX_STANDARD}
    -DCMAKE_EXE_LINKER_FLAGS=${CMAKE_EXE_LINKER_FLAGS}
    -DCMAKE_SHARED_LINKER_FLAGS=${CMAKE_SHARED_LINKER_FLAGS}
    -DCMAKE_POSITION_INDEPENDENT_CODE=${CMAKE_POSITION_INDEPENDENT_CODE}
    -DCMAKE_SKIP_BUILD_RPATH=${CMAKE_SKIP_BUILD_RPATH}
    -DCMAKE_INSTALL_RPATH_USE_LINK_PATH=${CMAKE_INSTALL_RPATH_USE_LINK_PATH}
    -DCMAKE_BUILD_WITH_INSTALL_RPATH=${CMAKE_BUILD_WITH_INSTALL_RPATH}
  FIND_PACKAGE_ARGS 
)
FetchContent_MakeAvailable(ZeroMQ)

message(STATUS "========== include ZeroMQ.cmake done ==========")
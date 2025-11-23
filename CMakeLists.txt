cmake_minimum_required(VERSION 3.11)

include(cmake/GitHelper.cmake)

project(nestdaq LANGUAGES CXX C)

message(STATUS "PROJECT_NAME: ${PROJECT_NAME}")

# check c++ compiler and turn on colored output
message("c++ compiler ID is ${CMAKE_CXX_COMPILER_ID}")
if("${CMAKE_CXX_COMPILER_ID}" MATCHES "GNU")
  message("turnn on colored error message of GNU")
  add_compile_options(-fdiagnostics-color=always)
elseif("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")
  message("turn on colored error message of Clang")
  add_compile_options(-fcolor-diagnostics)
endif()

#------------------------------------------------------------------------------
# C++ standard level
set(MY_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wshadow -Wfloat-equal")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -Ofast -DNDEBUG -march=native")
message("")
message("CMAKE_CXX_FLAGS:                ${CMAKE_CXX_FLAGS}")
message("CMAKE_CXX_FLAGS_DEBUG:          ${CMAKE_CXX_FLAGS_DEBUG}")
message("CMAKE_CXX_FLAGS_RELWITHDEBINFO: ${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")
message("CMAKE_CXX_FLAGS_RELEASE:        ${CMAKE_CXX_FLAGS_RELEASE}")
message("CMAKE_CXX_FLAGS_MINSIZEREL:     ${CMAKE_CXX_FLAGS_MINSIZEREL}")
message("CMAKE_BUILD_TYPE:               ${CMAKE_BUILD_TYPE}")
message("CMAKE_EXE_LINKER_FLAGS:         ${CMAKE_EXE_LINKER_FLAGS}")
message("CMAKE_SHARED_LINKER_FLAGS:      ${CMAKE_SHARED_LINKER_FLAGS}")
message("")

#------------------------------------------------------------------------------
include(GNUInstallDirs)
message("CMAKE_INSTALL_PREFIX:     ${CMAKE_INSTALL_PREFIX}")
message("CMAKE_INSTALL_INCLUDEDIR: ${CMAKE_INSTALL_INCLUDEDIR}")
message("CMAKE_INSTALL_LIBDIR:     ${CMAKE_INSTALL_LIBDIR}")
message("CMAKE_INSTALL_BINDIR:     ${CMAKE_INSTALL_BINDIR}")
message("CMAKE_INSTALL_DATADIR:    ${CMAKE_INSTALL_DATADIR}")
message("")


#------------------------------------------------------------------------------
# RPATH handling
set(CMAKE_SKIP_BUILD_RPATH FALSE)
set(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE)
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
if(APPLE)
    set(CMAKE_INSTALL_RPATH "@loader_path/../${CMAKE_INSTALL_LIBDIR}")
elseif(UNIX)
    set(CMAKE_INSTALL_RPATH "$ORIGIN/../${CMAKE_INSTALL_LIBDIR}")
endif()

#------------------------------------------------------------------------------
find_package(Threads REQUIRED)

include(cmake/ZeroMQ.cmake)
include(cmake/Boost.cmake)
include(cmake/FairLogger.cmake)
# include(cmake/FairMQ.cmake)
# 
# 
# #------------------------------------------------------------------------------
# add_subdirectory(nestdaq)
# add_subdirectory(plugins)
# add_subdirectory(examples)
# add_subdirectory(controller)
# add_subdirectory(share)
# add_subdirectory(scripts)

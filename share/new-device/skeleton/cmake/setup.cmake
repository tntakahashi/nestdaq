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
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(MY_CXX_STANDARD 17)
if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD ${MY_CXX_STANDARD})
elseif(${CMAKE_CXX_STANDARD} LESS ${MY_CXX_STANDARD})
  message(FATAL_ERROR "A minimum CMAKE_CXX_STANDARD of ${MY_CXX_STANDARD} is required.")
elseif(${CMAKE_CXX_STANDARD} GREATER ${MY_CXX_STANDARD})
  message(WARNING "A CMAKE_CXX_STANDARD of ${CMAKE_CXX_STANDARD} (greater than ${MY_CXX_STANDARD}) is not tested. Use on your on risk.")
endif()
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wshadow -Wfloat-equal -O3")
set(CMAKE_CXX_FLAGS_RELEASE "-Ofast -DNDEBUG -march=native")
message("")
message("CMAKE_CXX_FLAGS:                ${CMAKE_CXX_FLAGS}")
message("CMAKE_CXX_FLAGS_DEBUG:          ${CMAKE_CXX_FLAGS_DEBUG}")
message("CMAKE_CXX_FLAGS_RELWITHDEBINFO: ${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")
message("CMAKE_CXX_FLAGS_RELEASE:        ${CMAKE_CXX_FLAGS_RELEASE}")
message("CMAKE_CXX_FLAGS_MINSIZEREL:     ${CMAKE_CXX_FLAGS_MINSIZEREL}")
message("CMAKE_BUILD_TYPE:               ${CMAKE_BUILD_TYPE}")
message("")

#------------------------------------------------------------------------------
include(GNUInstallDirs)
message("CMAKE_INSTALL_PREFIX:     ${CMAKE_INSTALL_PREFIX}")
message("CMAKE_INSTALL_INCLUDEDIR: ${CMAKE_INSTALL_INCLUDEDIR}")
message("CMAKE_INSTALL_LIBDIR:     ${CMAKE_INSTALL_LIBDIR}")
message("CMAKE_INSTALL_BINDIR:     ${CMAKE_INSTALL_BINDIR}")
message("CMAKE_INSTALL_DATADIR:    ${CMAKE_INSTALL_DATADIR}")
message("")

find_package(Threads REQUIRED)

#------------------------------------------------------------------------------
# RPATH handling
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
list(FIND CMAKE_PLATFORM_IMPLICIT_LINK_DIRECTORIES "${CMAKE_INSTALL_PREFIX/${CMAKE_INSTALL_LIBDIR}}" isSystemDir)
if("${isSystemDir}" STREQUAL "-1")
  if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(CMAKE_EXE_LINKER_FLAGS    "${CMAKE_EXE_LINKER_FLAGS} -Wl,--enable-new-dtags")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--enable-new-dtags")
    set(CMAKE_INSTALL_RPATH       "$ORIGIN/../${CMAKE_INSTALL_LIBDIR}")
  elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(CMAKE_INSTALL_RPATH "@loader_path/../${CMAKE_INSTALL_LIBDIR}")
  else()
    set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}")
  endif()
endif()

message("CMAKE_EXE_LINKER_FLAGS:    ${CMAKE_EXE_LINKER_FLAGS}")
message("CMAKE_SHARED_LINKER_FLAGS: ${CMAKE_SHARED_LINKER_FLAGS}")

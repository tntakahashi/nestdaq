cmake_minimum_required(VERSION 3.24)

project(NestDAQExamples LANGUAGES CXX)

include(GNUInstallDirs)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
option(NESTDAQ_ENABLE_CLANG_TIDY "Run clang-tidy during builds" OFF)
if(NESTDAQ_ENABLE_CLANG_TIDY)
  find_program(CLANG_TIDY_EXE NAMES clang-tidy REQUIRED)
  set(CMAKE_CXX_CLANG_TIDY "${CLANG_TIDY_EXE}")
  message(STATUS "clang-tidy enabled: ${CLANG_TIDY_EXE}")
endif()

# Standalone examples do not inherit RPATH settings from the main NestDAQ build.
set(CMAKE_SKIP_BUILD_RPATH FALSE)
set(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE)
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
if(APPLE)
  set(CMAKE_INSTALL_RPATH "@loader_path/../${CMAKE_INSTALL_LIBDIR}")
elseif(UNIX)
  set(CMAKE_INSTALL_RPATH "$ORIGIN/../${CMAKE_INSTALL_LIBDIR}")
endif()

find_package(NestDAQ REQUIRED CONFIG)

foreach(exec IN ITEMS Sampler Sink NullDevice)
  add_executable(${exec}
    ${exec}.cxx
  )

  target_link_libraries(${exec} PUBLIC
    NestDAQ::NestDAQ
  )

  install(TARGETS ${exec}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  )
endforeach()

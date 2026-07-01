cmake_minimum_required(VERSION 3.22)

if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
  project(NestDAQExamples LANGUAGES CXX)
endif()

include("${CMAKE_CURRENT_LIST_DIR}/../cmake/NestDAQExamplesStandalone.cmake")

if(NOT TARGET NestDAQ::NestDAQ)
  find_package(NestDAQ REQUIRED CONFIG)
endif()

find_package(spdlog CONFIG QUIET)

foreach(exec IN ITEMS Sampler Sink NullDevice)
  add_executable(${exec}
    ${exec}.cxx
  )

  target_link_libraries(${exec} PUBLIC
    NestDAQ::NestDAQ
  )

  if(TARGET spdlog::spdlog)
    target_link_libraries(${exec} PRIVATE
      spdlog::spdlog
    )
  endif()

  install(TARGETS ${exec}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  )
endforeach()

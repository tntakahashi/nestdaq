cmake_minimum_required(VERSION 3.24)

project(NestDAQExamples LANGUAGES CXX)

include(GNUInstallDirs)

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

cmake_minimum_required(VERSION 3.22)

if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
  project(NestDAQExamples LANGUAGES CXX)
endif()

include("${CMAKE_CURRENT_LIST_DIR}/../cmake/NestDAQExamplesStandalone.cmake")

if(NOT TARGET NestDAQ::NestDAQ)
  find_package(NestDAQ REQUIRED CONFIG)
endif()

find_package(spdlog CONFIG QUIET)

set(_nestdaq_examples_spdlog_otel_link)
if(TARGET spdlog::spdlog)
  if(TARGET nestdaq_otel AND NestDAQ_HAVE_SPDLOG_TELEMETRY)
    set(_nestdaq_examples_spdlog_otel_link nestdaq_otel)
  elseif(TARGET NestDAQ::NestDAQ)
    get_target_property(_nestdaq_examples_include_dirs NestDAQ::NestDAQ INTERFACE_INCLUDE_DIRECTORIES)
    foreach(_nestdaq_examples_include_dir IN LISTS _nestdaq_examples_include_dirs)
      if(EXISTS "${_nestdaq_examples_include_dir}/nestdaq/telemetry/SpdlogOpenTelemetrySink.h")
        get_filename_component(_nestdaq_examples_prefix "${_nestdaq_examples_include_dir}" DIRECTORY)
        find_library(_nestdaq_examples_otel_library
          NAMES nestdaq_otel
          PATHS
            "${_nestdaq_examples_prefix}/lib64"
            "${_nestdaq_examples_prefix}/lib"
          NO_DEFAULT_PATH
          NO_CACHE
        )
        if(_nestdaq_examples_otel_library)
          set(_nestdaq_examples_spdlog_otel_link "${_nestdaq_examples_otel_library}")
        endif()
        unset(_nestdaq_examples_prefix)
      endif()
    endforeach()
    unset(_nestdaq_examples_include_dirs)
  endif()
endif()

foreach(exec IN ITEMS Sampler Sink NullDevice)
  add_executable(${exec}
    ${exec}.cxx
  )

  target_link_libraries(${exec} PUBLIC
    NestDAQ::NestDAQ
  )

  if(_nestdaq_examples_spdlog_otel_link)
    target_link_libraries(${exec} PRIVATE
      spdlog::spdlog
      ${_nestdaq_examples_spdlog_otel_link}
    )
    target_compile_definitions(${exec} PRIVATE
      NESTDAQ_EXAMPLES_HAVE_SPDLOG_OTEL
    )
  endif()

  install(TARGETS ${exec}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  )
endforeach()

unset(_nestdaq_examples_otel_library)
unset(_nestdaq_examples_spdlog_otel_link)

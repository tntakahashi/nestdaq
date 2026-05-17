find_package(Catch2 CONFIG REQUIRED)
include(Catch)

add_executable(nestdaq_telemetry_options_tests
  fairmq_throughput_log_parser_tests.cxx
  telemetry_options_tests.cxx
)

target_include_directories(nestdaq_telemetry_options_tests PRIVATE
  ${CMAKE_SOURCE_DIR}
  ${CMAKE_BINARY_DIR}
  ${FairLogger_INCDIR}
)

target_link_libraries(nestdaq_telemetry_options_tests PRIVATE
  nestdaq_telemetry
  nestdaq_fairmq_throughput_parser
  Catch2::Catch2WithMain
  FairMQ::FairMQ
  Boost::program_options
)

catch_discover_tests(nestdaq_telemetry_options_tests)

if(opentelemetry-cpp_FOUND)
  add_executable(nestdaq_telemetry_plugin_tests
    telemetry_plugin_tests.cxx
  )

  target_include_directories(nestdaq_telemetry_plugin_tests PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_BINARY_DIR}
    ${FairLogger_INCDIR}
  )

  target_compile_definitions(nestdaq_telemetry_plugin_tests PRIVATE
    NESTDAQ_OTEL_LIBRARY_PATH="$<TARGET_FILE:nestdaq_otel>"
  )

  target_link_libraries(nestdaq_telemetry_plugin_tests PRIVATE
    nestdaq_telemetry
    Catch2::Catch2WithMain
    FairMQ::FairMQ
    Boost::program_options
    nlohmann_json::nlohmann_json
    ${CMAKE_DL_LIBS}
  )

  add_dependencies(nestdaq_telemetry_plugin_tests nestdaq_otel)
  catch_discover_tests(nestdaq_telemetry_plugin_tests)
endif()

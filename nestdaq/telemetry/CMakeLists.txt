install(FILES
  ${CMAKE_CURRENT_SOURCE_DIR}/FairLoggerTelemetryLoader.h
  ${CMAKE_CURRENT_SOURCE_DIR}/OpenTelemetryInitializer.h
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/nestdaq/telemetry
)

if(opentelemetry-cpp_FOUND)
  add_library(nestdaq_fairlogger_otel SHARED
    FairLoggerOpenTelemetrySink.cxx
    OpenTelemetryInitializer.cxx
  )

  target_include_directories(nestdaq_fairlogger_otel PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_BINARY_DIR}
    ${FairLogger_INCDIR}
  )

  target_link_directories(nestdaq_fairlogger_otel PRIVATE
    ${FairLogger_LIBDIR}
  )

  target_link_libraries(nestdaq_fairlogger_otel PRIVATE
    FairLogger
    opentelemetry-cpp::logs
    opentelemetry-cpp::resources
    opentelemetry-cpp::ostream_log_record_exporter
    opentelemetry-cpp::otlp_http_log_record_exporter
    opentelemetry-cpp::otlp_grpc_log_record_exporter
  )

  set_target_properties(nestdaq_fairlogger_otel PROPERTIES
    OUTPUT_NAME nestdaq_fairlogger_otel
  )

  install(TARGETS nestdaq_fairlogger_otel
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  )
endif()

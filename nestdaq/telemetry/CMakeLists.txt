install(FILES
  ${CMAKE_CURRENT_SOURCE_DIR}/FairLoggerTelemetryLoader.h
  ${CMAKE_CURRENT_SOURCE_DIR}/OpenTelemetryInitializer.h
  ${CMAKE_CURRENT_SOURCE_DIR}/Telemetry.h
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/nestdaq/telemetry
)

add_library(nestdaq_fairmq_throughput_parser STATIC
  FairMQThroughputLogParser.cxx
)

target_include_directories(nestdaq_fairmq_throughput_parser PUBLIC
  ${CMAKE_SOURCE_DIR}
)

set_target_properties(nestdaq_fairmq_throughput_parser PROPERTIES
  POSITION_INDEPENDENT_CODE ON
)

if(opentelemetry-cpp_FOUND)
  add_library(nestdaq_otel SHARED
    FairLoggerOpenTelemetrySink.cxx
    OpenTelemetryInitializer.cxx
  )

  target_include_directories(nestdaq_otel PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_BINARY_DIR}
  )

  target_include_directories(nestdaq_otel SYSTEM PRIVATE
    ${FairLogger_INCDIR}
  )

  target_link_directories(nestdaq_otel PRIVATE
    ${FairLogger_LIBDIR}
  )

  target_link_libraries(nestdaq_otel PRIVATE
    nestdaq_fairmq_throughput_parser
    FairLogger
    opentelemetry-cpp::logs
    opentelemetry-cpp::metrics
    opentelemetry-cpp::resources
    opentelemetry-cpp::trace
    opentelemetry-cpp::ostream_log_record_exporter
    opentelemetry-cpp::ostream_metrics_exporter
    opentelemetry-cpp::ostream_span_exporter
    opentelemetry-cpp::otlp_http_log_record_exporter
    opentelemetry-cpp::otlp_http_metric_exporter
    opentelemetry-cpp::otlp_http_exporter
    opentelemetry-cpp::otlp_grpc_log_record_exporter
    opentelemetry-cpp::otlp_grpc_metrics_exporter
    opentelemetry-cpp::otlp_grpc_exporter
  )

  set_target_properties(nestdaq_otel PROPERTIES
    OUTPUT_NAME nestdaq_otel
  )

  install(TARGETS nestdaq_otel
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  )
endif()

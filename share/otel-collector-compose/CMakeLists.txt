install(FILES
  docker-compose.yaml;
  opensearch_dashboards.yaml;
  otel-collector-config.yaml;
  DESTINATION ${CMAKE_INSTALL_DATADIR}/otel-collector-compose
)

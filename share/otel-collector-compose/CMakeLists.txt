install(FILES
  docker-compose.yaml;
  opensearch_dashboards.yaml;
  otel-collector-config.yaml;
  otel-collector-config-victoria.yaml;
  DESTINATION ${CMAKE_INSTALL_DATADIR}/otel-collector-compose
)

install(DIRECTORY
  grafana
  DESTINATION ${CMAKE_INSTALL_DATADIR}/otel-collector-compose
)

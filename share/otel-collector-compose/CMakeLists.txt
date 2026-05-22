install(FILES
  README.md
  docker-compose.yaml;
  opensearch_dashboards.yaml;
  otel-collector-config.yaml;
  otel-collector-config-victoria.yaml;
  DESTINATION ${CMAKE_INSTALL_DATADIR}/otel-collector-compose
)

install(DIRECTORY
  grafana
  opensearch-dashboards
  DESTINATION ${CMAKE_INSTALL_DATADIR}/otel-collector-compose
)

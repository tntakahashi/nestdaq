install(FILES
  README.md
  compose-opensearch.yaml;
  compose-victoria.yaml;
  compose-clickhouse.yaml;
  opensearch_dashboards.yaml;
  otel-collector-config-opensearch.yaml;
  otel-collector-config-victoria.yaml;
  otel-collector-config-clickhouse.yaml;
  DESTINATION ${CMAKE_INSTALL_DATADIR}/otel-collector-compose
)

install(DIRECTORY
  grafana-victoria
  grafana-clickhouse
  opensearch-dashboards
  DESTINATION ${CMAKE_INSTALL_DATADIR}/otel-collector-compose
)

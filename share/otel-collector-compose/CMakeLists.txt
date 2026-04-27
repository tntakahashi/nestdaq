install(FILES
  docker-compose.yaml;
  opensearch_dashboards.yaml;
  otel-collector-config.yaml;
  DESTINATION ${CMAKE_INSTALL_DATADIR}/otel-collector-compose
)

install(FILES
  README.md;
  DESTINATION ${CMAKE_INSTALL_DATADIR}/doc/otel-collector-compose
)
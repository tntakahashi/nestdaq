set(EX_DATADIR "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_DATADIR}")

configure_file(
  ${CMAKE_SOURCE_DIR}/share/otel-collector-compose/docker-compose.yaml.in
  ${CMAKE_BINARY_DIR}/share/otel-collector-compose/docker-compose.yaml
  @ONLY
)

install(FILES
  ${CMAKE_BINARY_DIR}/share/otel-collector-compose/docker-compose.yaml;
  opensearch_dashboards.yaml;
  otel-collector-config.yaml;
  DESTINATION ${CMAKE_INSTALL_DATADIR}/otel-collector-compose
)

install(FILES
  README.md;
  DESTINATION ${CMAKE_INSTALL_DATADIR}/doc/otel-collector-compose
)
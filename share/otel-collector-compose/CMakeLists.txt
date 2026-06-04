install(FILES
  README.md
  DESTINATION ${CMAKE_INSTALL_DATADIR}/otel-collector-compose
)

install(DIRECTORY
  opensearch
  victoria
  clickhouse
  DESTINATION ${CMAKE_INSTALL_DATADIR}/otel-collector-compose
)

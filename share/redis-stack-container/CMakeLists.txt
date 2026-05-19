install(FILES
  README.md
  DESTINATION ${CMAKE_INSTALL_DATADIR}/redis-stack-container
)

install(PROGRAMS
  run-redis-stack.sh
  run-redis-stack-server.sh
  DESTINATION ${CMAKE_INSTALL_DATADIR}/redis-stack-container
)

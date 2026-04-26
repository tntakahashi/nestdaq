# library version file
configure_file(
    ${CMAKE_SOURCE_DIR}/nestdaq/version.h.in
    ${CMAKE_BINARY_DIR}/nestdaq/version.h
    @ONLY
)

install(FILES
    ${CMAKE_BINARY_DIR}/nestdaq/version.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/nestdaq
)
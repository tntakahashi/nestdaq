# Files
install(FILES
  ${CMAKE_CURRENT_SOURCE_DIR}/create_skeleton_device.cmake; 
  ${CMAKE_CURRENT_SOURCE_DIR}/README.md;
  DESTINATION ${CMAKE_INSTALL_DATADIR}/new-device
)

install(FILES
  ${CMAKE_CURRENT_SOURCE_DIR}/skeleton/Skeleton.h;
  ${CMAKE_CURRENT_SOURCE_DIR}/skeleton/Skeleton.cxx;
  ${CMAKE_CURRENT_SOURCE_DIR}/skeleton/CMakeLists.txt;
  DESTINATION ${CMAKE_INSTALL_DATADIR}/new-device/skeleton
)

install(FILES
  ${CMAKE_CURRENT_SOURCE_DIR}/skeleton/cmake/setup.cmake;
  ${CMAKE_CURRENT_SOURCE_DIR}/skeleton/cmake/setupFairMQandBoost.cmake;
  ${CMAKE_CURRENT_SOURCE_DIR}/skeleton/cmake/setupROOT.cmake;
  DESTINATION ${CMAKE_INSTALL_DATADIR}/new-device/skeleton/cmake
)

# new-device command (shell script)
configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/new-device.in
  ${CMAKE_CURRENT_BINARY_DIR}/new-device
  @ONLY
) 


install(PROGRAMS
  ${CMAKE_CURRENT_BINARY_DIR}/new-device; 
  DESTINATION ${CMAKE_INSTALL_BINDIR} 
)
cmake_minimum_required(VERSION 3.11)

project(skeleton_project LANGUAGES CXX C)

message(STATUS "PROJECT_NAME: ${PROJECT_NAME}")

include(cmake/setup.cmake)
include(cmake/setupFairMQandBoost.cmake)
include(cmake/setupROOT.cmake)

#==============================================================================
foreach(lvar IN ITEMS Skeleton)

  set(EXEC ${lvar})
  add_executable(${EXEC} 
    ${EXEC}.cxx;
  )

  target_include_directories(${EXEC} PUBLIC 
    ${Boost_INCLUDE_DIRS};  
    ${FairMQ_INCDIR};
    ${CMAKE_SOURCE_DIR};
    ${fmt_INCDIR};
  )

  target_link_directories(${EXEC} PUBLIC
    ${Boost_LIBRARY_DIRS}; 
    ${FairMQ_LIBDIR};
  )

  target_link_libraries(${EXEC} PUBLIC 
    ${Boost_LIBRARIES};  
    ${FairMQ_LIBRARIES};  # defined in cmake/setupFairMQandBoost.cmake
  )

  install(TARGETS
    ${EXEC};
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  )
endforeach()

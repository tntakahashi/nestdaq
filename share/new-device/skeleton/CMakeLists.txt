cmake_minimum_required(VERSION 3.11)

project(nestdaq LANGUAGES CXX C)

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
#    ${FairLogger_LIBDIR};
  )

  if ((FairMQ_VERSION VERSION_GREATER_EQUAL 1.8.0)
    AND (FairMQ_VERSION VERSION_LESS_EQUAL 1.8.9))
    target_link_libraries(${EXEC} PUBLIC 
      ${Boost_LIBRARIES};  
      fairmq;
      FairLogger;
      stdc++fs;
    )
  elseif (((FairMQ_VERSION VERSION_GREATER_EQUAL 1.4.55)
    AND (FairMQ_VERSION VERSION_LESS_EQUAL 1.4.56))
    OR (FairMQ_VERSION VERSION_EQUAL 1.5.0))
    target_link_libraries(${EXEC} PUBLIC 
      ${Boost_LIBRARIES};  
      FairMQ;
      FairMQStateMachine;
      FairLogger;
      stdc++fs;
    )
  else()
    message(FATAL_ERROR "Unsupported FairMQ version ${FairMQ_VERSION}")
  endif()

  install(TARGETS
    ${EXEC};
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  )
endforeach()

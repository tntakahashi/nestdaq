include_guard(GLOBAL)

include(CMakeFindDependencyMacro)
find_dependency(Threads)

if(NOT TARGET NestDAQ::NestDAQ)
  add_library(NestDAQ::NestDAQ INTERFACE IMPORTED)

  set(_nestdaq_include_dir "${PACKAGE_PREFIX_DIR}/include")
  if(NOT EXISTS "${_nestdaq_include_dir}/nestdaq/runDevice.h")
    message(FATAL_ERROR "NestDAQ include directory does not contain nestdaq/runDevice.h: ${_nestdaq_include_dir}")
  endif()

  set(_nestdaq_link_libraries
    ${Boost_LIBRARIES}
    ${fmt_LIB}
    FairLogger
  )

  if(FairMQ_VERSION VERSION_GREATER_EQUAL 1.8.0)
    list(APPEND _nestdaq_link_libraries fairmq)
  elseif((FairMQ_VERSION VERSION_GREATER_EQUAL 1.4.55)
      AND (FairMQ_VERSION VERSION_LESS_EQUAL 1.4.56))
    list(APPEND _nestdaq_link_libraries FairMQ)
  else()
    message(FATAL_ERROR "Unsupported FairMQ version ${FairMQ_VERSION}")
  endif()

  if(CMAKE_DL_LIBS)
    list(APPEND _nestdaq_link_libraries ${CMAKE_DL_LIBS})
  endif()

  set_target_properties(NestDAQ::NestDAQ PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${_nestdaq_include_dir};${Boost_INCLUDE_DIRS};${FairLogger_INCDIR};${FairMQ_INCDIR}"
    INTERFACE_LINK_DIRECTORIES "${Boost_LIBRARY_DIRS};${FairLogger_LIBDIR};${FairMQ_LIBDIR}"
    INTERFACE_LINK_LIBRARIES "${_nestdaq_link_libraries}"
  )

  unset(_nestdaq_include_dir)
  unset(_nestdaq_link_libraries)
endif()

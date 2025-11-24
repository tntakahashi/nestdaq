# 1. Find the insalled Boost package using find_package()
# 2. If the package is not found, fetch and build Boost from GitHub by ExternalProject_Add()

message(STATUS "========== include Boost.cmake ==========")
set(Boost_GIT_TAG "boost-${Boost_VERSION}")


find_package(Boost) # ${Boost_VERSION} QUIET)

if(Boost_FOUND)
  message(STATUS "Found Boost")

else()
  message(STATUS "Boost not found. --- fetching from GitHub")

  if(DEFINED ENV{CMAKE_BUILD_PARALLEL_LEVEL})
      set(PARALLEL_JOBS $ENV{CMAKE_BUILD_PARALLEL_LEVEL})
  else()
      # fallback
      include(ProcessorCount)
      ProcessorCount(PARALLEL_JOBS)
  endif()

  include(ExternalProject)
  ExternalProject_Add(
      boost
      GIT_REPOSITORY         https://github.com/boostorg/boost.git
      GIT_TAG                ${Boost_GIT_TAG}

      UPDATE_COMMAND "" # skip update command 

      # - <SOURCE_DIR>
      # - <BINARY_DIR>
      # - <INSTALL_DIR>
      # These are the placeholders 

      CONFIGURE_COMMAND 
        ${CMAKE_COMMAND} -E chdir <SOURCE_DIR>
        ./bootstrap.sh --prefix=<INSTALL_DIR>

      BUILD_COMMAND
        ${CMAKE_COMMAND} -E chdir <SOURCE_DIR>
        ./b2 --prefix=<INSTALL_DIR>
             --build-dir=<BINARY_DIR>
             -j${PARALLEL_JOBS}
             link=shared
             threading=multi
             runtime-link=shared
             cxxstd=${CMAKE_CXX_STANDARD}

      INSTALL_COMMAND 
        ${CMAKE_COMMAND} -E chdir <SOURCE_DIR>
        ./b2 --prefix=<INSTALL_DIR>
             --build-dir=<BINARY_DIR>
             --j${PARALLEL_JOBS}
             install

      INSTALL_DIR ${CMAKE_INSTALL_PREFIX}
  )

  set(Boost_DIR ${CMAKE_INSTALL_PREFIX})
endif()


message(STATUS "========== include Boost.cmake done ==========")
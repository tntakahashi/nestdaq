# 1. Find the insalled Boost package using find_package()
# 2. If the package is not found, fetch and build Boost from GitHub by ExternalProject_Add()

message(STATUS "========== include Boost.cmake ==========")
set(Boost_GIT_TAG "boost-${Boost_VERSION}")


find_package(Boost) # ${Boost_VERSION} QUIET)

if(Boost_FOUND)
  message(STATUS "Found Boost")

else()
  message(STATUS "Boost not found. --- fetching from GitHub")

  if(CMAKE_SHARED_LINKER_FLAGS)
    set(MY_CXX_FLAGS    "cxxflags=${CMAKE_SHARED_LINKER_FLAGS}")
    set(MY_LINKER_FLAGS "linkflags=${CMAKE_SHARED_LINKER_FLAGS}")
  endif()

  include(ExternalProject)
  ExternalProject_Add(
      boost
      GIT_REPOSITORY         https://github.com/boostorg/boost.git
      GIT_TAG                ${Boost_GIT_TAG}

      UPDATE_COMMAND "" # skip update command 

      # The following variables are placeholders 
      # - <SOURCE_DIR>
      # - <BINARY_DIR>
      # - <INSTALL_DIR>

      CONFIGURE_COMMAND 
        ${CMAKE_COMMAND} -E chdir <SOURCE_DIR>
        ./bootstrap.sh --prefix=<INSTALL_DIR>

      BUILD_COMMAND
        ${CMAKE_COMMAND} -E chdir <SOURCE_DIR>
        ./b2 --prefix=<INSTALL_DIR>
             --build-dir=<BINARY_DIR>
             link=static,shared
             threading=multi
             runtime-link=static,shared
             variant=release
             cxxstd=${CMAKE_CXX_STANDARD}
             ${MY_CXX_FLAGS} ${MY_LINKER_FLAGS} ${BOOST_BUILD_PARALLEL_LEVEL}

      INSTALL_COMMAND 
        ${CMAKE_COMMAND} -E chdir <SOURCE_DIR>
        ./b2 --prefix=<INSTALL_DIR>
             --build-dir=<BINARY_DIR>
             install
             ${BOOST_BUILD_PARALLEL_LEVEL}

      INSTALL_DIR ${CMAKE_INSTALL_PREFIX}
  )
endif()


message(STATUS "========== include Boost.cmake done ==========")
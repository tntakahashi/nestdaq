# Installation {#nestdaq_installation}

## Install external packages

### Prerequisites for AlmaLinux 9 and 10

```bash
dnf -y update && \
dnf -y install \
    epel-release \
    dnf-plugins-core && \
dnf config-manager --set-enabled crb && \
dnf -y groupinstall "Development Tools" && \
dnf -y install \
    bash-completion \
    gcc \
    gcc-c++ \
    cmake \
    make \
    ninja-build \
    mold \
    git \
    autoconf \
    libtool \
    libcurl-devel \
    openssl-devel \
    gnutls-devel \
    zlib-devel \
    bzip2-devel \
    libzstd-devel \
    libquadmath-devel \
    libstdc++-static \
    python3-devel

# Optional tools:
# - jq: format and inspect JSON output from command-line tools.
# - clang-tools-extra: provide clang-tidy for static analysis.
# - doxygen: generate API documentation.
# - graphviz: provide the dot command for Doxygen diagrams.
# dnf -y install jq clang-tools-extra doxygen graphviz

# If needed for AlmaLinux 9
# dnf -y install gcc-toolset-14
```

### Build and install external dependencies
The following command installs ZeroMQ, Boost, FairLogger, FairMQ, Catch2, hiredis, redis++, and Redis Stack.

```bash
# download the source code
git clone https://github.com/spadi-alliance/nestdaq

# configure
cmake \
  -DCMAKE_INSTALL_PREFIX=./install \
  -DBUILD_PARALLEL_LEVEL=$(nproc) \
  -B ./build-external \
  -S nestdaq/cmake

# Both the build and install steps are executed
cmake --build ./build-external
```

- In the command example above, CMake’s `ExternalProject` is used to perform `git clone`, build, and install.
  - In this case, the `--parallel` (or `-j`) option passed to cmake --build does not control the inner ExternalProject builds, so please specify the parallel build level during the initial configuration using `-DBUILD_PARALLEL_LEVEL=xxx`.
    - The `nproc` command prints the number of available CPU cores on the system. If this causes excessive memory usage, specify a smaller value manually.
- The default dependency versions are listed below. To override a version, pass `-Dxxxx_VERSION=yyyy` to CMake.
- If `-DWITH_REDIS_STACK=OFF` is specified, the external dependency build does not build or install Redis Stack. The default is `WITH_REDIS_STACK=ON`.
- If `-DWITH_OTEL_CPP=ON` is specified, the external dependency build also installs opentelemetry-cpp and its dependencies, such as nlohmann/json and gRPC. The default is `WITH_OTEL_CPP=OFF`.
- If Doxygen is found during the external dependency configure step, `doxygen-awesome-css` is installed as an optional documentation asset under `./install/share/doxygen-awesome-css`.
- To use Ninja instead of Make, add `-G Ninja` to the CMake options.
- To use `mold` instead of the system `ld`.
  - GCC 12.1 or later: Add `-DCMAKE_EXE_LINKER_FLAGS="-fuse-ld=mold"` and `-DCMAKE_SHARED_LINKER_FLAGS="-fuse-ld=mold"` to the CMake options
  - GCC 12.0 or earlier: Add `-DCMAKE_EXE_LINKER_FLAGS="-B<path-to-mold>"` and `-DCMAKE_SHARED_LINKER_FLAGS="-B<path-to-mold>"`

#### Versions of installed external dependencies

| Package                                                                  | Version (default) | CMake options to modify versions |
| :--                                                                      | :--               | :--                              |
| [ZeroMQ(libzmq)](https://github.com/zeromq/libzmq)                       | 4.3.5             | `ZeroMQ_VERSION`                 |
| [Boost](https://github.com/boostorg/boost)                               | 1.85.0            | `Boost_VERSION`                  | 
| [FairLogger](https://github.com/FairRootGroup/FairLogger)                | 2.3.0             | `FairLogger_VERSION`             |
| [FairMQ](https://github.com/FairRootGroup/FairMQ)                        | 1.10.0            | `FairMQ_VERSION`                 |
| [Catch2](https://github.com/catchorg/Catch2)                             | 3.14.0            | `Catch2_VERSION`                 |
| [hiredis](https://github.com/redis/hiredis)                              | 1.3.0             | `hiredis_VERSION`                |
| [redis++](https://github.com/sewenew/redis-plus-plus)                    | 1.3.15            | `redis_plus_plus_VERSION`        |
| [opentelemetry-cpp](https://github.com/open-telemetry/opentelemetry-cpp) | 1.24.0            | `opentelemetry-cpp_VERSION`      |
| [doxygen-awesome-css](https://github.com/jothepro/doxygen-awesome-css)   | 2.4.2             | `doxygen-awesome-css_VERSION`    |

##### External runtime components
Redis Stack (`redis-server`, `redis-cli`, Redis modules, etc.) is included in the external packages and is built and installed together with them. It is required by the NestDAQ application at runtime, but it is not a direct library dependency.

| Package                                                                  | Version (default) | CMake options to modify versions |
| :--                                                                      | :--               | :--                              |
| [Redis](https://github.com/redis/redis)                                  | 8.6.2             | `Redis_VERSION`                  |
| [RedisBloom](https://github.com/RedisBloom/RedisBloom)                   | 2.8.17            | `RedisBloom_VERSION`             |
| [RediSearch](https://github.com/RediSearch/RediSearch)                   | 2.10.25           | `RediSearch_VERSION`             |
| [RedisJSON](https://github.com/RedisJSON/RedisJSON)                      | 2.8.16            | `RedisJSON_VERSION`              |
| [RedisTimeSeries](https://github.com/RedisTimeSeries/RedisTimeSeries)    | 1.12.9            | `RedisTimeSeries_VERSION`        |


### Build and install NestDAQ library
```bash
cmake \
  -DCMAKE_PREFIX_PATH=./install \
  -DCMAKE_INSTALL_PREFIX=./install \
  -B ./build \
  -S nestdaq
cmake --build ./build --parallel $(nproc)
cmake --install ./build
```

- In the example above, both the main NestDAQ package and the external dependencies are installed in the same directory (`./install`).
  If the external dependencies are installed in a different location, specify that directory with `-DCMAKE_PREFIX_PATH=xxx`.
- To run `clang-tidy` during the NestDAQ build, add `-DNESTDAQ_ENABLE_CLANG_TIDY=ON`.
  This requires the `clang-tidy` command, provided by `clang-tools-extra` on AlmaLinux.
- To build and install Doxygen documentation, add `-DWITH_DOCS=ON`.
  This requires the `doxygen` command. If Doxygen is not found, documentation generation is skipped. If `dot` from Graphviz is available, Doxygen can use it to generate diagrams.
- The Doxygen HTML output uses `doxygen-awesome-css` from `CMAKE_PREFIX_PATH/share/doxygen-awesome-css` by default. To use another location, specify `-DNESTDAQ_DOXYGEN_AWESOME_DIR=/path/to/doxygen-awesome-css`.
- When `doxygen-awesome-css` is available, it is installed with the generated documentation under `./install/share/doc/nestdaq/doxygen-awesome-css`.
- When `-DWITH_DOCS=ON` and Doxygen is available, the HTML documentation is generated under `./build/docs/html` and installed under `./install/share/doc/nestdaq/html`.

### Build and install examples

The examples are built as a separate CMake project. Build and install the main
NestDAQ package first, then configure the examples with `find_package(NestDAQ)`
using the NestDAQ install prefix.

```bash
cmake \
  -DCMAKE_PREFIX_PATH=./install \
  -DCMAKE_INSTALL_PREFIX=./install \
  -B ./build-examples \
  -S nestdaq/examples
cmake --build ./build-examples --parallel $(nproc)
cmake --install ./build-examples
```

- `-DCMAKE_PREFIX_PATH=./install` must point to the directory where NestDAQ was installed.
- The installed example binaries are placed under `./install/bin`.

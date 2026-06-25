# Installation

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
# - clang-tools-extra: provide clang-tidy, clang-format, and related Clang tools.
# - doxygen: generate API documentation.
# - graphviz: provide the dot command for Doxygen diagrams.
# - astyle: format C/C++ source when needed.
# - tmux: keep long-running local validation sessions attached.
# dnf -y install jq clang-tools-extra doxygen graphviz astyle tmux

# If needed for AlmaLinux 9
# dnf -y install gcc-toolset-14
```

For AlmaLinux 8, enable the `powertools` repository instead of `crb` before
installing GCC Toolset packages:

```bash
dnf config-manager --set-enabled powertools
dnf -y install gcc-toolset-14
```

### Prerequisites for Debian 13 and Ubuntu 26.04

```bash
apt update && \
apt install -y \
    bash-completion \
    build-essential \
    ca-certificates \
    cmake \
    curl \
    make \
    ninja-build \
    mold \
    git \
    autoconf \
    libtool \
    libcurl4-openssl-dev \
    libssl-dev \
    libgnutls28-dev \
    zlib1g-dev \
    libz2-dev \
    libzstd-dev \
    python3 \
    python3-dev

# Optional tools:
# - jq: format and inspect JSON output from command-line tools.
# - clang-tools: provide clang-tidy and related LLVM/Clang tools.
# - clang-format: provide clang-format, packaged separately from clang-tools on Debian and Ubuntu.
# - doxygen: generate API documentation.
# - graphviz: provide the dot command for Doxygen diagrams.
# - astyle: format C/C++ source when needed.
# - tmux: keep long-running local validation sessions attached.
# apt install -y jq clang-tools clang-format doxygen graphviz astyle tmux
```

### Build and install external dependencies
The following command installs ZeroMQ, Boost, FairLogger, FairMQ, Catch2,
nlohmann/json, hiredis, redis++, and Redis Stack.

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

Redis Stack is a runtime service, not a direct library dependency. If you prefer
to run Redis Stack in a container instead of building and installing it here,
add `-DWITH_REDIS_STACK=OFF` to the external dependency configure command and
start Redis Stack separately. Container helper scripts and runtime notes are in
[`share/redis-stack-container/README.md`](share/redis-stack-container/README.md).

- In the command example above, CMake’s `ExternalProject` is used to perform `git clone`, build, and install.
  - In this case, the `--parallel` (or `-j`) option passed to cmake --build does not control the inner ExternalProject builds, so please specify the parallel build level during the initial configuration using `-DBUILD_PARALLEL_LEVEL=xxx`.
    - The `nproc` command prints the number of available CPU cores on the system. If this causes excessive memory usage, specify a smaller value manually.
- The default dependency versions are listed below. To override a version, pass `-Dxxxx_VERSION=yyyy` to CMake.
- If Doxygen is found during the external dependency configure step, `doxygen-awesome-css` is installed as an optional documentation asset under `./install/share/doxygen-awesome-css`.
- To use Ninja instead of Make, add `-G Ninja` to the CMake options.
- To use `mold` instead of the system `ld`.
  - GCC 12.1 or later: Add `-DCMAKE_EXE_LINKER_FLAGS="-fuse-ld=mold"` and `-DCMAKE_SHARED_LINKER_FLAGS="-fuse-ld=mold"` to the CMake options
  - GCC 12.0 or earlier: Add `-DCMAKE_EXE_LINKER_FLAGS="-B<path-to-mold>"` and `-DCMAKE_SHARED_LINKER_FLAGS="-B<path-to-mold>"`

#### External dependency build options

| Option | Default | Description |
| :-- | :-- | :-- |
| `BUILD_PARALLEL_LEVEL` | unset | Parallel level passed to inner `ExternalProject` builds. Set this at configure time; `cmake --build --parallel` does not control those inner builds. |
| `WITH_REDIS_STACK` | `ON` | Build and install Redis Stack runtime components. Set to `OFF` when Redis Stack is provided separately, for example by a container. |
| `WITH_SPDLOG` | `OFF` | Build and install spdlog. Enable this when building the optional NestDAQ spdlog OpenTelemetry sink. |
| `WITH_OTEL_CPP` | `OFF` | Build and install opentelemetry-cpp and optional transport dependencies such as gRPC. |
| `<package>_VERSION` | package-specific | Override the dependency version listed below, for example `-DFairMQ_VERSION=...`. |

The default `FairMQ_VERSION` depends on the GNU compiler version. GCC 9.1 or
later uses FairMQ 1.10.0 by default; older GCC releases use FairMQ 1.9.2. Pass
`-DFairMQ_VERSION=...` to override this selection explicitly.

Redis Stack also exposes low-level cache variables such as Redis build TLS,
allocator, and temporary Rust toolchain paths. These are intended for dependency
build maintenance; inspect the CMake cache or `cmake/dependencies/redis-stack.cmake`
when those knobs are needed.

#### Versions of installed external dependencies

| Package                                                                  | Version (default) | CMake options to modify versions |
| :--                                                                      | :--               | :--                              |
| [ZeroMQ(libzmq)](https://github.com/zeromq/libzmq)                       | 4.3.5             | `ZeroMQ_VERSION`                 |
| [Boost](https://github.com/boostorg/boost)                               | 1.85.0            | `Boost_VERSION`                  | 
| [FairLogger](https://github.com/FairRootGroup/FairLogger)                | 2.3.0             | `FairLogger_VERSION`             |
| [FairMQ](https://github.com/FairRootGroup/FairMQ)                        | 1.10.0 with GCC 9.1 or later; 1.9.2 with older GCC | `FairMQ_VERSION` |
| [Catch2](https://github.com/catchorg/Catch2)                             | 3.14.0            | `Catch2_VERSION`                 |
| [nlohmann/json](https://github.com/nlohmann/json)                        | 3.12.0            | `nlohmann_json_VERSION`          |
| [spdlog](https://github.com/gabime/spdlog)                                | 1.17.0            | `spdlog_VERSION`                 |
| [hiredis](https://github.com/redis/hiredis)                              | 1.3.0             | `hiredis_VERSION`                |
| [redis++](https://github.com/sewenew/redis-plus-plus)                    | 1.3.15            | `redis_plus_plus_VERSION`        |
| [opentelemetry-cpp](https://github.com/open-telemetry/opentelemetry-cpp) | 1.26.0            | `opentelemetry-cpp_VERSION`      |
| [doxygen-awesome-css](https://github.com/jothepro/doxygen-awesome-css)   | 2.4.2             | `doxygen-awesome-css_VERSION`    |

##### External runtime components
Redis Stack (`redis-server`, `redis-cli`, Redis modules, etc.) is included in
the external packages and is built and installed together with them by default.
It is required by the NestDAQ application at runtime, but it is not a direct
library dependency. It may also be run in a container instead; see
[`share/redis-stack-container/README.md`](share/redis-stack-container/README.md).

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
- When `doxygen-awesome-css` is available, it is installed with the generated documentation under `./install/share/doc/nestdaq/doxygen-awesome-css`.
- When `-DNestDAQ_BUILD_DOCS=ON` and Doxygen is available, the HTML documentation is generated under `./build/docs/html` and installed under `./install/share/doc/nestdaq/html`.

#### NestDAQ build options

| Option | Default | Description |
| :-- | :-- | :-- |
| `NESTDAQ_ENABLE_CLANG_TIDY` | `OFF` | Run `clang-tidy` during the NestDAQ build. This requires the `clang-tidy` command, provided by `clang-tools-extra` on AlmaLinux. |
| `NestDAQ_BUILD_DOCS` | `OFF` | Build and install Doxygen documentation. This requires `doxygen`; if Doxygen is not found, documentation generation is skipped. If `dot` from Graphviz is available, Doxygen can use it to generate diagrams. |
| `NestDAQ_BUILD_EXAMPLES` | `ON` | Build and install `Sampler`, `Sink`, and `NullDevice` with the main NestDAQ build. Set this to `OFF` to skip them. |
| `NESTDAQ_DOXYGEN_AWESOME_DIR` | discovered from `CMAKE_PREFIX_PATH` or install prefix | Directory containing `doxygen-awesome-css` assets used by generated documentation. |
| `BUILD_TESTING` | `ON` | Build NestDAQ tests when enabled. |

### Run local OpenTelemetry Collector and backend containers

NestDAQ can export OpenTelemetry logs, metrics, and traces to an OpenTelemetry
Collector. The repository provides optional Compose setups for local validation
under [`share/otel-collector-compose/`](share/otel-collector-compose/README.md).
They are runtime tools, not build dependencies, and are not intended for
production deployment as-is.

After installing NestDAQ, copy the installed setup to a working directory and
start one backend stack:

```bash
cp -a <install-prefix>/share/otel-collector-compose ./otel-collector-compose
cd ./otel-collector-compose/opensearch
docker compose -f compose-opensearch.yaml up
```

For Podman, use the same Compose files with `podman compose`.

Available local backend setups are:

- [`opensearch/`](share/otel-collector-compose/opensearch/README.md): logs and
  traces in OpenSearch, viewed with OpenSearch Dashboards.
- [`victoria/`](share/otel-collector-compose/victoria/README.md): logs,
  metrics, and traces in VictoriaLogs, VictoriaMetrics, and VictoriaTraces,
  viewed with Grafana.
- [`clickhouse/`](share/otel-collector-compose/clickhouse/README.md): logs,
  metrics, and traces in ClickStack/ClickHouse, viewed with the ClickStack user
  interface (UI).

By default, the Compose stacks expose OpenTelemetry Protocol (OTLP) gRPC on
`localhost:4317` and OTLP HTTP on `localhost:4318`. See
[`share/otel-collector-compose/README.md`](share/otel-collector-compose/README.md)
and the backend-specific README files for ports, volumes, credentials,
SELinux, and rootless Podman notes.

### Build and install examples

The examples are included in the main NestDAQ build by default. They can also be
built as a separate CMake project after installing NestDAQ. For a separate
examples build, configure the examples with `find_package(NestDAQ)` using the
NestDAQ install prefix.

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

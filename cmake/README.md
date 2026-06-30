# CMake Support Files

This directory contains CMake files used by the NestDAQ build, the installed
`find_package(NestDAQ)` package, and the separate external dependency build
project. For build commands, dependency versions, and user-facing options, see
[`INSTALL.md`](../INSTALL.md).

## Top-Level Build Helpers

| File | Purpose |
| :-- | :-- |
| `common.cmake` | Common build settings shared by the main project and dependency project: C++ standard checks, warning flags, install directories, `Threads`, and CMake compatibility arguments for `ExternalProject_Add`. |
| `NestDAQBuildSettings.cmake` | Helper functions for optional `clang-tidy` integration and install RPATH setup. |
| `GitHelper.cmake` | Reads Git tag, commit, branch, dirty-state, and remote metadata used to derive the project version. |
| `NestDAQExamplesStandalone.cmake` | Shared setup used when `examples/` is configured as its own standalone CMake project. |
| `PatchDoxygenAwesomeCssRefs.cmake` | Post-processes generated Doxygen HTML so pages refer to the installed `doxygen-awesome-css` asset path. |

## Installed Package Files

These files are installed with NestDAQ and are used by downstream projects that
call `find_package(NestDAQ REQUIRED CONFIG)`.

| File | Purpose |
| :-- | :-- |
| `NestDAQConfig.cmake.in` | Template for the installed package config. It adjusts `CMAKE_PREFIX_PATH`, finds FairMQ and its dependencies, and includes the version and target files. |
| `NestDAQVersion.cmake.in` | Template for installed NestDAQ version and Git metadata variables. |
| `NestDAQTargets.cmake` | Defines the imported `NestDAQ::NestDAQ` interface target and its include directories, link directories, and link libraries. |

## External Dependency Project

`cmake/CMakeLists.txt` is a standalone project for building and installing the
external dependencies used by NestDAQ. It includes files from
`cmake/dependencies/`, mostly using CMake `ExternalProject_Add`; Redis Stack
also uses `FetchContent` to materialize module source trees.

| File | Purpose |
| :-- | :-- |
| `dependencies/Boost.cmake` | Finds or builds Boost. |
| `dependencies/ZeroMQ.cmake` | Finds or builds ZeroMQ. |
| `dependencies/FairLogger.cmake` | Finds or builds FairLogger. |
| `dependencies/FairMQ.cmake` | Finds or builds FairMQ. |
| `dependencies/Catch2.cmake` | Finds or builds Catch2 for tests. |
| `dependencies/nlohmann_json.cmake` | Finds or builds nlohmann/json. |
| `dependencies/hiredis.cmake` | Finds or builds hiredis. |
| `dependencies/redis_plus_plus.cmake` | Finds or builds redis-plus-plus. |
| `dependencies/opentelemetry-cpp.cmake` | Builds opentelemetry-cpp and its optional transport dependencies. |
| `dependencies/spdlog.cmake` | Builds spdlog. For C++17 dependency builds it also pulls in `fmt` through `dependencies/fmt.cmake`. |
| `dependencies/fmt.cmake` | Builds fmt when required by spdlog or selected explicitly by dependency options. |
| `dependencies/redis-stack.cmake` | Builds Redis Stack components for Redis 8 or later: Redis, RedisBloom, RediSearch, RedisJSON, and RedisTimeSeries. The default Redis 8.2.7 module versions follow the release tags selected by Redis 8.2.7 itself. |
| `dependencies/redis-server-7.cmake` | Builds Redis 7.x server with standalone RedisTimeSeries. Redis 7.4 uses Redis 7.4.9 and RedisTimeSeries 1.12.14 by default; Redis 7.2 uses Redis 7.2.14 and RedisTimeSeries 1.10.24 by default. |
| `dependencies/doxygen-awesome-css.cmake` | Builds or installs the doxygen-awesome-css assets used by generated documentation. |
| `dependencies/patch_redisearch.cmake` | Applies local RediSearch CMake compatibility patches during the Redis Stack dependency build. |
| `dependencies/patch_redisjson.cmake` | Applies local RedisJSON build compatibility patches during the Redis Stack dependency build. |
| `dependencies/build_redis-stack_with_temp_rust.sh` | Wrapper used by the Redis Stack build to provide a temporary Rust toolchain environment when needed. |
| `dependencies/build_redis-server-7_with_redistimeseries.sh` | Wrapper used by the Redis 7.x build path. It builds RedisTimeSeries as a standalone module instead of copying module sources into the Redis source tree. |

The default dependency versions and options such as `WITH_REDIS_STACK`,
`WITH_REDIS_SERVER_7`, `WITH_OTEL_CPP`, `WITH_SPDLOG`, and
`BUILD_PARALLEL_LEVEL` are documented in [`INSTALL.md`](../INSTALL.md).

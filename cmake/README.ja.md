# CMakeサポートファイル

[English](README.md) | [日本語](README.ja.md)

このディレクトリには、NestDAQのビルド、インストール済みの
`find_package(NestDAQ)` package、および独立した外部依存関係ビルド
プロジェクトで使用するCMake fileがあります。ビルドコマンド、依存関係の
バージョン、user向けoptionについては
[`INSTALL.ja.md`](../INSTALL.ja.md)を参照してください。

<a id="1-top-level-build-helpers"></a>
## 1. トップレベルビルドヘルパー

| ファイル | 用途 |
| :-- | :-- |
| `common.cmake` | main projectとdependency projectで共有する共通ビルド設定。C++ standard check、warning flag、install directory、`Threads`、および`ExternalProject_Add`用CMake互換引数を設定します。 |
| `NestDAQBuildSettings.cmake` | 必要に応じて有効にできる`clang-tidy`連携とinstall RPATH設定のhelper function。 |
| `GitHelper.cmake` | project versionの導出に使用するGit tag、commit、branch、dirty state、remote metadataを読み取ります。 |
| `NestDAQExamplesStandalone.cmake` | `examples/`を独立したCMake projectとしてconfigureするときに使用する共通設定。 |
| `PatchDoxygenAwesomeCssRefs.cmake` | 生成されたDoxygen HTMLを後処理し、pageがインストール済みの`doxygen-awesome-css` asset pathを参照するようにします。 |

<a id="2-installed-package-files"></a>
## 2. インストールされるパッケージファイル

これらのfileはNestDAQとともにインストールされ、
`find_package(NestDAQ REQUIRED CONFIG)`を呼び出すdownstream projectで使用されます。

| ファイル | 用途 |
| :-- | :-- |
| `NestDAQConfig.cmake.in` | インストール済みpackage configのtemplate。`CMAKE_PREFIX_PATH`を調整し、FairMQとその依存関係を検索して、version fileとtarget fileをincludeします。 |
| `NestDAQVersion.cmake.in` | インストール済みNestDAQ versionおよびGit metadata variableのtemplate。 |
| `NestDAQTargets.cmake` | imported target `NestDAQ::NestDAQ`と、そのinclude directory、link directory、link libraryを定義します。 |

<a id="3-external-dependency-project"></a>
## 3. 外部依存関係プロジェクト

`cmake/CMakeLists.txt`は、NestDAQが使用する外部依存関係をビルド・
インストールする独立したprojectです。主にCMakeの`ExternalProject_Add`を
使用して`cmake/dependencies/`内のfileをincludeします。Redis Stackでは
module source treeを展開するために`FetchContent`も使用します。

| ファイル | 用途 |
| :-- | :-- |
| `dependencies/Boost.cmake` | Boostを検索またはビルドします。 |
| `dependencies/ZeroMQ.cmake` | ZeroMQを検索またはビルドします。 |
| `dependencies/FairLogger.cmake` | FairLoggerを検索またはビルドします。 |
| `dependencies/FairMQ.cmake` | FairMQを検索またはビルドします。 |
| `dependencies/Catch2.cmake` | test用Catch2を検索またはビルドします。 |
| `dependencies/nlohmann_json.cmake` | nlohmann/jsonを検索またはビルドします。 |
| `dependencies/hiredis.cmake` | hiredisを検索またはビルドします。 |
| `dependencies/redis_plus_plus.cmake` | redis-plus-plusを検索またはビルドします。 |
| `dependencies/opentelemetry-cpp.cmake` | opentelemetry-cppと、選択した機能に応じたtransport dependencyをビルドします。 |
| `dependencies/spdlog.cmake` | spdlogをビルドします。C++17 dependency buildでは、`dependencies/fmt.cmake`を通じて`fmt`も取得します。 |
| `dependencies/fmt.cmake` | spdlogで必要な場合、またはdependency optionで明示的に選択した場合にfmtをビルドします。 |
| `dependencies/redis-stack.cmake` | Redis 8以降向けRedis Stack component(Redis、RedisBloom、RediSearch、RedisJSON、RedisTimeSeries)をビルドします。defaultのRedis 8.2.7 module versionはRedis 8.2.7自身が選択しているrelease tagに従います。 |
| `dependencies/redis-server-7.cmake` | standalone RedisTimeSeriesとともにRedis 7.x serverをビルドします。defaultでは、Redis 7.4はRedis 7.4.9とRedisTimeSeries 1.12.14を使用し、Redis 7.2はRedis 7.2.14とRedisTimeSeries 1.10.24を使用します。 |
| `dependencies/doxygen-awesome-css.cmake` | 生成ドキュメントで使用するdoxygen-awesome-css assetをビルドまたはインストールします。 |
| `dependencies/patch_redisearch.cmake` | Redis Stack dependency build中に、ローカルのRediSearch CMake互換patchを適用します。 |
| `dependencies/patch_redisjson.cmake` | Redis Stack dependency build中に、ローカルのRedisJSON build互換patchを適用します。 |
| `dependencies/build_redis-stack_with_temp_rust.sh` | 必要な場合に一時的なRust toolchain環境を提供する、Redis Stack build用wrapper。 |
| `dependencies/build_redis-server-7_with_redistimeseries.sh` | Redis 7.x build pathで使用するwrapper。module sourceをRedis source treeにcopyせず、RedisTimeSeriesをstandalone moduleとしてビルドします。 |

defaultのdependency versionと、`WITH_REDIS_STACK`、
`WITH_REDIS_SERVER_7`、`WITH_OTEL_CPP`、`WITH_SPDLOG`、
`BUILD_PARALLEL_LEVEL`などのoptionについては
[`INSTALL.ja.md`](../INSTALL.ja.md)に記載されています。

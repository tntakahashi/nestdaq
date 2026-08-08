# NestDAQ

[English](README.md) | [日本語](README.ja.md)

[次へ: インストール](INSTALL.ja.md)

NestDAQは、粒子線計測向けのストリーミングデータ収集(DAQ)アプリケーションを構築するためのフレームワークです。
このリポジトリのコードをビルドしてインストールすると、共通機能とツールを利用できますが、それだけでは実際の回路に接続して動作するDAQアプリケーションにはなりません。
計測対象や回路に対応するDAQアプリケーションコードは、このリポジトリの提供範囲外です。

<a id="1-project-guide"></a>
## 1. プロジェクトガイド

| パス / ドキュメント | 用途 |
| :-- | :-- |
| [INSTALL.ja.md](INSTALL.ja.md) | 前提条件、依存関係のversionとビルドオプション、NestDAQのビルドオプション、外部サービス、example、ドキュメント生成。 |
| [examples/](examples/README.ja.md) | `Sampler`、`Sink`、`NullDevice`などのdevice例、詳細なローカル実行手順、[カスタムuser deviceの作成方法](examples/README.ja.md#4-creating-your-own-user-device)。 |
| [scripts/](scripts/README.ja.md) | プロセス起動およびtopology用の補助スクリプトと、device skeleton generator `generate-device-skeleton.py`。 |
| [controller/](controller/README.ja.md) | `daq-webctl` HTTP/WebSocket server、Redis制御、telemetry設定。 |
| [share/controller/](share/controller/README.ja.md) | `daq-webctl`が配信するWebブラウザー向けファイル。 |
| `nestdaq/` | version headerのtemplate、FairMQ device applicationのentry pointと`main()`を提供する`runDevice.h`、telemetry codeなど、NestDAQのcore headerとsource code。 |
| [nestdaq/telemetry/](nestdaq/telemetry/README.ja.md) | 必要に応じて有効にできるOpenTelemetry連携。 |
| [plugins/](plugins/README.ja.md) | DAQ service、metrics、parameter設定用のFairMQ plugin。 |
| [cmake/](cmake/README.ja.md) | CMake helper、インストールされるpackage file、外部依存関係をビルドするproject。 |
| [tests/](tests/) | C++ testとtest用の補助ファイル。 |
| `share/` | NestDAQとともにインストールされる設定ファイルと補助ファイル。 |
| [share/otel-collector-compose/](share/otel-collector-compose/README.ja.md) | `docker compose`または`podman compose`で実行するローカルOpenTelemetry Collectorと、[OpenSearch](share/otel-collector-compose/opensearch/README.ja.md)、[Victoria](share/otel-collector-compose/victoria/README.ja.md)、[ClickStack](share/otel-collector-compose/clickhouse/README.ja.md)用backend Compose stack。 |
| [share/redis-stack-container/](share/redis-stack-container/README.ja.md) | [Redis Stack](INSTALL.ja.md#redis-server-and-modules) containerを実行する補助スクリプト。 |
| [share/installers/](share/installers/README.ja.md) | 外部サービスをhostへインストールする`apt`および`dnf`用補助スクリプト。 |
| [CONTRIBUTING.ja.md](CONTRIBUTING.ja.md) | ブランチ運用方針、contribution workflow、formatting、static analysis、naming rule。 |

<a id="2-tested-systems"></a>
## 2. 検証済みシステム
| Distribution | バージョン | Compiler    | CMake  | FairMQ |
| ---       | ---     | ---         | ---    | ---    |
| AlmaLinux | 8.10    | GCC 8.5.0   | 3.26.5 | 1.9.2  |
| AlmaLinux | 9.8     | GCC 11.5.0  | 3.31.8 | 1.10.0 |
| AlmaLinux | 10.2    | GCC 14.3.1  | 3.31.8 | 1.10.0 |
| Debian    | 12      | GCC 12.2.0  | 3.25.1 | 1.10.0 |
| Debian    | 13      | GCC 14.2.0  | 3.31.6 | 1.10.0 |
| Ubuntu    | 22.04   | GCC 11.4.0  | 3.22.1 | 1.10.0 |
| Ubuntu    | 24.04   | GCC 13.3.0  | 3.28.3 | 1.10.0 |
| Ubuntu    | 26.04   | GCC 15.2.0  | 4.2.3  | 1.10.0 |

<a id="3-dependencies"></a>
## 3. 依存関係

NestDAQはBoost、FairLogger、FairMQ、hiredis、redis-plus-plusを使用します。
対応する機能を有効にした場合は、opentelemetry-cppやspdlogなどのtelemetryおよびlogging用の依存関係も使用します。
前提package、依存関係のdefault version、CMake override option、アプリケーションの実行時に使用する外部サービス、platform固有のビルド上の制約については、[INSTALL.ja.md](INSTALL.ja.md)を参照してください。

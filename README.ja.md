# NestDAQ

[English](README.md) | [日本語](README.ja.md)

素粒子計測向けストリーミングデータ収集（DAQ）実装

<a id="1-project-guide"></a>
## 1. プロジェクトガイド

| パス / ドキュメント | 用途 |
| :-- | :-- |
| [INSTALL.md](INSTALL.md) | 前提条件、外部依存関係のバージョンとビルドオプション、NestDAQのビルドオプション、ランタイムサービスの選択肢、example、任意のドキュメント生成。 |
| [examples/](examples/README.ja.md) | `Sampler`、`Sink`、`NullDevice`などのexample device。詳細なローカル実行手順と[カスタムuser deviceの作成](examples/README.ja.md#4-creating-your-own-user-device)についてはREADMEを参照してください。 |
| [scripts/](scripts/README.ja.md) | ランタイム起動・topology補助スクリプトと、device skeleton generator `generate-device-skeleton.py`。 |
| [controller/](controller/README.ja.md) | `daq-webctl` HTTP/WebSocket server、Redis制御、telemetry設定。 |
| [share/controller/](share/controller/README.ja.md) | `daq-webctl`が配信するブラウザasset。 |
| `nestdaq/` | NestDAQの公開headerとランタイムhelper。 |
| [nestdaq/telemetry/](nestdaq/telemetry/README.ja.md) | 任意のOpenTelemetry連携。 |
| [plugins/](plugins/README.ja.md) | DAQ service、metrics、parameter設定用のFairMQ plugin。 |
| [cmake/](cmake/README.ja.md) | CMake helper、インストールされるpackage file、外部依存関係のビルドプロジェクト。 |
| [tests/](tests/) | C++ testとtest support file。 |
| `share/` | NestDAQとともにインストールされるランタイム・設定asset。 |
| [share/otel-collector-compose/](share/otel-collector-compose/README.ja.md) | ローカルOpenTelemetry Collectorと、[OpenSearch](share/otel-collector-compose/opensearch/README.ja.md)、[Victoria](share/otel-collector-compose/victoria/README.ja.md)、[ClickStack](share/otel-collector-compose/clickhouse/README.ja.md)用backend Compose stack。 |
| [share/redis-stack-container/](share/redis-stack-container/README.ja.md) | Redis Stack container補助スクリプト。 |
| [share/installers/](share/installers/README.ja.md) | `apt`または`dnf`を使用するランタイムサービス用host package installer補助スクリプト。 |
| [CONTRIBUTING.md](CONTRIBUTING.md) | contribution workflow、formatting、static analysis、naming rule。 |

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

NestDAQはBoost、FairLogger、FairMQ、hiredis、redis-plus-plus、および
opentelemetry-cppやspdlogなどの任意のtelemetry/logging依存関係を使用します。
前提package、依存関係のdefault version、CMake override option、
ランタイムサービスの選択肢、platform固有のビルド上の注意事項については
[INSTALL.md](INSTALL.md)を参照してください。

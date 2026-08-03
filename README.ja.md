# NestDAQ

[English](README.md) | [日本語](README.ja.md)

NestDAQは、粒子線計測向けのストリーミングデータ収集(DAQ)アプリケーションを
構築するためのフレームワークです。このリポジトリのコードをビルド・インストールすると
共通機能とツールを利用できますが、それだけでは実際の回路に接続して動作するDAQ
アプリケーションにはなりません。計測対象や回路に対応するDAQアプリケーションコードは、
このリポジトリの提供範囲外です。

<a id="1-project-guide"></a>
## 1. プロジェクトガイド

| パス / ドキュメント | 用途 |
| :-- | :-- |
| [INSTALL.ja.md](INSTALL.ja.md) | この文書では、前提条件、依存関係のバージョンとビルドオプション、NestDAQのビルドオプション、実行時に使用する外部サービスの選択肢、example、ドキュメント生成について説明します。 |
| [examples/](examples/README.ja.md) | このディレクトリでは、`Sampler`、`Sink`、`NullDevice`などのdevice例、詳細なローカル実行手順、[カスタムuser deviceの作成方法](examples/README.ja.md#4-creating-your-own-user-device)を説明します。 |
| [scripts/](scripts/README.ja.md) | このディレクトリには、プロセス起動・topology補助スクリプトと、device skeleton generator `generate-device-skeleton.py`があります。 |
| [controller/](controller/README.ja.md) | このディレクトリでは、`daq-webctl` HTTP/WebSocket server、Redis制御、telemetry設定を実装しています。 |
| [share/controller/](share/controller/README.ja.md) | このディレクトリには、`daq-webctl`が配信するWebブラウザー向けファイルがあります。 |
| `nestdaq/` | このディレクトリには、version headerのtemplate、FairMQ device applicationのentry pointと`main()`を提供する`runDevice.h`、telemetry codeなど、NestDAQのcore headerとsource codeがあります。 |
| [nestdaq/telemetry/](nestdaq/telemetry/README.ja.md) | このディレクトリでは、必要に応じて有効にできるOpenTelemetry連携を提供します。 |
| [plugins/](plugins/README.ja.md) | このディレクトリでは、DAQ service、metrics、parameter設定用のFairMQ pluginを提供します。 |
| [cmake/](cmake/README.ja.md) | このディレクトリには、CMake helper、インストールされるpackage file、外部依存関係をビルドするprojectがあります。 |
| [tests/](tests/) | このディレクトリには、C++ testとtest用の補助ファイルがあります。 |
| `share/` | このディレクトリには、NestDAQとともにインストールされる設定ファイルと補助ファイルがあります。 |
| [share/otel-collector-compose/](share/otel-collector-compose/README.ja.md) | このディレクトリでは、ローカルOpenTelemetry Collectorと、[OpenSearch](share/otel-collector-compose/opensearch/README.ja.md)、[Victoria](share/otel-collector-compose/victoria/README.ja.md)、[ClickStack](share/otel-collector-compose/clickhouse/README.ja.md)用backend Compose stackを提供します。 |
| [share/redis-stack-container/](share/redis-stack-container/README.ja.md) | このディレクトリでは、Redis Stack containerを実行する補助スクリプトを提供します。 |
| [share/installers/](share/installers/README.ja.md) | このディレクトリでは、実行時に使用する外部サービスをhostへインストールする`apt`・`dnf`用補助スクリプトを提供します。 |
| [CONTRIBUTING.ja.md](CONTRIBUTING.ja.md) | この文書では、ブランチ運用方針、contribution workflow、formatting、static analysis、naming ruleを説明します。 |

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
対応機能を有効にした場合は、opentelemetry-cppやspdlogなどの
telemetry/logging依存関係も使用します。
前提package、依存関係のdefault version、CMake override option、
実行時に使用する外部サービスの選択肢、platform固有のビルド上の注意事項については
[INSTALL.ja.md](INSTALL.ja.md)を参照してください。

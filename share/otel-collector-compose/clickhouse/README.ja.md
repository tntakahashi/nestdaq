# ClickStack OpenTelemetry (OTel) バックエンド

[English](README.md) | [日本語](README.ja.md)

[トップ: NestDAQ](../../../README.ja.md) | [前の保存先候補: Victoria](../victoria/README.ja.md) | [次へ: Package installer](../../installers/README.ja.md)

このローカル検証用スタックは、ClickStack OpenTelemetry CollectorでOpenTelemetryのログ、メトリクス、トレースを受信します。
受信したデータをClickHouseに保存し、ClickStackユーザーインターフェース (UI) で表示します。

このバックエンドは実験的で、まだ十分に検証されていません。

この文書で**Compose**とは、Docker Compose (`docker compose`) またはPodman Compose (`podman compose`) を指します。
どちらかを使用してこのスタックを管理します。

このディレクトリから起動します。
以下のshellコマンド例では、`#`で始まる行は読者向けの説明コメントであり、shellでは実行されません。

```bash
# Docker ComposeでClickHouseの検証用スタックを起動します。
docker compose -f compose-clickhouse.yaml up
```

Podmanの場合:

```bash
# Podman ComposeでClickHouseの検証用スタックを起動します。
podman compose -f compose-clickhouse.yaml up
```

`podman compose`を使用するには、`podman-compose`やDocker Compose pluginなどのCompose providerが必要です。
Compose providerをインストールし、`PATH`から検出できるようにしてください。

<a id="1-components"></a>
## 1. コンポーネント

- `clickstack`: ClickStack UI、OpenTelemetry Collector、ClickHouseを1つのコンテナーで実行します。

ClickStack UIを`http://localhost:8080`で開いてください。
初回利用時にUIユーザーを作成します。
ClickStackはローカルのClickHouse instanceに接続し、ログ、メトリクス、トレース用のdata sourceを準備します。

このスタックはローカル検証用です。
production環境では、明示的な認証情報、retention policy、backup policy、およびこのsample Composeファイルの外部で管理するdeployment topologyを使用してください。

<a id="2-ports"></a>
## 2. ポート

- ClickStack UI: `http://localhost:8080`
- ClickHouse HTTP: `http://localhost:8123`
- OpenTelemetry Protocol (OTLP) Google remote procedure call (gRPC) receiver: `localhost:4317`
- OTLP HTTP receiver: `http://localhost:4318`

<a id="3-nestdaq-telemetry-endpoint-examples"></a>
## 3. NestDAQテレメトリーエンドポイントの例

ホストプロセスはOTLP/gRPCに`localhost:4317`、OTLP/HTTPに`http://localhost:4318`を使用します。
同じComposeネットワーク内のNestDAQ deviceコンテナーまたは`daq-webctl`コンテナーは、OTLP gRPCに`clickstack:4317`、OTLP HTTPに`http://clickstack:4318`を使用してください。

例えば、HTTPエンドポイントでは次のパスを使用します。

```text
http://localhost:4318/v1/logs
http://localhost:4318/v1/metrics
http://localhost:4318/v1/traces
```

<a id="4-environment-variables"></a>
## 4. 環境変数

| 変数 | デフォルト | 説明 |
| :-- | :-- | :-- |
| `CLICKSTACK_IMAGE` | `docker.io/clickhouse/clickstack-all-in-one:2` | ClickStack all-in-oneイメージ。 |
| `CLICKSTACK_UI_PORT` | `8080` | ClickStack UIに割り当てるホストポート。 |
| `CLICKHOUSE_HTTP_PORT` | `8123` | ClickHouse HTTPに割り当てるホストポート。 |
| `OTEL_COLLECTOR_GRPC_PORT` | `4317` | OTLP gRPCに割り当てるホストポート。 |
| `OTEL_COLLECTOR_HTTP_PORT` | `4318` | OTLP HTTPに割り当てるホストポート。 |
| `CLICKSTACK_DB_DIR` | `./clickstack-db` | `/data/db/`にbind mountするホストディレクトリ。 |
| `CLICKSTACK_CLICKHOUSE_DATA_DIR` | `./clickstack-clickhouse-data` | `/var/lib/clickhouse/`にbind mountするホストディレクトリ。 |
| `CLICKSTACK_CLICKHOUSE_LOG_DIR` | `./clickstack-clickhouse-logs` | `/var/log/clickhouse-server/`にbind mountするホストディレクトリ。 |

<a id="5-stop"></a>
## 5. 停止

ローカル検証用コンテナーとネットワークを停止して削除します。

```bash
# Dockerの検証用コンテナーとネットワークを停止して削除します。
docker compose -f compose-clickhouse.yaml down
```

Podmanの場合:

```bash
# Podmanの検証用コンテナーとネットワークを停止して削除します。
podman compose -f compose-clickhouse.yaml down
```

`down`ではClickStackとClickHouseのデータディレクトリおよびログディレクトリを削除しません。
同じディレクトリでこのCompose構成を再び起動すると、以前のバックエンドデータが再利用されます。

保存されたバックエンドデータを破棄したい場合に限り、データディレクトリとログディレクトリを削除してください。

```bash
# ClickStackとClickHouseの全データおよびログを完全に破棄します。
rm -rf ./clickstack-db \
       ./clickstack-clickhouse-data \
       ./clickstack-clickhouse-logs
```

rootless Podmanでは、ファイル所有権のためユーザー名前空間経由で削除する必要がある場合があります。

```bash
# rootless Podmanのデータとログをユーザー名前空間経由で破棄します。
podman unshare rm -rf ./clickstack-db \
                       ./clickstack-clickhouse-data \
                       ./clickstack-clickhouse-logs
```

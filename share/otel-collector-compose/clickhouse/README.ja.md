# ClickStack OpenTelemetry(OTel)バックエンド

[English](README.md) | [日本語](README.ja.md)

このローカル検証用スタックは、ClickStack OpenTelemetry CollectorでOpenTelemetryのログ、メトリクス、トレースを受信し、ClickHouseに保存して、ClickStackユーザーインターフェース(UI)で表示します。

このバックエンドは実験的で、まだ十分に検証されていません。

このディレクトリから起動します。

```bash
docker compose -f compose-clickhouse.yaml up
```

Podmanの場合:

```bash
podman compose -f compose-clickhouse.yaml up
```

`podman compose`を使用するには、`podman-compose`やDocker Compose pluginなどのCompose providerがインストールされ、`PATH`から見つけられる必要があります。

<a id="1-components"></a>
## 1. コンポーネント

- `clickstack`: ClickStack UI、OpenTelemetry Collector、ClickHouseを1つのcontainerで実行します。

ClickStack UIを`http://localhost:8080`で開いてください。初回利用時にUI userを作成します。ClickStackはlocalのClickHouse instanceに接続し、ログ、メトリクス、トレース用のdata sourceを準備します。

このスタックはローカル検証用です。production deploymentでは、明示的な認証情報、retention policy、backup policy、およびこのsample compose fileの外部で管理されるdeployment topologyを使用してください。

<a id="2-ports"></a>
## 2. ポート

- ClickStack UI: `http://localhost:8080`
- ClickHouse HTTP: `http://localhost:8123`
- OpenTelemetry Protocol(OTLP)Google remote procedure call(gRPC)receiver: `localhost:4317`
- OTLP Hypertext Transfer Protocol(HTTP)receiver: `http://localhost:4318`

<a id="3-nestdaq-telemetry-endpoint-examples"></a>
## 3. NestDAQテレメトリーエンドポイントの例

ホストプロセスはOTLP/gRPCに`localhost:4317`、OTLP/HTTPに`http://localhost:4318`を使用します。同じcompose network内のNestDAQ device containerまたは`daq-webctl` containerは、OTLP gRPCに`clickstack:4317`、OTLP HTTPに`http://clickstack:4318`を使用してください。

例えば、HTTP endpointでは次のpathを使用します。

```text
http://localhost:4318/v1/logs
http://localhost:4318/v1/metrics
http://localhost:4318/v1/traces
```

<a id="4-environment-variables"></a>
## 4. 環境変数

| 変数 | デフォルト | 説明 |
| :-- | :-- | :-- |
| `CLICKSTACK_IMAGE` | `docker.io/clickhouse/clickstack-all-in-one:2` | ClickStack all-in-one image。 |
| `CLICKSTACK_UI_PORT` | `8080` | ClickStack UIに割り当てるhost port。 |
| `CLICKHOUSE_HTTP_PORT` | `8123` | ClickHouse HTTPに割り当てるhost port。 |
| `OTEL_COLLECTOR_GRPC_PORT` | `4317` | OTLP gRPCに割り当てるhost port。 |
| `OTEL_COLLECTOR_HTTP_PORT` | `4318` | OTLP HTTPに割り当てるhost port。 |
| `CLICKSTACK_DB_DIR` | `./clickstack-db` | `/data/db`にbind mountするhost directory。 |
| `CLICKSTACK_CLICKHOUSE_DATA_DIR` | `./clickstack-clickhouse-data` | `/var/lib/clickhouse`にbind mountするhost directory。 |
| `CLICKSTACK_CLICKHOUSE_LOG_DIR` | `./clickstack-clickhouse-logs` | `/var/log/clickhouse-server`にbind mountするhost directory。 |

<a id="5-stop"></a>
## 5. 停止

ローカル検証用containerとnetworkを停止して削除します。

```bash
docker compose -f compose-clickhouse.yaml down
```

Podmanの場合:

```bash
podman compose -f compose-clickhouse.yaml down
```

ClickStackとClickHouseのdata/log directoryは`down`では削除されません。同じdirectoryでこのcompose構成を再び起動すると、以前のbackend dataが再利用されます。

保存されたbackend dataを破棄したい場合に限り、data directoryとlog directoryを削除してください。

```bash
rm -rf ./clickstack-db \
       ./clickstack-clickhouse-data \
       ./clickstack-clickhouse-logs
```

rootless Podmanでは、file ownershipのためuser namespace経由で削除する必要がある場合があります。

```bash
podman unshare rm -rf ./clickstack-db \
                       ./clickstack-clickhouse-data \
                       ./clickstack-clickhouse-logs
```

# Victoria OpenTelemetry（OTel）バックエンド

[English](README.md) | [日本語](README.ja.md)

このローカル検証用スタックは、OpenTelemetry CollectorでOpenTelemetryのログ、メトリクス、トレースを受信し、Victoria stack serviceに保存して、Grafanaで表示します。

このバックエンドは実験的で、まだ十分に検証されていません。

このディレクトリから起動します。

```bash
docker compose -f compose-victoria.yaml up
```

Podmanの場合:

```bash
podman compose -f compose-victoria.yaml up
```

`podman compose`を使用するには、`podman-compose`やDocker Compose pluginなどのCompose providerがインストールされ、`PATH`から見つけられる必要があります。

<a id="1-components"></a>
## 1. コンポーネント

- `otel-collector`: OpenTelemetry Protocol（OTLP）のログ、メトリクス、トレースを受信します。
- `victoriametrics`: メトリクスを保存します。
- `victorialogs`: ログを保存します。
- `victoriatraces`: トレースを保存します。
- `grafana`: Victoria service用のExplore viewとdashboardを提供します。

GrafanaにはVictoriaMetrics、VictoriaLogs、VictoriaTracesのdatasourceがprovisioningされています。VictoriaTracesはGrafana組み込みのJaeger datasourceを通して次のように設定されています。

```text
http://victoriatraces:10428/select/jaeger
```

Collectorはログ、メトリクス、トレースを次のendpointへexportします。

```text
http://victorialogs:9428/insert/opentelemetry/v1/logs
http://victoriametrics:8428/opentelemetry/v1/metrics
http://victoriatraces:10428/insert/opentelemetry/v1/traces
```

<a id="2-ports"></a>
## 2. ポート

- VictoriaMetrics: `http://localhost:8428`
- VictoriaLogs: `http://localhost:9428`
- VictoriaTraces: `http://localhost:10428`
- Grafana: `http://localhost:3000`
- OTLP Google remote procedure call（gRPC）receiver: `localhost:4317`
- OTLP Hypertext Transfer Protocol（HTTP）receiver: `http://localhost:4318`

ホストプロセスは上記の`localhost` endpointを使用します。同じcompose network内のNestDAQ device containerまたは`daq-webctl` containerは、OTLP gRPCには`otel-collector:4317`を、OTLP HTTPには`http://otel-collector:4318`を使用してください。

<a id="3-environment-variables"></a>
## 3. 環境変数

| 変数 | デフォルト | 説明 |
| :-- | :-- | :-- |
| `OTEL_COLLECTOR_IMAGE` | `docker.io/otel/opentelemetry-collector-contrib:0.155.0` | Collector image。 |
| `VICTORIAMETRICS_IMAGE` | `docker.io/victoriametrics/victoria-metrics:v1.143.0` | VictoriaMetrics image。 |
| `VICTORIALOGS_IMAGE` | `docker.io/victoriametrics/victoria-logs:v1.50.0` | VictoriaLogs image。 |
| `VICTORIATRACES_IMAGE` | `docker.io/victoriametrics/victoria-traces:v0.8.2` | VictoriaTraces image。 |
| `GRAFANA_IMAGE` | `docker.io/grafana/grafana:12.4.0` | Grafana image。 |
| `VICTORIAMETRICS_PORT` | `8428` | VictoriaMetricsに割り当てるhost port。 |
| `VICTORIALOGS_PORT` | `9428` | VictoriaLogsに割り当てるhost port。 |
| `VICTORIATRACES_PORT` | `10428` | VictoriaTracesに割り当てるhost port。 |
| `GRAFANA_PORT` | `3000` | Grafanaに割り当てるhost port。 |
| `OTEL_COLLECTOR_GRPC_PORT` | `4317` | OTLP gRPCに割り当てるhost port。 |
| `OTEL_COLLECTOR_HTTP_PORT` | `4318` | OTLP HTTPに割り当てるhost port。 |
| `VICTORIAMETRICS_DATA_DIR` | `./victoriametrics-data` | VictoriaMetrics data用host directory。 |
| `VICTORIALOGS_DATA_DIR` | `./victorialogs-data` | VictoriaLogs data用host directory。 |
| `VICTORIATRACES_DATA_DIR` | `./victoriatraces-data` | VictoriaTraces data用host directory。 |
| `GRAFANA_DATA_DIR` | `./grafana-data` | `/var/lib/grafana`にbind mountするhost directory。 |
| `GRAFANA_ADMIN_PASSWORD` | `admin` | Grafana admin password。 |
| `GRAFANA_PROVISIONING_DIR` | `./grafana/provisioning` | Grafana provisioning directory。 |
| `OTEL_COLLECTOR_CONFIG_FILE` | `./otel-collector-config-victoria.yaml` | Collector config file。 |

<a id="4-stop"></a>
## 4. 停止

ローカル検証用containerとnetworkを停止して削除します。

```bash
docker compose -f compose-victoria.yaml down
```

Podmanの場合:

```bash
podman compose -f compose-victoria.yaml down
```

VictoriaとGrafanaのdata directoryは`down`では削除されません。同じdata directoryでこのcompose構成を再び起動すると、以前のログ、メトリクス、トレース、およびGrafanaの状態が再利用されます。

保存されたbackend dataを破棄したい場合に限り、data directoryを削除してください。

```bash
rm -rf ./victoriametrics-data \
       ./victorialogs-data \
       ./victoriatraces-data \
       ./grafana-data
```

rootless Podmanでは、file ownershipのためuser namespace経由で削除する必要がある場合があります。

```bash
podman unshare rm -rf ./victoriametrics-data \
                       ./victorialogs-data \
                       ./victoriatraces-data \
                       ./grafana-data
```

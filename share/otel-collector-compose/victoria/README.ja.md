# Victoria OpenTelemetry(OTel)バックエンド

[English](README.md) | [日本語](README.ja.md)

このローカル検証用スタックは、OpenTelemetry CollectorでOpenTelemetryのログ、メトリクス、トレースを受信します。
受信したデータをVictoria stackを構成する各serviceに保存し、Grafanaで表示します。

このバックエンドは実験的で、まだ十分に検証されていません。

この文書で**Compose**とは、Docker Compose (`docker compose`)またはPodman Compose (`podman compose`)を指します。
どちらかを使用してこのスタックを管理します。

このディレクトリから起動します。
以下のshellコマンド例では、`#`で始まる行は読者向けの説明コメントであり、shellでは実行されません。

```bash
# Docker ComposeでVictoriaの検証用スタックを起動します。
docker compose -f compose-victoria.yaml up
```

Podmanの場合:

```bash
# Podman ComposeでVictoriaの検証用スタックを起動します。
podman compose -f compose-victoria.yaml up
```

`podman compose`を使用するには、`podman-compose`やDocker Compose pluginなどのCompose providerが必要です。
Compose providerをインストールし、`PATH`から検出できるようにしてください。

<a id="1-components"></a>
## 1. コンポーネント

- `otel-collector`: OpenTelemetry Protocol(OTLP)のログ、メトリクス、トレースを受信します。
- `victoriametrics`: メトリクスを保存します。
- `victorialogs`: ログを保存します。
- `victoriatraces`: トレースを保存します。
- `grafana`: 各Victoria service用のExplore viewとdashboardを提供します。

GrafanaにはVictoriaMetrics、VictoriaLogs、VictoriaTracesのdata sourceがprovisioningによって設定されています。
VictoriaTracesはGrafana組み込みのJaeger data sourceを使用し、次のURLに接続します。

```text
http://victoriatraces:10428/select/jaeger
```

Collectorはログ、メトリクス、トレースを次のエンドポイントへexportします。

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
- OTLP Google remote procedure call(gRPC) receiver: `localhost:4317`
- OTLP Hypertext Transfer Protocol(HTTP) receiver: `http://localhost:4318`

ホストプロセスは上記の`localhost`エンドポイントを使用します。
同じComposeネットワーク内のNestDAQ deviceコンテナーまたは`daq-webctl`コンテナーは、OTLP gRPCには`otel-collector:4317`を、OTLP HTTPには`http://otel-collector:4318`を使用してください。

<a id="3-environment-variables"></a>
## 3. 環境変数

| 変数 | デフォルト | 説明 |
| :-- | :-- | :-- |
| `OTEL_COLLECTOR_IMAGE` | `docker.io/otel/opentelemetry-collector-contrib:0.155.0` | Collectorイメージ。 |
| `VICTORIAMETRICS_IMAGE` | `docker.io/victoriametrics/victoria-metrics:v1.143.0` | VictoriaMetricsイメージ。 |
| `VICTORIALOGS_IMAGE` | `docker.io/victoriametrics/victoria-logs:v1.50.0` | VictoriaLogsイメージ。 |
| `VICTORIATRACES_IMAGE` | `docker.io/victoriametrics/victoria-traces:v0.8.2` | VictoriaTracesイメージ。 |
| `GRAFANA_IMAGE` | `docker.io/grafana/grafana:12.4.0` | Grafanaイメージ。 |
| `VICTORIAMETRICS_PORT` | `8428` | VictoriaMetricsに割り当てるホストポート。 |
| `VICTORIALOGS_PORT` | `9428` | VictoriaLogsに割り当てるホストポート。 |
| `VICTORIATRACES_PORT` | `10428` | VictoriaTracesに割り当てるホストポート。 |
| `GRAFANA_PORT` | `3000` | Grafanaに割り当てるホストポート。 |
| `OTEL_COLLECTOR_GRPC_PORT` | `4317` | OTLP gRPCに割り当てるホストポート。 |
| `OTEL_COLLECTOR_HTTP_PORT` | `4318` | OTLP HTTPに割り当てるホストポート。 |
| `VICTORIAMETRICS_DATA_DIR` | `./victoriametrics-data` | VictoriaMetricsデータ用のホストディレクトリ。 |
| `VICTORIALOGS_DATA_DIR` | `./victorialogs-data` | VictoriaLogsデータ用のホストディレクトリ。 |
| `VICTORIATRACES_DATA_DIR` | `./victoriatraces-data` | VictoriaTracesデータ用のホストディレクトリ。 |
| `GRAFANA_DATA_DIR` | `./grafana-data` | `/var/lib/grafana`にbind mountするホストディレクトリ。 |
| `GRAFANA_ADMIN_PASSWORD` | `admin` | Grafana管理者パスワード。 |
| `GRAFANA_PROVISIONING_DIR` | `./grafana/provisioning` | Grafana provisioningディレクトリ。 |
| `OTEL_COLLECTOR_CONFIG_FILE` | `./otel-collector-config-victoria.yaml` | Collector設定ファイル。 |

<a id="4-stop"></a>
## 4. 停止

ローカル検証用コンテナーとネットワークを停止して削除します。

```bash
# Dockerの検証用コンテナーとネットワークを停止して削除します。
docker compose -f compose-victoria.yaml down
```

Podmanの場合:

```bash
# Podmanの検証用コンテナーとネットワークを停止して削除します。
podman compose -f compose-victoria.yaml down
```

`down`ではVictoriaとGrafanaのデータディレクトリを削除しません。
同じデータディレクトリでこのCompose構成を再び起動すると、以前のログ、メトリクス、トレース、およびGrafanaの状態が再利用されます。

保存されたバックエンドデータを破棄したい場合に限り、データディレクトリを削除してください。

```bash
# VictoriaとGrafanaの全データを完全に破棄します。
rm -rf ./victoriametrics-data \
       ./victorialogs-data \
       ./victoriatraces-data \
       ./grafana-data
```

rootless Podmanでは、ファイル所有権のためユーザー名前空間経由で削除する必要がある場合があります。

```bash
# rootless Podmanのデータをユーザー名前空間経由で破棄します。
podman unshare rm -rf ./victoriametrics-data \
                       ./victorialogs-data \
                       ./victoriatraces-data \
                       ./grafana-data
```

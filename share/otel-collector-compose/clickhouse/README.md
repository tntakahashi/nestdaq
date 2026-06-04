# ClickHouse OTel Backend

This local validation stack receives OpenTelemetry logs, metrics, and traces
with OpenTelemetry Collector, stores them in ClickHouse, and opens them in
Grafana.

Start from this directory:

```bash
docker compose -f compose-clickhouse.yaml up
```

For Podman:

```bash
podman compose -f compose-clickhouse.yaml up
```

## Components

- `otel-collector`: receives OTLP logs, metrics, and traces.
- `clickhouse`: stores logs, metrics, and traces.
- `grafana`: provides a ClickHouse datasource using the official Grafana
  ClickHouse plugin.

The collector uses the ClickHouse exporter over the native protocol:

```text
tcp://clickhouse:9000?dial_timeout=10s
```

It creates the `otel` database and default OpenTelemetry tables on startup.
The table names are:

- logs: `otel.otel_logs`
- traces: `otel.otel_traces`
- metrics: `otel.otel_metrics_gauge`
- metrics: `otel.otel_metrics_sum`
- metrics: `otel.otel_metrics_summary`
- metrics: `otel.otel_metrics_histogram`
- metrics: `otel.otel_metrics_exp_histogram`

The OpenTelemetry Collector ClickHouse exporter currently marks logs and traces
as beta and metrics as alpha. This is suitable for local validation; production
deployments should manage schema, retention, credentials, and exporter upgrades
explicitly.

Grafana is provisioned with a `ClickHouse` datasource that connects to
`clickhouse:9000`, database `otel`, user `default`.

## Ports

- ClickHouse HTTP: `http://localhost:8123`
- ClickHouse native protocol: `localhost:9000`
- Grafana: `http://localhost:3000`
- OTLP gRPC receiver: `localhost:4317`
- OTLP HTTP receiver: `http://localhost:4318`

## Runtime Options

| Variable | Default | Description |
| :-- | :-- | :-- |
| `OTEL_COLLECTOR_IMAGE` | `docker.io/otel/opentelemetry-collector-contrib:0.150.1` | Collector image. |
| `CLICKHOUSE_IMAGE` | `docker.io/clickhouse/clickhouse-server:26.3.12.3-lts` | ClickHouse image. |
| `GRAFANA_IMAGE` | `docker.io/grafana/grafana:12.4.0` | Grafana image. |
| `CLICKHOUSE_HTTP_PORT` | `8123` | Host port mapped to ClickHouse HTTP. |
| `CLICKHOUSE_NATIVE_PORT` | `9000` | Host port mapped to ClickHouse native protocol. |
| `GRAFANA_PORT` | `3000` | Host port mapped to Grafana. |
| `OTEL_COLLECTOR_GRPC_PORT` | `4317` | Host port mapped to OTLP gRPC. |
| `OTEL_COLLECTOR_HTTP_PORT` | `4318` | Host port mapped to OTLP HTTP. |
| `CLICKHOUSE_DATA_DIR` | `./clickhouse-data` | Host directory bind-mounted to `/var/lib/clickhouse`. |
| `CLICKHOUSE_LOG_DIR` | `./clickhouse-logs` | Host directory bind-mounted to `/var/log/clickhouse-server`. |
| `GRAFANA_DATA_DIR` | `./grafana-data` | Host directory bind-mounted to `/var/lib/grafana`. |
| `GRAFANA_ADMIN_PASSWORD` | `admin` | Grafana admin password. |
| `GRAFANA_PROVISIONING_DIR` | `./grafana/provisioning` | Grafana provisioning directory. |
| `OTEL_COLLECTOR_CONFIG_FILE` | `./otel-collector-config-clickhouse.yaml` | Collector config file. |

## Stop

```bash
docker compose -f compose-clickhouse.yaml down
rm -rf ./clickhouse-data \
       ./clickhouse-logs \
       ./grafana-data
```

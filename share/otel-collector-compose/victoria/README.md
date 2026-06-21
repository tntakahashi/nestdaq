# Victoria OpenTelemetry (OTel) Backend

This local validation stack receives OpenTelemetry logs, metrics, and traces
with OpenTelemetry Collector, stores them in Victoria stack services, and opens
them in Grafana.

Start from this directory:

```bash
docker compose -f compose-victoria.yaml up
```

For Podman:

```bash
podman compose -f compose-victoria.yaml up
```

## Components

- `otel-collector`: receives OpenTelemetry Protocol (OTLP) logs, metrics, and
  traces.
- `victoriametrics`: stores metrics.
- `victorialogs`: stores logs.
- `victoriatraces`: stores traces.
- `grafana`: provides Explore views and dashboards for the Victoria services.

Grafana is provisioned with VictoriaMetrics, VictoriaLogs, and VictoriaTraces
datasources. VictoriaTraces is configured through Grafana's built-in Jaeger
datasource using:

```text
http://victoriatraces:10428/select/jaeger
```

The collector exports logs, metrics, and traces to these endpoints:

```text
http://victorialogs:9428/insert/opentelemetry/v1/logs
http://victoriametrics:8428/opentelemetry/v1/metrics
http://victoriatraces:10428/insert/opentelemetry/v1/traces
```

## Ports

- VictoriaMetrics: `http://localhost:8428`
- VictoriaLogs: `http://localhost:9428`
- VictoriaTraces: `http://localhost:10428`
- Grafana: `http://localhost:3000`
- OTLP Google remote procedure call (gRPC) receiver: `localhost:4317`
- OTLP Hypertext Transfer Protocol (HTTP) receiver: `http://localhost:4318`

## Runtime Options

| Variable | Default | Description |
| :-- | :-- | :-- |
| `OTEL_COLLECTOR_IMAGE` | `docker.io/otel/opentelemetry-collector-contrib:0.150.1` | Collector image. |
| `VICTORIAMETRICS_IMAGE` | `docker.io/victoriametrics/victoria-metrics:v1.143.0` | VictoriaMetrics image. |
| `VICTORIALOGS_IMAGE` | `docker.io/victoriametrics/victoria-logs:v1.50.0` | VictoriaLogs image. |
| `VICTORIATRACES_IMAGE` | `docker.io/victoriametrics/victoria-traces:v0.8.2` | VictoriaTraces image. |
| `GRAFANA_IMAGE` | `docker.io/grafana/grafana:12.4.0` | Grafana image. |
| `VICTORIAMETRICS_PORT` | `8428` | Host port mapped to VictoriaMetrics. |
| `VICTORIALOGS_PORT` | `9428` | Host port mapped to VictoriaLogs. |
| `VICTORIATRACES_PORT` | `10428` | Host port mapped to VictoriaTraces. |
| `GRAFANA_PORT` | `3000` | Host port mapped to Grafana. |
| `OTEL_COLLECTOR_GRPC_PORT` | `4317` | Host port mapped to OTLP gRPC. |
| `OTEL_COLLECTOR_HTTP_PORT` | `4318` | Host port mapped to OTLP HTTP. |
| `VICTORIAMETRICS_DATA_DIR` | `./victoriametrics-data` | Host directory for VictoriaMetrics data. |
| `VICTORIALOGS_DATA_DIR` | `./victorialogs-data` | Host directory for VictoriaLogs data. |
| `VICTORIATRACES_DATA_DIR` | `./victoriatraces-data` | Host directory for VictoriaTraces data. |
| `GRAFANA_DATA_DIR` | `./grafana-data` | Host directory bind-mounted to `/var/lib/grafana`. |
| `GRAFANA_ADMIN_PASSWORD` | `admin` | Grafana admin password. |
| `GRAFANA_PROVISIONING_DIR` | `./grafana/provisioning` | Grafana provisioning directory. |
| `OTEL_COLLECTOR_CONFIG_FILE` | `./otel-collector-config-victoria.yaml` | Collector config file. |

## Stop

```bash
docker compose -f compose-victoria.yaml down
rm -rf ./victoriametrics-data \
       ./victorialogs-data \
       ./victoriatraces-data \
       ./grafana-data
```

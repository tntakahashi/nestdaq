# Victoria OpenTelemetry (OTel) Backend

[English](README.md) | [日本語](README.ja.md)

This local validation stack receives OpenTelemetry logs, metrics, and traces
with OpenTelemetry Collector, stores them in Victoria stack services, and opens
them in Grafana.

This backend is experimental and not yet fully verified.

Use either Docker Compose (`docker compose`) or Podman Compose
(`podman compose`) to manage this stack.

Start from this directory:

```bash
docker compose -f compose-victoria.yaml up
```

For Podman:

```bash
podman compose -f compose-victoria.yaml up
```

`podman compose` requires a Compose provider such as `podman-compose` or the
Docker Compose plugin to be installed and discoverable in `PATH`.

## 1. Components

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

## 2. Ports

- VictoriaMetrics: `http://localhost:8428`
- VictoriaLogs: `http://localhost:9428`
- VictoriaTraces: `http://localhost:10428`
- Grafana: `http://localhost:3000`
- OTLP Google remote procedure call (gRPC) receiver: `localhost:4317`
- OTLP Hypertext Transfer Protocol (HTTP) receiver: `http://localhost:4318`

Host processes use the `localhost` endpoints above. A NestDAQ device container
or `daq-webctl` container in the same compose network should use
`otel-collector:4317` for OTLP gRPC, or `http://otel-collector:4318` for OTLP
HTTP.

<a id="3-runtime-options"></a>
## 3. Environment Variables

| Variable | Default | Description |
| :-- | :-- | :-- |
| `OTEL_COLLECTOR_IMAGE` | `docker.io/otel/opentelemetry-collector-contrib:0.155.0` | Collector image. |
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

## 4. Stop

Stop and remove the local validation containers and network:

```bash
docker compose -f compose-victoria.yaml down
```

For Podman:

```bash
podman compose -f compose-victoria.yaml down
```

The Victoria and Grafana data directories are not deleted by `down`. If you
start this compose setup again with the same data directories, the previous
logs, metrics, traces, and Grafana state are reused.

Delete the data directories only when you want to discard the stored backend
data:

```bash
rm -rf ./victoriametrics-data \
       ./victorialogs-data \
       ./victoriatraces-data \
       ./grafana-data
```

For rootless Podman, file ownership may require removal through the user
namespace:

```bash
podman unshare rm -rf ./victoriametrics-data \
                       ./victorialogs-data \
                       ./victoriatraces-data \
                       ./grafana-data
```

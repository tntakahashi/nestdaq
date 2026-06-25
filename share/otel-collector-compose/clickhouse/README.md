# ClickStack OpenTelemetry (OTel) Backend

This local validation stack receives OpenTelemetry logs, metrics, and traces
with the ClickStack OpenTelemetry Collector, stores them in ClickHouse, and
opens them in the ClickStack user interface (UI).

Start from this directory:

```bash
docker compose -f compose-clickhouse.yaml up
```

For Podman:

```bash
podman compose -f compose-clickhouse.yaml up
```

## Components

- `clickstack`: runs the ClickStack UI, OpenTelemetry Collector, and
  ClickHouse in one container.

Open the ClickStack UI at `http://localhost:8080`. On first use, create the UI
user. ClickStack connects to the local ClickHouse instance and prepares data
sources for logs, metrics, and traces.

This stack is intended for local validation. Production deployments should use
explicit credentials, retention policy, backup policy, and a deployment topology
managed outside this sample compose file.

## Ports

- ClickStack UI: `http://localhost:8080`
- ClickHouse HTTP: `http://localhost:8123`
- OpenTelemetry Protocol (OTLP) Google remote procedure call (gRPC) receiver: `localhost:4317`
- OTLP Hypertext Transfer Protocol (HTTP) receiver: `http://localhost:4318`

## NestDAQ Telemetry Endpoint Examples

Host processes use `localhost:4317` for OTLP/gRPC or
`http://localhost:4318` for OTLP/HTTP. A NestDAQ device container or
`daq-webctl` container in the same compose network should use
`clickstack:4317` for OTLP gRPC, or `http://clickstack:4318` for OTLP HTTP.

For example, HTTP endpoints use these paths:

```text
http://localhost:4318/v1/logs
http://localhost:4318/v1/metrics
http://localhost:4318/v1/traces
```

## Runtime Options

| Variable | Default | Description |
| :-- | :-- | :-- |
| `CLICKSTACK_IMAGE` | `docker.io/clickhouse/clickstack-all-in-one:2` | ClickStack all-in-one image. |
| `CLICKSTACK_UI_PORT` | `8080` | Host port mapped to the ClickStack UI. |
| `CLICKHOUSE_HTTP_PORT` | `8123` | Host port mapped to ClickHouse HTTP. |
| `OTEL_COLLECTOR_GRPC_PORT` | `4317` | Host port mapped to OTLP gRPC. |
| `OTEL_COLLECTOR_HTTP_PORT` | `4318` | Host port mapped to OTLP HTTP. |
| `CLICKSTACK_DB_DIR` | `./clickstack-db` | Host directory bind-mounted to `/data/db`. |
| `CLICKSTACK_CLICKHOUSE_DATA_DIR` | `./clickstack-clickhouse-data` | Host directory bind-mounted to `/var/lib/clickhouse`. |
| `CLICKSTACK_CLICKHOUSE_LOG_DIR` | `./clickstack-clickhouse-logs` | Host directory bind-mounted to `/var/log/clickhouse-server`. |

## Stop

Stop and remove the local validation container and network:

```bash
docker compose -f compose-clickhouse.yaml down
```

For Podman:

```bash
podman compose -f compose-clickhouse.yaml down
```

The ClickStack and ClickHouse data/log directories are not deleted by `down`.
If you start this compose setup again with the same directories, the previous
backend data is reused.

Delete the data and log directories only when you want to discard the stored
backend data:

```bash
rm -rf ./clickstack-db \
       ./clickstack-clickhouse-data \
       ./clickstack-clickhouse-logs
```

For rootless Podman, file ownership may require removal through the user
namespace:

```bash
podman unshare rm -rf ./clickstack-db \
                       ./clickstack-clickhouse-data \
                       ./clickstack-clickhouse-logs
```

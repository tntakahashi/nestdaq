# OpenTelemetry Collector Compose Setups

This directory contains local validation Compose setups for receiving
OpenTelemetry data with OpenTelemetry Collector and storing it in one selected
backend.

These stacks are intended for local validation only. They publish service ports
on the host, use simple local credentials where applicable, and should not be
exposed on a public or shared network.

No default `compose.yaml` or `docker-compose.yaml` is installed. Choose one
backend explicitly:

- `compose-opensearch.yaml`: logs and traces in OpenSearch, viewed with
  OpenSearch Dashboards.
- `compose-victoria.yaml`: logs, metrics, and traces in VictoriaLogs,
  VictoriaMetrics, and VictoriaTraces, viewed with Grafana.
- `compose-clickhouse.yaml`: logs, metrics, and traces in ClickHouse, viewed
  with Grafana.

## Start

After installation, copy the installed setup to a working directory. If
`./otel-collector-compose` already exists, remove it first or choose a
different destination.

```bash
cp -a <install-prefix>/share/otel-collector-compose ./otel-collector-compose
cd ./otel-collector-compose
```

Start one backend stack.

```bash
docker compose -f compose-opensearch.yaml up
docker compose -f compose-victoria.yaml up
docker compose -f compose-clickhouse.yaml up
```

For Podman, use the same files with `podman compose`:

```bash
podman compose -f compose-opensearch.yaml up
podman compose -f compose-victoria.yaml up
podman compose -f compose-clickhouse.yaml up
```

If you run multiple stacks at the same time, override conflicting host ports
such as `GRAFANA_PORT`, `OTEL_COLLECTOR_GRPC_PORT`, and
`OTEL_COLLECTOR_HTTP_PORT`.

## OpenSearch Backend

`compose-opensearch.yaml` starts:

- `otel-collector`: receives OTLP logs and traces over gRPC and HTTP.
- `opensearch`: stores logs and traces exported by the collector.
- `opensearch-dashboards`: provides the web UI for OpenSearch.
- `opensearch-dashboards-setup`: creates initial Data Views for logs and
  traces if they do not already exist.

OpenSearch 2.12 and later, including OpenSearch 3.x, requires
`OPENSEARCH_INITIAL_ADMIN_PASSWORD` when the bundled demo security
configuration is installed. This local validation compose disables that demo
configuration installer and the Security plugin, so no OpenSearch admin
password is required for this stack.

Open `http://localhost:5601/app/discover` after the stack starts. The setup
service creates Data Views for `otel-logs-*` and `otel-traces-*`, and sets
`otel-logs-*` as the default only when no default Data View is already
configured. Existing Data Views and the existing default are left unchanged on
later runs.

The collector stores logs in indices named:

```text
otel-logs-%{service.name}-yyyy.MM.dd
```

It stores traces in indices named:

```text
otel-traces-%{service.name}-yyyy.MM.dd
```

If `service.name` is missing, `unknown-service` is used. OpenSearch requires
lowercase index names. NestDAQ telemetry lowercases ASCII uppercase letters in
`service.name` before export; external OTLP clients should also send lowercase
`service.name` values when using this compose setup.

Default ports:

- OpenSearch: `http://localhost:9200`
- OpenSearch Dashboards: `http://localhost:5601`
- OTLP gRPC receiver: `localhost:4317`
- OTLP HTTP receiver: `http://localhost:4318`

OpenSearch runs as container `uid=1000,gid=1000`. With rootless Podman, the
host directory bind-mounted to `/usr/share/opensearch/data` must be readable
and writable by that container uid/gid as seen from the Podman user namespace:

```bash
mkdir -p ./opensearch-data
podman unshare chown -R 1000:1000 ./opensearch-data
podman unshare chmod -R u+rwX ./opensearch-data
podman compose -f compose-opensearch.yaml up
```

Alternatively, map the container's `1000:1000` user to the host user that
starts Podman Compose:

```bash
mkdir -p ./opensearch-data
PODMAN_USERNS="keep-id:uid=1000,gid=1000" \
podman compose --in-pod=false -f compose-opensearch.yaml up
```

The `PODMAN_USERNS` setting changes the user namespace mapping. It does not
change the OpenSearch image's runtime user, which remains container
`uid=1000,gid=1000`.

If a previous run failed while requesting
`OPENSEARCH_INITIAL_ADMIN_PASSWORD`, stop the stack and remove the local
OpenSearch data directory before starting again.

```bash
docker compose -f compose-opensearch.yaml down
rm -rf ./opensearch-data
```

For rootless Podman:

```bash
podman compose -f compose-opensearch.yaml down
podman unshare rm -rf ./opensearch-data
```

## Victoria Backend

`compose-victoria.yaml` starts:

- `otel-collector`: receives OTLP logs, metrics, and traces.
- `victoriametrics`: stores metrics.
- `victorialogs`: stores logs.
- `victoriatraces`: stores traces.
- `grafana`: provides Explore views and dashboards for the Victoria services.

Default ports:

- VictoriaMetrics: `http://localhost:8428`
- VictoriaLogs: `http://localhost:9428`
- VictoriaTraces: `http://localhost:10428`
- Grafana: `http://localhost:3000`
- OTLP gRPC receiver: `localhost:4317`
- OTLP HTTP receiver: `http://localhost:4318`

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

## ClickHouse Backend

`compose-clickhouse.yaml` starts:

- `otel-collector`: receives OTLP logs, metrics, and traces.
- `clickhouse`: stores logs, metrics, and traces.
- `grafana`: provides a ClickHouse datasource using the official Grafana
  ClickHouse plugin.

Default ports:

- ClickHouse HTTP: `http://localhost:8123`
- ClickHouse native protocol: `localhost:9000`
- Grafana: `http://localhost:3000`
- OTLP gRPC receiver: `localhost:4317`
- OTLP HTTP receiver: `http://localhost:4318`

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

## Runtime Options

All stacks use pinned image defaults and allow overriding them with environment
variables.

| Variable | Default | Used by | Description |
| :-- | :-- | :-- | :-- |
| `OTEL_COLLECTOR_IMAGE` | `docker.io/otel/opentelemetry-collector-contrib:0.150.1` | all | Collector image. |
| `OPENSEARCH_IMAGE` | `docker.io/opensearchproject/opensearch:3.6.0` | OpenSearch | OpenSearch image. |
| `OPENSEARCH_DASHBOARDS_IMAGE` | `docker.io/opensearchproject/opensearch-dashboards:3.6.0` | OpenSearch | OpenSearch Dashboards image. |
| `VICTORIAMETRICS_IMAGE` | `docker.io/victoriametrics/victoria-metrics:v1.143.0` | Victoria | VictoriaMetrics image. |
| `VICTORIALOGS_IMAGE` | `docker.io/victoriametrics/victoria-logs:v1.50.0` | Victoria | VictoriaLogs image. |
| `VICTORIATRACES_IMAGE` | `docker.io/victoriametrics/victoria-traces:v0.8.2` | Victoria | VictoriaTraces image. |
| `CLICKHOUSE_IMAGE` | `docker.io/clickhouse/clickhouse-server:26.3.12.3-lts` | ClickHouse | ClickHouse image. |
| `GRAFANA_IMAGE` | `docker.io/grafana/grafana:12.4.0` | Victoria, ClickHouse | Grafana image. |

Common port and config variables:

| Variable | Default | Used by | Description |
| :-- | :-- | :-- | :-- |
| `OTEL_COLLECTOR_GRPC_PORT` | `4317` | all | Host port mapped to OTLP gRPC. |
| `OTEL_COLLECTOR_HTTP_PORT` | `4318` | all | Host port mapped to OTLP HTTP. |
| `OTEL_COLLECTOR_CONFIG_FILE` | backend-specific config | all | Collector config file mounted into the collector. |
| `GRAFANA_PORT` | `3000` | Victoria, ClickHouse | Host port mapped to Grafana. |
| `GRAFANA_ADMIN_PASSWORD` | `admin` | Victoria, ClickHouse | Grafana admin password. |
| `GRAFANA_DATA_DIR` | `./grafana-data` | Victoria, ClickHouse | Host directory bind-mounted to `/var/lib/grafana`. |
| `GRAFANA_PROVISIONING_DIR` | backend-specific provisioning | Victoria, ClickHouse | Host directory mounted to `/etc/grafana/provisioning`. |

Backend-specific data and port variables:

| Variable | Default | Used by | Description |
| :-- | :-- | :-- | :-- |
| `OPENSEARCH_PORT` | `9200` | OpenSearch | Host port mapped to OpenSearch. |
| `OPENSEARCH_DASHBOARDS_PORT` | `5601` | OpenSearch | Host port mapped to OpenSearch Dashboards. |
| `OPENSEARCH_DATA_DIR` | `./opensearch-data` | OpenSearch | Host directory bind-mounted to `/usr/share/opensearch/data`. |
| `OPENSEARCH_DASHBOARDS_CONFIG_FILE` | `./opensearch_dashboards.yaml` | OpenSearch | OpenSearch Dashboards config file. |
| `OPENSEARCH_DASHBOARDS_SETUP_SCRIPT` | `./opensearch-dashboards/setup-dashboards.js` | OpenSearch | Initial Dashboards setup script. |
| `VICTORIAMETRICS_PORT` | `8428` | Victoria | Host port mapped to VictoriaMetrics. |
| `VICTORIALOGS_PORT` | `9428` | Victoria | Host port mapped to VictoriaLogs. |
| `VICTORIATRACES_PORT` | `10428` | Victoria | Host port mapped to VictoriaTraces. |
| `VICTORIAMETRICS_DATA_DIR` | `./victoriametrics-data` | Victoria | Host directory for VictoriaMetrics data. |
| `VICTORIALOGS_DATA_DIR` | `./victorialogs-data` | Victoria | Host directory for VictoriaLogs data. |
| `VICTORIATRACES_DATA_DIR` | `./victoriatraces-data` | Victoria | Host directory for VictoriaTraces data. |
| `CLICKHOUSE_HTTP_PORT` | `8123` | ClickHouse | Host port mapped to ClickHouse HTTP. |
| `CLICKHOUSE_NATIVE_PORT` | `9000` | ClickHouse | Host port mapped to ClickHouse native protocol. |
| `CLICKHOUSE_DATA_DIR` | `./clickhouse-data` | ClickHouse | Host directory bind-mounted to `/var/lib/clickhouse`. |
| `CLICKHOUSE_LOG_DIR` | `./clickhouse-logs` | ClickHouse | Host directory bind-mounted to `/var/log/clickhouse-server`. |

On SELinux-enabled systems, the Compose files apply the `:Z` label option to
bind-mounted paths.

## Stop

Stop a stack with the same compose file used to start it:

```bash
docker compose -f compose-opensearch.yaml down
docker compose -f compose-victoria.yaml down
docker compose -f compose-clickhouse.yaml down
```

For Podman:

```bash
podman compose -f compose-opensearch.yaml down
podman compose -f compose-victoria.yaml down
podman compose -f compose-clickhouse.yaml down
```

To remove data stored in the default bind-mounted directories:

```bash
rm -rf ./opensearch-data \
       ./victoriametrics-data \
       ./victorialogs-data \
       ./victoriatraces-data \
       ./clickhouse-data \
       ./clickhouse-logs \
       ./grafana-data
```

With rootless Podman, remove directories from the Podman user namespace if the
host user cannot delete files created by containers:

```bash
podman unshare rm -rf ./opensearch-data \
  ./victoriametrics-data \
  ./victorialogs-data \
  ./victoriatraces-data \
  ./clickhouse-data \
  ./clickhouse-logs \
  ./grafana-data
```

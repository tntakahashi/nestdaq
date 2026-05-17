# OpenTelemetry Collector Compose Setup

## OpenTelemetry Collector with OpenSearch

This directory contains a Compose setup for receiving OpenTelemetry data with
OpenTelemetry Collector, storing logs and traces in OpenSearch, and viewing
them with OpenSearch Dashboards.

This stack is intended for local validation only. It disables OpenSearch
security and publishes service ports on the host, so do not expose it on a
public or shared network.

The installed package provides `docker-compose.yaml` together with the collector
OpenSearch Dashboards configuration file and Grafana provisioning files.

## Components

- `otel-collector`: receives OTLP data over gRPC and HTTP.
- `opensearch`: stores logs and traces exported by the collector.
- `opensearch-dashboards`: provides the web UI for OpenSearch.
- `victoriametrics`: stores metrics exported by the collector when the
  `victoria` profile is enabled.
- `victorialogs`: stores logs exported by the collector when the `victoria`
  profile is enabled.
- `victoriatraces`: stores traces exported by the collector when the `victoria`
  profile is enabled.
- `grafana`: provides dashboards and Explore views for VictoriaMetrics,
  VictoriaLogs, and VictoriaTraces when the `victoria` profile is enabled.

## Start

After installation, copy the installed compose file, config files, and Grafana
provisioning files to a working directory.
```bash
mkdir -p ./otel-collector-compose
cp <install-prefix>/share/otel-collector-compose/docker-compose.yaml \
   <install-prefix>/share/otel-collector-compose/otel-collector-config.yaml \
   <install-prefix>/share/otel-collector-compose/otel-collector-config-victoria.yaml \
   <install-prefix>/share/otel-collector-compose/opensearch_dashboards.yaml \
   ./otel-collector-compose/
cp -r <install-prefix>/share/otel-collector-compose/grafana \
   ./otel-collector-compose/
```

Then run Compose with the copied compose file.

For Docker:

```bash
docker compose -f ./otel-collector-compose/docker-compose.yaml up
```

For Podman, create writable data directories first:

```bash
mkdir -p ./otel-collector-compose/opensearch-data
podman unshare chown -R 1000:1000 ./otel-collector-compose/opensearch-data
podman compose -f ./otel-collector-compose/docker-compose.yaml up
```

To also start VictoriaMetrics, VictoriaLogs, VictoriaTraces, and Grafana, use
the `victoria` profile and the Victoria collector config:

```bash
OTEL_COLLECTOR_CONFIG_FILE=./otel-collector-config-victoria.yaml \
docker compose -f ./otel-collector-compose/docker-compose.yaml --profile victoria up
```

For Podman with the `victoria` profile:

```bash
mkdir -p ./otel-collector-compose/victoriametrics-data \
         ./otel-collector-compose/victorialogs-data \
         ./otel-collector-compose/victoriatraces-data \
         ./otel-collector-compose/grafana-data
podman unshare chown -R 1000:1000 \
  ./otel-collector-compose/victoriametrics-data \
  ./otel-collector-compose/victorialogs-data \
  ./otel-collector-compose/victoriatraces-data \
  ./otel-collector-compose/grafana-data
OTEL_COLLECTOR_CONFIG_FILE=./otel-collector-config-victoria.yaml \
podman compose -f ./otel-collector-compose/docker-compose.yaml --profile victoria up
```

By default, these services are available on host ports:

- OpenSearch: `http://localhost:9200`
- OpenSearch Dashboards: `http://localhost:5601`
- OTLP gRPC receiver: `localhost:4317`
- OTLP HTTP receiver: `http://localhost:4318`

With the `victoria` profile, these additional services are available:

- VictoriaMetrics: `http://localhost:8428`
- VictoriaLogs: `http://localhost:9428`
- VictoriaTraces: `http://localhost:10428`
- Grafana: `http://localhost:3000`

## Runtime Options

The host-side ports, data directories, and mounted config files can be changed
with environment variables. Victoria and Grafana options are used when the
`victoria` profile is enabled.

| Variable                                | Default                         | Required | Description                                                                                                                                               |
| :--                                     | :--                             | :--      | :--                                                                                                                                                       |
| `OPENSEARCH_PORT`                       | `9200`                          | no       | Host port mapped to OpenSearch port `9200`.                                                                                                               |
| `OPENSEARCH_DASHBOARDS_PORT`            | `5601`                          | no       | Host port mapped to OpenSearch Dashboards port `5601`.                                                                                                    |
| `VICTORIAMETRICS_PORT`                  | `8428`                          | no       | Host port mapped to VictoriaMetrics port `8428`.                                                                                                          |
| `VICTORIALOGS_PORT`                     | `9428`                          | no       | Host port mapped to VictoriaLogs port `9428`.                                                                                                             |
| `VICTORIATRACES_PORT`                   | `10428`                         | no       | Host port mapped to VictoriaTraces port `10428`.                                                                                                          |
| `GRAFANA_PORT`                          | `3000`                          | no       | Host port mapped to Grafana port `3000`.                                                                                                                  |
| `GRAFANA_ADMIN_PASSWORD`                | `admin`                         | no       | Grafana admin password.                                                                                                                                   |
| `OTEL_COLLECTOR_GRPC_PORT`              | `4317`                          | no       | Host port mapped to the OTLP gRPC receiver.                                                                                                               |
| `OTEL_COLLECTOR_HTTP_PORT`              | `4318`                          | no       | Host port mapped to the OTLP HTTP receiver.                                                                                                               |
| `OPENSEARCH_DATA_DIR`                   | `./opensearch-data`             | no       | Host directory bind-mounted to `/usr/share/opensearch/data`.                                                                                              |
| `VICTORIAMETRICS_DATA_DIR`              | `./victoriametrics-data`        | no       | Host directory bind-mounted to `/victoria-metrics-data`.                                                                                                  |
| `VICTORIALOGS_DATA_DIR`                 | `./victorialogs-data`           | no       | Host directory bind-mounted to `/victoria-logs-data`.                                                                                                     |
| `VICTORIATRACES_DATA_DIR`               | `./victoriatraces-data`         | no       | Host directory bind-mounted to `/victoria-traces-data`.                                                                                                   |
| `GRAFANA_DATA_DIR`                      | `./grafana-data`                | no       | Host directory bind-mounted to `/var/lib/grafana`.                                                                                                        |
| `OTEL_COLLECTOR_CONFIG_FILE`            | `./otel-collector-config.yaml`  | no       | Host path to the OpenTelemetry Collector config file.                                                                                                     |
| `OPENSEARCH_DASHBOARDS_CONFIG_FILE`     | `./opensearch_dashboards.yaml`  | no       | Host path to the OpenSearch Dashboards config file.                                                                                                       |
| `GRAFANA_PROVISIONING_DIR`              | `./grafana/provisioning`        | no       | Host path to Grafana provisioning files.                                                                                                                  |

```bash
OPENSEARCH_PORT=19200 \
OPENSEARCH_DASHBOARDS_PORT=15601 \
VICTORIAMETRICS_PORT=18428 \
VICTORIALOGS_PORT=19428 \
VICTORIATRACES_PORT=20428 \
GRAFANA_PORT=13000 \
OTEL_COLLECTOR_GRPC_PORT=14317 \
OTEL_COLLECTOR_HTTP_PORT=14318 \
OPENSEARCH_DATA_DIR=/path/to/opensearch-data \
VICTORIAMETRICS_DATA_DIR=/path/to/victoriametrics-data \
VICTORIALOGS_DATA_DIR=/path/to/victorialogs-data \
VICTORIATRACES_DATA_DIR=/path/to/victoriatraces-data \
GRAFANA_DATA_DIR=/path/to/grafana-data \
OTEL_COLLECTOR_CONFIG_FILE=/path/to/otel-collector-config.yaml \
OPENSEARCH_DASHBOARDS_CONFIG_FILE=/path/to/opensearch_dashboards.yaml \
GRAFANA_PROVISIONING_DIR=/path/to/grafana/provisioning \
docker compose -f ./otel-collector-compose/docker-compose.yaml up
```

If a data directory variable is not set, data is stored next to the copied
`docker-compose.yaml`; for example, OpenSearch uses
`./otel-collector-compose/opensearch-data`. With the `victoria` profile,
VictoriaMetrics uses `./otel-collector-compose/victoriametrics-data`,
VictoriaLogs uses `./otel-collector-compose/victorialogs-data`, VictoriaTraces
uses `./otel-collector-compose/victoriatraces-data`, and Grafana uses
`./otel-collector-compose/grafana-data`. On SELinux-enabled systems, the
Compose file applies the `:Z` label option to the mounted paths.

With rootless Podman, bind-mounted data directories can otherwise be created
with ownership that containers cannot write to. Create them first and set the
ownership from the Podman user namespace:

```bash
mkdir -p ./otel-collector-compose/opensearch-data \
         ./otel-collector-compose/victoriametrics-data \
         ./otel-collector-compose/victorialogs-data \
         ./otel-collector-compose/victoriatraces-data \
         ./otel-collector-compose/grafana-data
podman unshare chown -R 1000:1000 \
  ./otel-collector-compose/opensearch-data \
  ./otel-collector-compose/victoriametrics-data \
  ./otel-collector-compose/victorialogs-data \
  ./otel-collector-compose/victoriatraces-data \
  ./otel-collector-compose/grafana-data
```

If data directory variables are set, run the same ownership adjustment on those
directories instead.

```bash
mkdir -p "$OPENSEARCH_DATA_DIR" "$VICTORIAMETRICS_DATA_DIR" \
         "$VICTORIALOGS_DATA_DIR" "$VICTORIATRACES_DATA_DIR" \
         "$GRAFANA_DATA_DIR"
podman unshare chown -R 1000:1000 "$OPENSEARCH_DATA_DIR" \
  "$VICTORIAMETRICS_DATA_DIR" "$VICTORIALOGS_DATA_DIR" \
  "$VICTORIATRACES_DATA_DIR" "$GRAFANA_DATA_DIR"
```

If config variables are not set, the Compose file uses
`otel-collector-config.yaml`, `opensearch_dashboards.yaml`, and
`grafana/provisioning` next to the copied `docker-compose.yaml`. With the
example above, these are expected under `./otel-collector-compose/`.

To use different collector, OpenSearch Dashboards, or Grafana provisioning
files, set `OTEL_COLLECTOR_CONFIG_FILE`, `OPENSEARCH_DASHBOARDS_CONFIG_FILE`,
and `GRAFANA_PROVISIONING_DIR` when running Compose, or edit the copied
`docker-compose.yaml`.

Use `OTEL_COLLECTOR_CONFIG_FILE=./otel-collector-config-victoria.yaml` together
with `--profile victoria` to export logs and traces to VictoriaLogs and
VictoriaTraces, and metrics to VictoriaMetrics.

## Collector Pipelines

The collector accepts OTLP data on both receivers:

- gRPC: `0.0.0.0:4317`
- HTTP: `0.0.0.0:4318`

Logs are exported to OpenSearch indices named:

```text
otel-logs-%{service.name}-yyyy.MM.dd
```

Traces are exported to OpenSearch indices named:

```text
otel-traces-%{service.name}-yyyy.MM.dd
```

If `service.name` is missing, `unknown-service` is used as the fallback.

The default collector config stores logs and traces in OpenSearch. It does not
configure a metrics pipeline. Use the `victoria` profile with
`otel-collector-config-victoria.yaml` to store metrics.

With the `victoria` profile, logs are also exported to VictoriaLogs at:

```text
http://victorialogs:9428/insert/opentelemetry/v1/logs
```

Traces are also exported to VictoriaTraces at:

```text
http://victoriatraces:10428/insert/opentelemetry/v1/traces
```

Metrics are exported to VictoriaMetrics at:

```text
http://victoriametrics:8428/opentelemetry/v1/metrics
```

Grafana is provisioned with VictoriaMetrics, VictoriaLogs, and VictoriaTraces
datasources. VictoriaTraces is configured through Grafana's built-in Jaeger
datasource using:

```text
http://victoriatraces:10428/select/jaeger
```

## Stop

Stop the stack:

```bash
docker compose -f ./otel-collector-compose/docker-compose.yaml down
```

For Podman:

```bash
podman compose -f ./otel-collector-compose/docker-compose.yaml down
```

To remove data stored in the default bind-mounted directories:

```bash
rm -rf ./otel-collector-compose/opensearch-data \
       ./otel-collector-compose/victoriametrics-data \
       ./otel-collector-compose/victorialogs-data \
       ./otel-collector-compose/victoriatraces-data \
       ./otel-collector-compose/grafana-data
```

With rootless Podman, remove the directory from the Podman user namespace if the
host user cannot delete files created by the container:

```bash
podman unshare rm -rf ./otel-collector-compose/opensearch-data \
  ./otel-collector-compose/victoriametrics-data \
  ./otel-collector-compose/victorialogs-data \
  ./otel-collector-compose/victoriatraces-data \
  ./otel-collector-compose/grafana-data
```

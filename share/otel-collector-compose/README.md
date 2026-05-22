# OpenTelemetry Collector Compose Setup

## OpenTelemetry Collector with OpenSearch

This directory contains a Compose setup for receiving OpenTelemetry data with
OpenTelemetry Collector, storing logs and traces in OpenSearch, and viewing
them with OpenSearch Dashboards.

This stack is intended for local validation only. It disables OpenSearch
security and publishes service ports on the host, so do not expose it on a
public or shared network.

OpenSearch 2.12 and later, including OpenSearch 3.x, requires
`OPENSEARCH_INITIAL_ADMIN_PASSWORD` when the bundled demo security
configuration is installed. This local validation compose disables that demo
configuration installer and the Security plugin, so no OpenSearch admin
password is required for this stack.

The installed package provides `docker-compose.yaml` together with the collector,
OpenSearch Dashboards configuration file, OpenSearch Dashboards setup script,
and Grafana provisioning files.

## Components

- `otel-collector`: receives OTLP data over gRPC and HTTP.
- `opensearch`: stores logs and traces exported by the collector.
- `opensearch-dashboards`: provides the web UI for OpenSearch.
- `opensearch-dashboards-setup`: creates the initial logs and traces Data
  Views in OpenSearch Dashboards if they do not already exist.
- `victoriametrics`: stores metrics exported by the collector when the
  `victoria` profile is enabled.
- `victorialogs`: stores logs exported by the collector when the `victoria`
  profile is enabled.
- `victoriatraces`: stores traces exported by the collector when the `victoria`
  profile is enabled.
- `grafana`: provides dashboards and Explore views for VictoriaMetrics,
  VictoriaLogs, and VictoriaTraces when the `victoria` profile is enabled.

## Start

After installation, copy the installed compose setup to a working directory.
If `./otel-collector-compose` already exists, remove it first or choose a different destination.
```bash
cp -a <install-prefix>/share/otel-collector-compose ./otel-collector-compose
```

Then run Compose with the copied compose file.

For Docker:

```bash
docker compose -f ./otel-collector-compose/docker-compose.yaml up
```

For rootless Podman, OpenSearch runs as `uid=1000,gid=1000` inside the
container. The host directory bind-mounted to `/usr/share/opensearch/data`
must be readable and writable by that container uid/gid. One reliable setup is
to create the directory and set its ownership from the Podman user namespace:

```bash
mkdir -p ./otel-collector-compose/opensearch-data
podman unshare chown -R 1000:1000 ./otel-collector-compose/opensearch-data
podman unshare chmod -R u+rwX ./otel-collector-compose/opensearch-data
podman compose -f ./otel-collector-compose/docker-compose.yaml up
```

Alternatively, map the container's `1000:1000` user to the host user that
starts Podman Compose:

```bash
mkdir -p ./otel-collector-compose/opensearch-data
PODMAN_USERNS="keep-id:uid=1000,gid=1000" \
podman compose --in-pod=false -f ./otel-collector-compose/docker-compose.yaml up
```

The `PODMAN_USERNS` setting changes the user namespace mapping. It does not
change the OpenSearch image's runtime user, which remains container
`uid=1000,gid=1000`.

If a previous run failed while requesting
`OPENSEARCH_INITIAL_ADMIN_PASSWORD`, stop the stack and remove the local
OpenSearch data directory before starting again:

```bash
docker compose -f ./otel-collector-compose/docker-compose.yaml down
rm -rf ./otel-collector-compose/opensearch-data
```

For Podman:

```bash
podman compose -f ./otel-collector-compose/docker-compose.yaml down
podman unshare rm -rf ./otel-collector-compose/opensearch-data
```

To also start VictoriaMetrics, VictoriaLogs, VictoriaTraces, and Grafana, use
the `victoria` profile and the Victoria collector config:

```bash
OTEL_COLLECTOR_CONFIG_FILE=./otel-collector-config-victoria.yaml \
docker compose -f ./otel-collector-compose/docker-compose.yaml --profile victoria up
```

For rootless Podman with the `victoria` profile, the additional bind-mounted
data directories must also be writable by the users used inside their
containers. A simple local validation setup is:

```bash
mkdir -p ./otel-collector-compose/opensearch-data
podman unshare chown -R 1000:1000 ./otel-collector-compose/opensearch-data
podman unshare chmod -R u+rwX ./otel-collector-compose/opensearch-data
mkdir -p ./otel-collector-compose/victoriametrics-data \
         ./otel-collector-compose/victorialogs-data \
         ./otel-collector-compose/victoriatraces-data \
         ./otel-collector-compose/grafana-data
podman unshare chown -R 1000:1000 \
  ./otel-collector-compose/victoriametrics-data \
  ./otel-collector-compose/victorialogs-data \
  ./otel-collector-compose/victoriatraces-data \
  ./otel-collector-compose/grafana-data
podman unshare chmod -R u+rwX \
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

## OpenSearch Dashboards

Open `http://localhost:5601/app/discover` after the stack starts. The
`opensearch-dashboards-setup` service creates Data Views for `otel-logs-*`
and `otel-traces-*` when they do not already exist. It sets `otel-logs-*` as
the default Data View only when no default is already configured. Existing Data
Views and the existing default Data View are left unchanged on later runs.

Use the Data View selector in Discover to switch from logs to traces. If no
documents are shown, widen the time range and confirm that NestDAQ is exporting
OTLP logs or traces and that OpenSearch has matching `otel-logs-*` or
`otel-traces-*` indices.

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
| `OPENSEARCH_DASHBOARDS_SETUP_SCRIPT`    | `./opensearch-dashboards/setup-dashboards.js` | no | Host path to the OpenSearch Dashboards setup script. |
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
OPENSEARCH_DASHBOARDS_SETUP_SCRIPT=/path/to/setup-dashboards.js \
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
with ownership that containers cannot write to. OpenSearch writes to
`/usr/share/opensearch/data` as container `uid=1000,gid=1000`; that uid/gid
must have read, write, and directory search permissions on the host directory
as seen from the Podman user namespace. Create data directories first and set
ownership and owner permissions from that namespace:

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
podman unshare chmod -R u+rwX \
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
podman unshare chmod -R u+rwX "$OPENSEARCH_DATA_DIR" \
  "$VICTORIAMETRICS_DATA_DIR" "$VICTORIALOGS_DATA_DIR" \
  "$VICTORIATRACES_DATA_DIR" "$GRAFANA_DATA_DIR"
```

As an alternative to changing ownership, rootless Podman can map the
container's `1000:1000` user to the host user that starts the stack:

```bash
PODMAN_USERNS="keep-id:uid=1000,gid=1000" \
podman compose --in-pod=false -f ./otel-collector-compose/docker-compose.yaml up
```

Use the same environment setting with `--profile victoria` when starting the
Victoria/Grafana stack. This is Podman-specific and may depend on the compose
provider, so `podman unshare chown` is the more explicit setup for shared
instructions.

If config variables are not set, the Compose file uses
`otel-collector-config.yaml`, `opensearch_dashboards.yaml`,
`opensearch-dashboards/setup-dashboards.js`, and `grafana/provisioning` next
to the copied `docker-compose.yaml`. With the
example above, these are expected under `./otel-collector-compose/`.

To use different collector, OpenSearch Dashboards, or Grafana provisioning
files, set `OTEL_COLLECTOR_CONFIG_FILE`, `OPENSEARCH_DASHBOARDS_CONFIG_FILE`,
`OPENSEARCH_DASHBOARDS_SETUP_SCRIPT`, and `GRAFANA_PROVISIONING_DIR` when
running Compose, or edit the copied `docker-compose.yaml`.

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
OpenSearch requires lowercase index names. NestDAQ telemetry lowercases ASCII
uppercase letters in `service.name` before export; external OTLP clients should
also send lowercase `service.name` values when using this compose setup.

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

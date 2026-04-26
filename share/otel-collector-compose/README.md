# OpenTelemetry Collector with OpenSearch

This directory contains a Compose setup for receiving OpenTelemetry data with
OpenTelemetry Collector, storing logs and traces in OpenSearch, and viewing them
with OpenSearch Dashboards.

The installed package provides `docker-compose.yaml` together with the collector
and OpenSearch Dashboards configuration files.

## Components

- `otel-collector`: receives OTLP data over gRPC and HTTP.
- `opensearch`: stores logs and traces exported by the collector.
- `opensearch-dashboards`: provides the web UI for OpenSearch.

## Prerequisites

Create the external network before starting the stack:

```bash
docker network create otel-net
```

For Podman:

```bash
podman network create otel-net
```

## Start

After installation, copy the installed compose file to a working directory and
run Compose with the copied file:

```bash
mkdir -p ./otel-collector-compose
cp <install-prefix>/share/otel-collector-compose/docker-compose.yaml ./otel-collector-compose/

docker compose -f ./otel-collector-compose/docker-compose.yaml up
```

For Podman:

```bash
podman compose -f ./otel-collector-compose/docker-compose.yaml up
```

By default, the services are available on these host ports:

- OpenSearch: `http://localhost:9200`
- OpenSearch Dashboards: `http://localhost:5601`
- OTLP gRPC receiver: `localhost:4317`
- OTLP HTTP receiver: `http://localhost:4318`

## Runtime Options

The host-side ports, OpenSearch data directory, and mounted config files can be
changed with environment variables:

```bash
OPENSEARCH_PORT=19200 \
OPENSEARCH_DASHBOARDS_PORT=15601 \
OTEL_COLLECTOR_GRPC_PORT=14317 \
OTEL_COLLECTOR_HTTP_PORT=14318 \
OPENSEARCH_DATA_DIR=/path/to/opensearch-data \
OTEL_COLLECTOR_CONFIG_FILE=/path/to/otel-collector-config.yaml \
OPENSEARCH_DASHBOARDS_CONFIG_FILE=/path/to/opensearch_dashboards.yaml \
docker compose -f ./otel-collector-compose/docker-compose.yaml up
```

If `OPENSEARCH_DATA_DIR` is not set, OpenSearch data is stored in
`opensearch-data` next to the copied `docker-compose.yaml`; with the example
above, this is `./otel-collector-compose/opensearch-data`. The Compose file bind
mounts that directory to `/usr/share/opensearch/data` in the OpenSearch
container. On SELinux-enabled systems, the Compose file applies the `:Z` label
option to the mounted paths.

To use different collector or OpenSearch Dashboards config files, either set
`OTEL_COLLECTOR_CONFIG_FILE` and `OPENSEARCH_DASHBOARDS_CONFIG_FILE` when running
Compose, or edit the copied `docker-compose.yaml`.

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

## Stop

Stop the stack:

```bash
docker compose -f ./otel-collector-compose/docker-compose.yaml down
```

For Podman:

```bash
podman compose -f ./otel-collector-compose/docker-compose.yaml down
```

To remove data stored in the default bind-mounted directory:

```bash
rm -rf ./otel-collector-compose/opensearch-data
```
